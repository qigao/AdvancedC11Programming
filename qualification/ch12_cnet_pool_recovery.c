/*
 * Installed Salts CNet C11: a real client connection becomes pool READY only
 * after an application protocol byte exchange, not merely TCP CONNECTED.
 * Manager owns transport identity; Pool owns physical/lease admission; the
 * protocol owns its two actual reserved operation slots. All remain on one
 * CNet Owner, with no Actor, second Reactor, background dial or replay.
 */
#include <cnet/cnet.h>
#include <cnet/client_pool.h>
#include <cnet/manager.h>
#include <cnet/recovery_policy.h>
#include <salts/clock.h>
#include <salts/error_codes.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum { BOOK_READY_BYTES = 4, BOOK_WAIT_MS = 6000 };

typedef struct book_session {
    cnet_client client;
    cnet_client peer;
    cnet_listener listener;
    cnet_manager manager;
    cnet_client_pool pool;
    cnet_reconnect_state recovery;
    cnet_reconnect_ticket attempt;
    cnet_pool_connection physical;
    cnet_managed_connection managed;
    cnet_connection outgoing;
    cnet_connection incoming;
    cnet_pool_key key;
    cnet_pool_lease leases[2];

    size_t client_connected, peer_connected, client_terminal;
    size_t ready_received, ready_sent, recycled;
    size_t reserved_slots, released_slots;
    bool occupied[2];
    bool ready_armed;
    int callback_failure;
} book_session;

