/*
 * Installed-SDK C11 qualification: one real two-SG-Owner TCP handoff.
 *
 * Owner 0 accepts a detached loopback stream. A bounded MPSC inbox moves
 * the socket exactly once. Owner 1 alone creates the final CNet connection,
 * runs its callbacks, and retires the Manager record before releasing credit.
 * Each SG shard has exactly one NativeIO observe authority (host lease).
 *
 * The client first selects a stable remote endpoint ID; the server later
 * selects a local final SG Owner. Neither decision reserves capacity or
 * authorizes retry. Both policies use the installed public CNet SDK.
 *
 * Reference behavior: Salts v2.3 CNet SG handoff test; no Actor or extra
 * transport engine, no implicit retry, no private source-tree headers.
 */
#include <cnet/cnet.h>
#include <cnet/destination_policy.h>
#include <cnet/handoff.h>
#include <cnet/manager.h>
#include <cnet/owner_placement.h>
#include <cnet/sg_host.h>
#include <salts/clock.h>
#include <salts/native_io_sharded.h>
#include <salts/thread.h>
#include <salts/error_codes.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum { BOOK_OWNERS = 2, BOOK_FINAL_OWNER = 1,
       BOOK_BATCH = 16, BOOK_DEADLINE_MS = 12000, BOOK_PAYLOAD = 4 };

typedef struct book_case book_case;
typedef struct book_lane {
    book_case *test;
    size_t shard;
    native_io_sharded_host_lease lease;
    native_io_backend *backend;
    cnet_client client;
    cnet_listener listener; /* owner 0, never owner 1 */
    cnet_manager manager;   /* owner 1, never owner 0 */
    cnet_stream_endpoint local;
    cnet_connection connection;
    cnet_managed_connection managed;
    cnet_handoff_ticket taken_credit;
    const void *owner_token;
    size_t connected, terminal, bytes, sent, recycled;
    size_t accepted, published, taken, placement, placement_denied;
    size_t remote_selected, remote_denied, connect_attempts;
    uint64_t remote_endpoint_id;
    int status;
    bool sending, close_allowed, closing, finished, client_stopped;
} book_lane;

struct book_case {
    native_io_sharded *sg;
    cnet_handoff inbox;
    book_lane lanes[BOOK_OWNERS];
};

#define BOOK_CHECK(expr) do {                                           \
    if (!(expr)) {                                                     \
        fprintf(stderr, "line %d: %s\n", __LINE__, #expr);              \
        return 1;                                                      \
    }                                                                  \
} while (0)

static void book_error(book_lane *lane, int status)
{
    if (lane->status == SALTS_OK)
        lane->status = status;
}

#define BOOK_CALL(lane, expr) do {                                     \
    int book_rc_ = (expr);                                            \
    if (book_rc_ != SALTS_OK) {                                       \
        book_error((lane), book_rc_);                                  \
        return;                                                        \
    }                                                                  \
} while (0)

static native_io_backend_kind book_backend(void)
{
#if defined(_WIN32)
    return NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
    return NATIVE_IO_BACKEND_KQUEUE;
#else
    return NATIVE_IO_BACKEND_EPOLL;
#endif
}

static cnet_client_config book_client_config(void)
{
    cnet_client_config config = {0};
    config.backend = book_backend();
    config.connection_capacity = 2u;
    config.command_capacity = 8u;
    config.request_capacity = BOOK_BATCH;
    config.completion_batch_capacity = BOOK_BATCH;
    config.event_capacity = 8u;
    config.max_send_bytes = 1024u;
    config.receive_buffer_bytes = 1024u;
    config.connect_timeout_ms = BOOK_DEADLINE_MS;
    config.read_timeout_ms = BOOK_DEADLINE_MS;
    config.write_timeout_ms = BOOK_DEADLINE_MS;
    return config;
}

static bool book_on_owner(book_lane *lane)
{
    return lane->owner_token == cmeta_thread_current_token();
}

static void book_state(void *user, cnet_connection connection,
                       cnet_connection_state state, const cnet_error *error)
{
    book_lane *lane = (book_lane *)user;
    (void)connection;
    (void)error;
    if (!book_on_owner(lane)) {
        book_error(lane, SALTS_EPERM);
        return;
    }
    if (state == CNET_CONNECTION_CONNECTED)
        ++lane->connected;
    if (state == CNET_CONNECTION_CLOSED || state == CNET_CONNECTION_FAILED)
        ++lane->terminal;
    if (state == CNET_CONNECTION_FAILED)
        book_error(lane, SALTS_ECONNRESET);
}

static void book_receive(void *user, cnet_connection connection,
                         const cnet_receive_view *view)
{
    book_lane *lane = (book_lane *)user;
    const char *expected = lane->shard == BOOK_FINAL_OWNER ? "ping" : "pong";
    if (!book_on_owner(lane) || view == NULL ||
        lane->bytes > BOOK_PAYLOAD || view->size > BOOK_PAYLOAD - lane->bytes ||
        memcmp(view->data, expected + lane->bytes, view->size) != 0) {
        book_error(lane, SALTS_EPROTO);
        return;
    }
    lane->bytes += view->size;
    if (lane->bytes < BOOK_PAYLOAD) {
        int rc = cnet_receive(&lane->client, connection, 1u);
        if (rc != SALTS_OK)
            book_error(lane, rc);
    }
}

static void book_sent(void *user, cnet_connection connection, size_t size)
{
    book_lane *lane = (book_lane *)user;
    (void)connection;
    if (!book_on_owner(lane)) {
        book_error(lane, SALTS_EPERM);
        return;
    }
    lane->sent += size;
}

static void book_recycle(void *user)
{
    book_lane *lane = (book_lane *)user;
    if (!book_on_owner(lane))
        book_error(lane, SALTS_EPERM);
    ++lane->recycled;
}

static cnet_observer book_observer(book_lane *lane)
{
    cnet_observer observer = {0};
    observer.on_state = book_state;
    observer.on_receive = book_receive;
    observer.on_send = book_sent;
    observer.user = lane;
    return observer;
}

static bool book_quiescent(void *user)
{
    book_lane *lane = (book_lane *)user;
    return lane->client_stopped && lane->client.impl == NULL &&
           lane->listener.impl == NULL && lane->manager.impl == NULL;
}

static void book_initialize(native_io_sharded_context *context, void *user)
{
    book_lane *lane = (book_lane *)user;
    book_case *test = lane->test;
    cnet_client_config config = book_client_config();

    if (native_io_sharded_context_shard(context) != lane->shard) {
        book_error(lane, SALTS_EPERM);
        return;
    }
    lane->owner_token = cmeta_thread_current_token();
    BOOK_CALL(lane, native_io_sharded_context_acquire_host(
        context, book_quiescent, lane, &lane->lease, &lane->backend));
    BOOK_CALL(lane, cnet_client_init_external(&lane->client, &config, lane->backend));

    if (lane->shard == BOOK_FINAL_OWNER) {
        cnet_manager_config manager = {
            sizeof(cnet_manager_config), CNET_MANAGER_VERSION,
            &lane->client, 1u, 1u
        };
        cnet_handoff_config inbox = {
            sizeof(cnet_handoff_config), CNET_HANDOFF_VERSION, 1u, 1u
        };
        BOOK_CALL(lane, cnet_manager_init(&lane->manager, &manager));
        BOOK_CALL(lane, cnet_handoff_init(&test->inbox, &inbox));
    } else {
        cnet_stream_endpoint loopback = CNET_STREAM_ENDPOINT_INIT;
        native_io_request request = {0};
        loopback.family = CNET_DATAGRAM_ADDRESS_IPV4;
        loopback.address[0] = 127u;
        loopback.address[3] = 1u;

        BOOK_CALL(lane, cnet_listener_open(
            &lane->listener, book_backend(), CNET_DATAGRAM_ADDRESS_IPV4));
        BOOK_CALL(lane, cnet_listener_bind_open_endpoint(
            &lane->listener, &loopback));
        BOOK_CALL(lane, cnet_listener_local_endpoint(
            &lane->listener, &lane->local));
        BOOK_CALL(lane, cnet_listener_listen(&lane->listener, 4u));
        BOOK_CALL(lane, cnet_listener_attach_external(
            &lane->listener, lane->backend));
        BOOK_CALL(lane, cnet_listener_submit_external_accept(
            &lane->listener, &request));
    }
}