#define BOOK_CHECK(expr) do {                                          \
    if (!(expr)) {                                                     \
        fprintf(stderr, "book client pool line %d: %s\n", __LINE__, #expr); \
        return 1;                                                      \
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
    cnet_client_config c = {0};
    c.backend = book_backend();
    c.connection_capacity = 2u;
    c.command_capacity = 8u;
    c.request_capacity = 16u;
    c.completion_batch_capacity = 8u;
    c.event_capacity = 16u;
    c.max_send_bytes = 1024u;
    c.receive_buffer_bytes = 1024u;
    c.connect_timeout_ms = BOOK_WAIT_MS;
    c.read_timeout_ms = BOOK_WAIT_MS;
    c.write_timeout_ms = BOOK_WAIT_MS;
    return c;
}

static void book_client_state(void *user, cnet_connection connection,
                              cnet_connection_state state, const cnet_error *error)
{
    book_session *s = (book_session *)user;
    (void)connection;
    (void)error;
    if (state == CNET_CONNECTION_CONNECTED)
        ++s->client_connected;
    if (state == CNET_CONNECTION_CLOSED || state == CNET_CONNECTION_FAILED)
        ++s->client_terminal;
    if (state == CNET_CONNECTION_FAILED)
        s->callback_failure = SALTS_ECONNRESET;
}

static void book_peer_state(void *user, cnet_connection connection,
                            cnet_connection_state state, const cnet_error *error)
{
    book_session *s = (book_session *)user;
    (void)connection;
    (void)error;
    if (state == CNET_CONNECTION_CONNECTED)
        ++s->peer_connected;
    if (state == CNET_CONNECTION_FAILED)
        s->callback_failure = SALTS_ECONNRESET;
}

static void book_client_receive(void *user, cnet_connection connection,
                                const cnet_receive_view *view)
{
    static const char ready[] = "RDY!";
    book_session *s = (book_session *)user;
    if (view == NULL || s->ready_received > BOOK_READY_BYTES ||
        view->size > BOOK_READY_BYTES - s->ready_received ||
        memcmp(view->data, ready + s->ready_received, view->size) != 0) {
        s->callback_failure = SALTS_EPROTO;
        return;
    }
    s->ready_received += view->size;
    if (s->ready_received < BOOK_READY_BYTES) {
        int rc = cnet_receive(&s->client, connection, 1u);
        if (rc != SALTS_OK)
            s->callback_failure = rc;
    }
}

static void book_peer_sent(void *user, cnet_connection connection, size_t bytes)
{
    book_session *s = (book_session *)user;
    (void)connection;
    s->ready_sent += bytes;
}

static void book_recycle(void *user)
{
    book_session *s = (book_session *)user;
    ++s->recycled;
}

/* These are actual application-owned operation slots, not a can_reuse hint.
 * The callback must never reenter Pool and must outlive every accepted lease. */
static int book_reserve_slot(void *user, cnet_managed_connection managed,
                             uint64_t *out_token)
{
    book_session *s = (book_session *)user;
    if (out_token == NULL || managed.manager != s->managed.manager ||
        managed.incarnation != s->managed.incarnation ||
        managed.generation != s->managed.generation ||
        managed.slot != s->managed.slot ||
        s->ready_received != BOOK_READY_BYTES)
        return SALTS_EINVAL;
    for (size_t i = 0; i < 2u; ++i) {
        if (!s->occupied[i]) {
            s->occupied[i] = true;
            ++s->reserved_slots;
            *out_token = (uint64_t)(i + 1u);
            return SALTS_OK;
        }
    }
    return SALTS_ENOBUFS;
}

static void book_release_slot(void *user, uint64_t token)
{
    book_session *s = (book_session *)user;
    if (token == 0u || token > 2u || !s->occupied[(size_t)token - 1u]) {
        s->callback_failure = SALTS_EPROTO;
        return;
    }
    s->occupied[(size_t)token - 1u] = false;
    ++s->released_slots;
}

static int book_progress(book_session *s)
{
    size_t events = 0u, work = 0u;
    int ready = 0, rc;
    BOOK_CHECK(cnet_client_poll(&s->client, 0u, &events) == SALTS_OK);
    BOOK_CHECK(cnet_client_poll(&s->peer, 0u, &events) == SALTS_OK);
    BOOK_CHECK(cnet_manager_advance(&s->manager, 1u, &work) == SALTS_OK);

    if (s->incoming.slot == 0u) {
        rc = cnet_listener_wait(&s->listener, 0u, &ready);
        BOOK_CHECK(rc == SALTS_OK || rc == SALTS_ETIMEDOUT);
        if (ready) {
            cnet_accepted_stream stream = CNET_ACCEPTED_STREAM_INIT;
            cnet_observer observer = {0};
            observer.on_state = book_peer_state;
            observer.on_send = book_peer_sent;
            observer.user = s;
            BOOK_CHECK(cnet_listener_accept_detached(&s->listener, &stream) == SALTS_OK);
            BOOK_CHECK(cnet_client_adopt_accepted(
                &s->peer, &stream, &observer, &s->incoming) == SALTS_OK);
        }
    }
    BOOK_CHECK(s->callback_failure == 0);
    return 0;
}

static int book_send_ready(book_session *s)
{
    static const char ready[] = "RDY!";
    mem_buffer_t *buffer;
    int rc;
    BOOK_CHECK(s->client_connected == 1u && s->peer_connected == 1u);
    BOOK_CHECK(s->ready_received == 0u && !s->ready_armed);
    BOOK_CHECK(cnet_receive(&s->client, s->outgoing, 1u) == SALTS_OK);
    s->ready_armed = true;
    buffer = mem_get_buffer(mem_global(), BOOK_READY_BYTES);
    BOOK_CHECK(buffer != NULL);
    memcpy(mem_buffer_data(buffer), ready, BOOK_READY_BYTES);
    mem_set_used(buffer, BOOK_READY_BYTES);
    rc = cnet_send_buffer(&s->peer, s->incoming, buffer);
    mem_buffer_release(buffer);
    BOOK_CHECK(rc == SALTS_OK);
    return 0;
}

/* Application-owned admission: no transport callback automatically grants
 * READY. This tiny sample's protocol evidence is one real "RDY!" frame,
 * not a TLS authentication, negotiated multiplexed peer, or execution ACK. */
static int book_publish_protocol_ready(book_session *s)
{
    int rc;
    if (s->client_connected != 1u ||
        s->ready_received != BOOK_READY_BYTES ||
        s->ready_sent != BOOK_READY_BYTES)
        return SALTS_EBUSY;
    rc = cnet_reconnect_protocol_ready(&s->recovery, s->attempt, 102u);
    if (rc != SALTS_OK)
        return rc;
    return cnet_pool_bind_ready(&s->pool, s->physical, s->managed, 2u);
}

static int book_retry_contract(void)
{
    cnet_retry_input request = {0};
    cnet_retry_result decision = {0};
    request.size = sizeof(request);
    request.version = CNET_RECOVERY_POLICY_VERSION;
    request.attempts_used = 1u;
    request.max_attempts = 3u;
    request.now_ms = 200u;
    request.deadline_ms = 1000u;
    request.request_body_bytes = 4u;
    request.remaining_retry_byte_budget = 4u;

    /* A restored transport never authorizes application DATA replay. */
    BOOK_CHECK(cnet_retry_evaluate(&request, &decision) == SALTS_OK);
    BOOK_CHECK(!decision.allowed && decision.reason == CNET_RETRY_DISABLED);
    request.explicitly_enabled = true;
    request.owned_replayable_body = true;
    BOOK_CHECK(cnet_retry_evaluate(&request, &decision) == SALTS_OK);
    BOOK_CHECK(!decision.allowed && decision.reason == CNET_RETRY_UNAUTHORIZED);
    request.protocol_proves_not_executed = true;
    BOOK_CHECK(cnet_retry_evaluate(&request, &decision) == SALTS_OK);
    BOOK_CHECK(decision.allowed && decision.reason == CNET_RETRY_ALLOWED);
    request.one_attempt_contract = true;
    BOOK_CHECK(cnet_retry_evaluate(&request, &decision) == SALTS_OK);
    BOOK_CHECK(!decision.allowed && decision.reason == CNET_RETRY_ONE_ATTEMPT);
    request.one_attempt_contract = false;
    request.owned_replayable_body = false;
    BOOK_CHECK(cnet_retry_evaluate(&request, &decision) == SALTS_OK);
    BOOK_CHECK(!decision.allowed && decision.reason == CNET_RETRY_UNREPLAYABLE);
    request.owned_replayable_body = true;
    request.security_failure = true;
    BOOK_CHECK(cnet_retry_evaluate(&request, &decision) == SALTS_OK);
    BOOK_CHECK(!decision.allowed && decision.reason == CNET_RETRY_SECURITY);
    request.security_failure = false;
    request.backoff_not_before_ms = 250u;
    BOOK_CHECK(cnet_retry_evaluate(&request, &decision) == SALTS_OK);
    BOOK_CHECK(!decision.allowed && decision.reason == CNET_RETRY_WAIT_BACKOFF);
    return 0;
}

int main(void)
{
    book_session s = {0};
    const cnet_client_config config = book_client_config();
    const cnet_listener_config listener_config = {
        book_backend(), "127.0.0.1", 0u, 4u
    };
    cnet_manager_config manager_config = {0};
    cnet_pool_config pool_config = {0};
    cnet_reconnect_config recovery_config = {0};
    cnet_manager_attachment attachment = {0};
    cnet_connect_options connect_options = {0};
    cnet_reconnect_snapshot recovery = {0};
    cnet_manager_snapshot manager_state = {0};
    cnet_pool_snapshot pool_state = {0};
    cnet_pool_protocol_ops protocol = {book_reserve_slot, book_release_slot, &s};
    cnet_pool_lease denied = {0};
    cnet_managed_connection leased = {0};
    cnet_pool_key incompatible = {0};
    char uri[100];
    uint16_t port = 0u;
    uint64_t wait_ms = 0u;
    uint64_t until;
    int written;

    BOOK_CHECK(cnet_client_init(&s.client, &config) == SALTS_OK);
    BOOK_CHECK(cnet_client_init(&s.peer, &config) == SALTS_OK);
    BOOK_CHECK(cnet_listener_init(&s.listener, &listener_config) == SALTS_OK);
    BOOK_CHECK(cnet_listener_port(&s.listener, &port) == SALTS_OK && port != 0u);
    written = snprintf(uri, sizeof(uri), "tcp://127.0.0.1:%u", (unsigned)port);
    BOOK_CHECK(written > 0 && (size_t)written < sizeof(uri));

    manager_config.size = sizeof(manager_config);
    manager_config.version = CNET_MANAGER_VERSION;
    manager_config.client = &s.client;
    manager_config.record_capacity = 1u;
    manager_config.connection_capacity = 1u;
    BOOK_CHECK(cnet_manager_init(&s.manager, &manager_config) == SALTS_OK);

    pool_config.size = sizeof(pool_config);
    pool_config.version = CNET_CLIENT_POOL_VERSION;
    pool_config.manager = &s.manager;
    pool_config.owner_id = 7u;
    pool_config.max_connections = 1u;
    pool_config.max_connecting = 1u;
    pool_config.max_leases = 3u;
    BOOK_CHECK(cnet_pool_init(&s.pool, &pool_config) == SALTS_OK);

    s.key = (cnet_pool_key){0};
    s.key.size = sizeof(s.key);
    s.key.version = CNET_CLIENT_POOL_VERSION;
    s.key.runtime_id = 1u;
    s.key.owner_id = 7u;
    s.key.endpoint_id = 101u;
    s.key.peer_generation = 4u;
    s.key.authority_id = 55u;
    s.key.transport_id = 1u;
    s.key.tls_trust_id = 77u;
    s.key.protocol_id = 20u;
    s.key.session_id = 8u;
    BOOK_CHECK(cnet_pool_reserve_connecting(&s.pool, &s.key, &s.physical) == SALTS_OK);
    {
        cnet_pool_connection competing = {0};
        BOOK_CHECK(cnet_pool_reserve_connecting(
            &s.pool, &s.key, &competing) == SALTS_ENOBUFS);
        BOOK_CHECK(competing.slot == 0u);
    }
    BOOK_CHECK(cnet_pool_try_acquire(
        &s.pool, &s.key, &protocol, &denied, &leased) == SALTS_ENOBUFS);
    BOOK_CHECK(denied.slot == 0u);

    recovery_config.size = sizeof(recovery_config);
    recovery_config.version = CNET_RECOVERY_POLICY_VERSION;
    recovery_config.max_attempts = 2u;
    recovery_config.deadline_ms = 1000u;
    recovery_config.initial_backoff_ms = 40u;
    recovery_config.maximum_backoff_ms = 160u;
    recovery_config.jitter_seed = 19u;
    BOOK_CHECK(cnet_reconnect_init(&s.recovery, &recovery_config) == SALTS_OK);
    BOOK_CHECK(cnet_reconnect_begin(
        &s.recovery, 100u, &s.attempt, &wait_ms) == SALTS_OK);
    BOOK_CHECK(wait_ms == 0u);

    attachment.observer.on_state = book_client_state;
    attachment.observer.on_receive = book_client_receive;
    attachment.observer.user = &s;
    attachment.on_recycle = book_recycle;
    BOOK_CHECK(cnet_manager_reserve(&s.manager, &attachment, &s.managed) == SALTS_OK);
    connect_options.uri = uri;
    BOOK_CHECK(cnet_manager_connect(
        &s.manager, s.managed, &connect_options, &s.outgoing) == SALTS_OK);
    BOOK_CHECK(s.outgoing.slot != 0u);
    BOOK_CHECK(cnet_pool_try_acquire(
        &s.pool, &s.key, &protocol, &denied, &leased) == SALTS_ENOBUFS);

    until = cmeta_monotonic_ms() + BOOK_WAIT_MS;
    while ((s.client_connected == 0u || s.peer_connected == 0u) &&
           cmeta_monotonic_ms() < until) {
        BOOK_CHECK(book_progress(&s) == 0);
        cmeta_sleep_ms(1u);
    }
    BOOK_CHECK(s.client_connected == 1u && s.peer_connected == 1u);
    BOOK_CHECK(cnet_reconnect_connected(&s.recovery, s.attempt, 101u) == SALTS_OK);
    BOOK_CHECK(cnet_reconnect_get_snapshot(&s.recovery, &recovery) == SALTS_OK);
    BOOK_CHECK(recovery.awaiting_protocol && !recovery.protocol_ready);
    BOOK_CHECK(book_publish_protocol_ready(&s) == SALTS_EBUSY);
    BOOK_CHECK(cnet_pool_try_acquire(
        &s.pool, &s.key, &protocol, &denied, &leased) == SALTS_ENOBUFS);

    BOOK_CHECK(book_send_ready(&s) == 0);
    until = cmeta_monotonic_ms() + BOOK_WAIT_MS;
    while ((s.ready_received != BOOK_READY_BYTES ||
            s.ready_sent != BOOK_READY_BYTES) &&
           cmeta_monotonic_ms() < until) {
        BOOK_CHECK(book_progress(&s) == 0);
        cmeta_sleep_ms(1u);
    }
    BOOK_CHECK(s.ready_received == BOOK_READY_BYTES);
    BOOK_CHECK(s.ready_sent == BOOK_READY_BYTES);
    BOOK_CHECK(book_publish_protocol_ready(&s) == SALTS_OK);
    BOOK_CHECK(cnet_reconnect_get_snapshot(&s.recovery, &recovery) == SALTS_OK);
    BOOK_CHECK(recovery.protocol_ready && !recovery.awaiting_protocol);
    BOOK_CHECK(cnet_pool_get_snapshot(&s.pool, &pool_state) == SALTS_OK);
    BOOK_CHECK(pool_state.ready == 1u && pool_state.active_leases == 0u);

    incompatible = s.key;
    incompatible.tls_trust_id++;
    BOOK_CHECK(cnet_pool_try_acquire(
        &s.pool, &incompatible, &protocol, &denied, &leased) == SALTS_ENOBUFS);
    BOOK_CHECK(cnet_pool_try_acquire(
        &s.pool, &s.key, NULL, &denied, &leased) == SALTS_ENOTSUP);
    BOOK_CHECK(cnet_pool_try_acquire(
        &s.pool, &s.key, &protocol, &s.leases[0], &leased) == SALTS_OK);
    BOOK_CHECK(leased.slot == s.managed.slot &&
               leased.generation == s.managed.generation);
    BOOK_CHECK(cnet_pool_try_acquire(
        &s.pool, &s.key, &protocol, &s.leases[1], &leased) == SALTS_OK);
    BOOK_CHECK(s.reserved_slots == 2u && s.occupied[0] && s.occupied[1]);
    BOOK_CHECK(cnet_pool_try_acquire(
        &s.pool, &s.key, &protocol, &denied, &leased) == SALTS_ENOBUFS);
    BOOK_CHECK(denied.slot == 0u);

    /* Stop admission first. The Manager still owns a real BOUND transport. */
    BOOK_CHECK(cnet_pool_begin_drain(&s.pool, s.physical) == SALTS_OK);
    BOOK_CHECK(cnet_pool_try_acquire(
        &s.pool, &s.key, &protocol, &denied, &leased) == SALTS_ENOBUFS);
    BOOK_CHECK(cnet_pool_terminal(&s.pool, s.physical) == SALTS_EBUSY);
    BOOK_CHECK(cnet_pool_destroy(&s.pool) == SALTS_EBUSY);
    BOOK_CHECK(book_retry_contract() == 0);
    BOOK_CHECK(cnet_close(&s.client, s.outgoing) == SALTS_OK);

    until = cmeta_monotonic_ms() + BOOK_WAIT_MS;
    do {
        BOOK_CHECK(book_progress(&s) == 0);
        BOOK_CHECK(cnet_manager_get_snapshot(
            &s.manager, &manager_state) == SALTS_OK);
        if (manager_state.drained && s.client_terminal == 1u && s.recycled == 1u)
            break;
        cmeta_sleep_ms(1u);
    } while (cmeta_monotonic_ms() < until);
    BOOK_CHECK(manager_state.drained && s.client_terminal == 1u && s.recycled == 1u);
    BOOK_CHECK(cnet_pool_terminal(&s.pool, s.physical) == SALTS_OK);
    BOOK_CHECK(cnet_pool_get_snapshot(&s.pool, &pool_state) == SALTS_OK);
    BOOK_CHECK(pool_state.terminal_waiting_for_leases == 1u);
    BOOK_CHECK(pool_state.active_leases == 2u && !pool_state.drained);
    BOOK_CHECK(cnet_pool_destroy(&s.pool) == SALTS_EBUSY);
    BOOK_CHECK(cnet_pool_release(&s.pool, s.leases[0]) == SALTS_OK);
    BOOK_CHECK(cnet_pool_release(&s.pool, s.leases[0]) == SALTS_ENOENT);
    BOOK_CHECK(cnet_pool_release(&s.pool, s.leases[1]) == SALTS_OK);
    BOOK_CHECK(s.callback_failure == 0);
    BOOK_CHECK(s.released_slots == 2u && !s.occupied[0] && !s.occupied[1]);
    BOOK_CHECK(cnet_pool_get_snapshot(&s.pool, &pool_state) == SALTS_OK);
    BOOK_CHECK(pool_state.drained && pool_state.active_leases == 0u);
    BOOK_CHECK(cnet_pool_destroy(&s.pool) == SALTS_OK);

    /* A transport loss starts a bounded recovery episode, not DATA replay.
     * This gate deliberately does not create a replacement connection. */
    BOOK_CHECK(cnet_reconnect_lost(&s.recovery, s.attempt,
        CNET_RECONNECT_TRANSIENT, 120u, 1000u) == SALTS_OK);
    BOOK_CHECK(cnet_reconnect_get_snapshot(&s.recovery, &recovery) == SALTS_OK);
    BOOK_CHECK(!recovery.protocol_ready &&
               recovery.next_attempt_ms >= 140u &&
               recovery.next_attempt_ms <= 160u);
    {
        cnet_reconnect_ticket next = {0};
        BOOK_CHECK(cnet_reconnect_begin(
            &s.recovery, 120u, &next, &wait_ms) == SALTS_EBUSY);
        BOOK_CHECK(wait_ms == recovery.next_attempt_ms - 120u);
        BOOK_CHECK(next.generation == 0u);
    }
    BOOK_CHECK(cnet_reconnect_seal(&s.recovery) == SALTS_OK);
    BOOK_CHECK(cnet_manager_destroy(&s.manager) == SALTS_OK);
    BOOK_CHECK(cnet_client_stop(&s.client, BOOK_WAIT_MS) == SALTS_OK);
    BOOK_CHECK(cnet_client_stop(&s.peer, BOOK_WAIT_MS) == SALTS_OK);
    BOOK_CHECK(cnet_client_destroy(&s.client) == SALTS_OK);
    BOOK_CHECK(cnet_client_destroy(&s.peer) == SALTS_OK);
    BOOK_CHECK(cnet_listener_close(&s.listener) == SALTS_OK);
    BOOK_CHECK(cnet_listener_destroy(&s.listener) == SALTS_OK);
    return 0;
}