/* Choose the client remote identity first; only a later call can dial. */
static void book_connect(native_io_sharded_context *context, void *user)
{
    book_lane *lane = (book_lane *)user;
    cnet_observer observer = book_observer(lane);
    /* ID 101 maps to the actual listener's loopback endpoint. ID 202 is
     * an advisory alternative: the test must never dial it on rejection. */
    cnet_destination_hint hints[2] = {
        {101u, 1u, 3u, false}, {202u, 1u, 3u, true}
    };
    cnet_destination_selection policy = {0};
    cnet_destination_result selected = {0};
    if (native_io_sharded_context_shard(context) != 0u ||
        lane->shard != 0u || !book_on_owner(lane)) {
        book_error(lane, SALTS_EPERM);
        return;
    }

    policy.size = sizeof(policy);
    policy.version = CNET_DESTINATION_POLICY_VERSION;
    policy.kind = CNET_DESTINATION_EXPLICIT;
    policy.endpoints = hints;
    policy.endpoint_count = 2u;
    policy.explicit_endpoint_id = 101u;
    policy.snapshot_generation = 17u;
    policy.now_ms = 10u;
    policy.expires_at_ms = 100u;
    /* The pinned peer is ineligible while another advisory hint is eligible.
     * Reject without reroute, transport creation, or callback delivery. */
    if (cnet_destination_choose(&policy, &selected) != SALTS_ENOBUFS ||
        selected.index != SIZE_MAX || selected.endpoint_id != 0u ||
        lane->connect_attempts != 0u) {
        book_error(lane, SALTS_EPROTO);
        return;
    }
    ++lane->remote_denied;

    hints[0].eligible = true;
    BOOK_CALL(lane, cnet_destination_choose(&policy, &selected));
    if (selected.index != 0u || selected.endpoint_id != 101u ||
        selected.snapshot_generation != policy.snapshot_generation ||
        lane->connect_attempts != 0u) {
        book_error(lane, SALTS_EPROTO);
        return;
    }
    /* The application resolves stable ID 101 to the real listener address.
     * cnet_destination_choose did not dial or reserve a protocol pool slot. */
    lane->remote_endpoint_id = selected.endpoint_id;
    ++lane->remote_selected;
    ++lane->connect_attempts;
    BOOK_CALL(lane, cnet_connect_endpoint(
        &lane->client, &lane->local, NULL, &observer, &lane->connection));
}

/* Owner 0 alone performs the accept; successful publication moves the socket. */
static void book_publish(book_lane *lane)
{
    cnet_accepted_stream accepted = CNET_ACCEPTED_STREAM_INIT;
    cnet_handoff_ticket credit = {0}, denied = {0}, stale = {0};
    cnet_owner_placement_hint owners[BOOK_OWNERS] = {{0}};
    cnet_owner_placement_input placement = {0};
    size_t selected = SIZE_MAX;
    book_case *test = lane->test;
    int rc;

    owners[0].eligible = true;
    owners[BOOK_FINAL_OWNER].eligible = false;
    placement.size = sizeof(placement);
    placement.version = CNET_OWNER_PLACEMENT_VERSION;
    placement.kind = CNET_OWNER_PLACE_STRICT_KEY;
    placement.owners = owners;
    placement.owner_count = BOOK_OWNERS;
    placement.key_known = true;
    placement.key_hash = BOOK_FINAL_OWNER; /* 1 % 2 -> Owner 1. */

    /* An unavailable strict-key Owner is not silently replaced by Owner 0,
     * and refusal occurs before any accepted socket leaves the listener. */
    rc = cnet_owner_placement_choose(&placement, &selected);
    if (rc != SALTS_ENOBUFS || selected != SIZE_MAX ||
        lane->published != 0u || lane->placement != 0u) {
        book_error(lane, SALTS_EPROTO);
        return;
    }
    ++lane->placement_denied;

    owners[BOOK_FINAL_OWNER].eligible = true;
    rc = cnet_owner_placement_choose(&placement, &selected);
    if (rc != SALTS_OK || selected != BOOK_FINAL_OWNER) {
        book_error(lane, SALTS_EPROTO);
        return;
    }
    ++lane->placement;
    BOOK_CALL(lane, cnet_listener_accept_detached(&lane->listener, &accepted));
    BOOK_CALL(lane, cnet_handoff_reserve(&test->inbox, &credit));

    /* Placement is advisory; the second real credit must be denied. */
    if (cnet_handoff_reserve(&test->inbox, &denied) != SALTS_ENOBUFS ||
        denied.slot != 0u) {
        book_error(lane, SALTS_EPROTO);
        return;
    }
    stale = credit;
    ++stale.generation;
    if (cnet_handoff_publish(&test->inbox, stale, &accepted) != SALTS_ENOENT ||
        !accepted.internal_active) {
        book_error(lane, SALTS_EPROTO);
        return;
    }
    BOOK_CALL(lane, cnet_handoff_publish(&test->inbox, credit, &accepted));
    if (accepted.internal_active) {
        book_error(lane, SALTS_EPROTO);
        return;
    }
    ++lane->published;
    /* The host explicitly schedules the final owner's next progress turn. */
}

/* Owner 1 takes the socket and alone creates final transport/callback state. */
static void book_adopt(book_lane *lane)
{
    book_case *test = lane->test;
    cnet_accepted_stream accepted = CNET_ACCEPTED_STREAM_INIT;
    cnet_handoff_ticket credit = {0};
    cnet_manager_attachment attachment = {0};
    int rc = cnet_handoff_take(&test->inbox, &credit, &accepted);
    if (rc == SALTS_ENOENT)
        return;
    if (rc != SALTS_OK) {
        book_error(lane, rc);
        return;
    }
    ++lane->taken;
    attachment.observer = book_observer(lane);
    attachment.on_recycle = book_recycle;
    rc = cnet_manager_reserve(&lane->manager, &attachment, &lane->managed);
    if (rc != SALTS_OK) {
        (void)cnet_accepted_stream_close(&accepted);
        (void)cnet_handoff_release(&test->inbox, credit);
        book_error(lane, rc);
        return;
    }
    rc = cnet_manager_adopt(&lane->manager, lane->managed, &accepted,
                            NULL, &lane->connection);
    if (rc != SALTS_OK || accepted.internal_active ||
        lane->connection.slot == 0u) {
        book_error(lane, rc != SALTS_OK ? rc : SALTS_EPROTO);
        return;
    }
    lane->taken_credit = credit;
}

static void book_send(book_lane *lane, const char *payload)
{
    mem_buffer_t *buffer = mem_get_buffer(mem_global(), BOOK_PAYLOAD);
    int rc;
    if (buffer == NULL) {
        book_error(lane, SALTS_ENOMEM);
        return;
    }
    memcpy(mem_buffer_data(buffer), payload, BOOK_PAYLOAD);
    mem_set_used(buffer, BOOK_PAYLOAD);
    rc = cnet_send_buffer(&lane->client, lane->connection, buffer);
    mem_buffer_release(buffer);
    if (rc != SALTS_OK)
        book_error(lane, rc);
}

static void book_advance(native_io_sharded_context *context, void *user)
{
    book_lane *lane = (book_lane *)user;
    book_case *test = lane->test;
    native_io_sharded_completion completions[BOOK_BATCH];
    cnet_client *clients[1] = {&lane->client};
    cnet_sg_host_routes routes = {
        sizeof(cnet_sg_host_routes), CNET_SG_HOST_ROUTING_VERSION,
        lane->shard == 0u ? &lane->listener : NULL, clients, 1u
    };
    size_t count = 0u, events = 0u, accepts = 0u, settled = 0u;
    int rc;

    if (lane->finished || lane->status != SALTS_OK)
        return;
    if (native_io_sharded_context_shard(context) != lane->shard ||
        !book_on_owner(lane)) {
        book_error(lane, SALTS_EPERM);
        return;
    }
    if (lane->shard == BOOK_FINAL_OWNER && lane->taken == 0u) {
        book_adopt(lane);
        if (lane->status != SALTS_OK)
            return;
    }

    BOOK_CALL(lane, cnet_client_advance_external(&lane->client, &events));
    rc = native_io_sharded_context_observe_host(
        context, lane->lease, completions, BOOK_BATCH, 0u, &count);
    if (rc != SALTS_OK && rc != SALTS_ETIMEDOUT) {
        book_error(lane, rc);
        return;
    }
    BOOK_CALL(lane, cnet_sg_host_route_batch(
        completions, count, &routes, &accepts, &settled));
    if (lane->shard == 0u && accepts != 0u) {
        lane->accepted += accepts;
        if (accepts != 1u || lane->published != 0u) {
            book_error(lane, SALTS_EPROTO);
            return;
        }
        book_publish(lane);
        if (lane->status != SALTS_OK)
            return;
    }
    BOOK_CALL(lane, cnet_client_advance_external(&lane->client, &events));
    if (lane->shard == BOOK_FINAL_OWNER) {
        size_t work = 0u;
        BOOK_CALL(lane, cnet_manager_advance(&lane->manager, 1u, &work));
    }

    if (lane->connected && !lane->sending) {
        lane->sending = true;
        BOOK_CALL(lane, cnet_receive(&lane->client, lane->connection, 1u));
        book_send(lane, lane->shard == 0u ? "ping" : "pong");
        if (lane->status != SALTS_OK)
            return;
    }
    if (lane->close_allowed && lane->sending &&
        lane->bytes == BOOK_PAYLOAD && lane->sent == BOOK_PAYLOAD &&
        !lane->closing) {
        lane->closing = true;
        BOOK_CALL(lane, cnet_close(&lane->client, lane->connection));
    }

    if (lane->closing && lane->terminal) {
        if (lane->shard == BOOK_FINAL_OWNER) {
            cnet_manager_snapshot snapshot = {0};
            if (!lane->recycled)
                return;
            BOOK_CALL(lane, cnet_manager_get_snapshot(&lane->manager, &snapshot));
            if (!snapshot.drained || snapshot.bound || snapshot.retired ||
                snapshot.reserved) {
                book_error(lane, SALTS_EPROTO);
                return;
            }
            BOOK_CALL(lane, cnet_handoff_release(&test->inbox, lane->taken_credit));
            if (cnet_handoff_release(&test->inbox, lane->taken_credit) !=
                SALTS_ENOENT) {
                book_error(lane, SALTS_EPROTO);
                return;
            }
            BOOK_CALL(lane, cnet_manager_destroy(&lane->manager));
        } else {
            BOOK_CALL(lane, cnet_listener_close(&lane->listener));
            BOOK_CALL(lane, cnet_listener_destroy(&lane->listener));
        }
        BOOK_CALL(lane, cnet_client_stop_external(&lane->client));
        BOOK_CALL(lane, cnet_client_destroy(&lane->client));
        lane->client_stopped = true;
        BOOK_CALL(lane, native_io_sharded_context_release_host(
            context, lane->lease));
        lane->finished = true;
    }
}

int main(void)
{
    book_case test = {0};
    native_io_sharded_task setup[BOOK_OWNERS] = {{0}};
    native_io_sharded_task progress[BOOK_OWNERS] = {{0}};
    native_io_sharded_task connect_task = {0};
    const native_io_sharded_config config = {
        BOOK_OWNERS, 8u, {book_backend(), 24u, 48u, BOOK_BATCH}
    };
    const uint64_t deadline = cmeta_monotonic_ms() + BOOK_DEADLINE_MS;
    bool finished = false;

    BOOK_CHECK(native_io_sharded_create(&config, &test.sg) == SALTS_OK);
    BOOK_CHECK(test.sg != NULL);
    for (size_t i = 0u; i < BOOK_OWNERS; ++i) {
        book_lane *lane = &test.lanes[i];
        lane->test = &test;
        lane->shard = i;
        setup[i] = (native_io_sharded_task){book_initialize, NULL, NULL, lane};
        progress[i] = (native_io_sharded_task){book_advance, NULL, NULL, lane};
        BOOK_CHECK(native_io_sharded_submit_to(test.sg, i, &setup[i]) == SALTS_OK);
    }
    BOOK_CHECK(native_io_sharded_wait(test.sg) == SALTS_OK);
    for (size_t i = 0u; i < BOOK_OWNERS; ++i) {
        BOOK_CHECK(test.lanes[i].status == SALTS_OK);
        BOOK_CHECK(test.lanes[i].lease.owner_shard == (uint32_t)i);
        BOOK_CHECK(test.lanes[i].backend != NULL);
    }
    BOOK_CHECK(test.lanes[0].backend != test.lanes[1].backend);
    BOOK_CHECK(test.inbox.impl != NULL);

    connect_task = (native_io_sharded_task){
        book_connect, NULL, NULL, &test.lanes[0]
    };
    BOOK_CHECK(native_io_sharded_submit_to(test.sg, 0u, &connect_task) ==
               SALTS_OK);
    BOOK_CHECK(native_io_sharded_wait(test.sg) == SALTS_OK);
    BOOK_CHECK(test.lanes[0].status == SALTS_OK);

    while (!finished && cmeta_monotonic_ms() < deadline) {
        finished = true;
        for (size_t i = 0u; i < BOOK_OWNERS; ++i) {
            book_lane *lane = &test.lanes[i];
            BOOK_CHECK(lane->status == SALTS_OK);
            if (lane->finished)
                continue;
            finished = false;
            BOOK_CHECK(native_io_sharded_submit_to(
                test.sg, i, &progress[i]) == SALTS_OK);
        }
        if (finished)
            break;
        BOOK_CHECK(native_io_sharded_wait(test.sg) == SALTS_OK);
        /* A send callback is not proof of the other app's receive. */
        if (test.lanes[0].bytes == BOOK_PAYLOAD &&
            test.lanes[1].bytes == BOOK_PAYLOAD &&
            test.lanes[0].sent == BOOK_PAYLOAD &&
            test.lanes[1].sent == BOOK_PAYLOAD) {
            test.lanes[0].close_allowed = true;
            test.lanes[1].close_allowed = true;
        }
        cmeta_sleep_ms(1u);
    }

    BOOK_CHECK(finished);
    for (size_t i = 0u; i < BOOK_OWNERS; ++i) {
        const book_lane *lane = &test.lanes[i];
        BOOK_CHECK(lane->status == SALTS_OK);
        BOOK_CHECK(lane->finished && lane->connected == 1u &&
                   lane->terminal == 1u);
        BOOK_CHECK(lane->bytes == BOOK_PAYLOAD && lane->sent == BOOK_PAYLOAD);
        BOOK_CHECK(lane->client.impl == NULL);
    }
    BOOK_CHECK(test.lanes[0].remote_denied == 1u);
    BOOK_CHECK(test.lanes[0].remote_selected == 1u);
    BOOK_CHECK(test.lanes[0].remote_endpoint_id == 101u);
    BOOK_CHECK(test.lanes[0].connect_attempts == 1u);
    BOOK_CHECK(test.lanes[0].accepted == 1u);
    BOOK_CHECK(test.lanes[0].placement_denied == 1u);
    BOOK_CHECK(test.lanes[0].placement == 1u);
    BOOK_CHECK(test.lanes[0].published == 1u);
    BOOK_CHECK(test.lanes[1].taken == 1u);
    BOOK_CHECK(test.lanes[1].recycled == 1u);

    {
        cnet_handoff_snapshot snapshot = {0};
        BOOK_CHECK(cnet_handoff_get_snapshot(&test.inbox, &snapshot) == SALTS_OK);
        BOOK_CHECK(snapshot.drained && !snapshot.reserved &&
                   !snapshot.queued && !snapshot.taken);
    }
    /* Final SG barrier establishes producer quiescence before teardown. */
    BOOK_CHECK(cnet_handoff_seal(&test.inbox) == SALTS_OK);
    BOOK_CHECK(cnet_handoff_destroy(&test.inbox) == SALTS_OK);
    BOOK_CHECK(native_io_sharded_shutdown(test.sg) == SALTS_OK);
    BOOK_CHECK(native_io_sharded_destroy(test.sg) == SALTS_OK);
    return 0;
}
