/*
 * Application ACE Service Configurator: two independent, exact CMeta typed
 * Strategies select *real* CNet 2.3 server and client policy candidates.
 *
 * SERVER: final local SG Owner placement, strict-key maps key % owner_count.
 * CLIENT: remote endpoint selection, strict-key rendezvous hashes stable IDs.
 *
 * Choices are advisory. No real handoff reservation, dial, protocol-READY pool,
 * hidden retry, Actor, or second I/O loop is implied by a successful choice.
 * Caller-owned capacity/connection admission must happen separately.
 */
#include <cmeta/interface.h>
#include <cnet/owner_placement.h>
#include <cnet/destination_policy.h>
#include <salts/thread.h>
#include <salts/error_codes.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BOOK_CHECK(expr) do {                                         \
    if (!(expr)) {                                                    \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);    \
        return 1;                                                     \
    }                                                                 \
} while (0)

/* FR2 fixes the actual pointer's ABI carrier and OUT borrow contract. */
#define BOOK_SERVER_METHODS(X, I)                                     \
    X(I, FR2, int, select, stateful,                                  \
      &cmeta_type_int, CMETA_ABI_SCALAR, CMETA_RESULT_VALUE,          \
      (int, ticket, CMETA_PARAM_IN, &cmeta_type_int, CMETA_ABI_SCALAR), \
      (int *, out, CMETA_PARAM_OUT | CMETA_PARAM_BORROWED,            \
       &cmeta_type_int_ptr, CMETA_ABI_OBJECT_POINTER))

#define BOOK_CLIENT_METHODS(X, I)                                     \
    X(I, FR2, int, select, stateful,                                  \
      &cmeta_type_int, CMETA_ABI_SCALAR, CMETA_RESULT_VALUE,          \
      (int, ticket, CMETA_PARAM_IN, &cmeta_type_int, CMETA_ABI_SCALAR), \
      (int *, out, CMETA_PARAM_OUT | CMETA_PARAM_BORROWED,            \
       &cmeta_type_int_ptr, CMETA_ABI_OBJECT_POINTER))

CMETA_INTERFACE(book_ace_server_policy, BOOK_SERVER_METHODS);
CMETA_INTERFACE(book_ace_client_policy, BOOK_CLIENT_METHODS);

typedef enum book_policy_kind {
    BOOK_POLICY_NONE = 0,
    BOOK_POLICY_STRICT,
    BOOK_POLICY_RR
} book_policy_kind;

typedef struct book_config {
    book_policy_kind server;
    book_policy_kind client;
} book_config;

typedef enum book_control_status {
    BOOK_CONTROL_OK = 0,
    BOOK_CONTROL_INVALID,
    BOOK_CONTROL_BUSY,
    BOOK_CONTROL_FOREIGN,
    BOOK_CONTROL_STALE
} book_control_status;

typedef struct book_host {
    const void *owner_token;
    book_config active;
    cnet_owner_placement_hint owners[3];
    cnet_destination_hint endpoints[3];
    uint64_t generation;
    size_t leases;
    bool open;
} book_host;

typedef struct book_call_lease {
    book_host *host;
    const struct book_call_lease *identity;
    uint64_t generation;
    bool live;
    bool spent; /* one-shot tombstone; no lease slot resurrection */
} book_call_lease;

static book_control_status book_parse(const char *path, book_config *out)
{
    FILE *file;
    char line[128];
    book_config candidate = {0};
    unsigned seen = 0u;
    book_control_status status = BOOK_CONTROL_INVALID;

    if (path == NULL || out == NULL)
        return BOOK_CONTROL_INVALID;
    file = fopen(path, "rb");
    if (file == NULL)
        return BOOK_CONTROL_INVALID;

    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strlen(line);
        if (length == 0u ||
            (length == sizeof(line) - 1u && line[length - 1u] != '\n'))
            goto finish;
        if (line[length - 1u] == '\n')
            line[--length] = '\0';
        if (length != 0u && line[length - 1u] == '\r')
            line[--length] = '\0';
        if (length == 0u)
            goto finish;

        if (strcmp(line, "version=1") == 0) {
            if ((seen & 1u) != 0u)
                goto finish;
            seen |= 1u;
        } else if (strncmp(line, "server=", 7u) == 0) {
            if ((seen & 2u) != 0u)
                goto finish;
            if (strcmp(line + 7u, "strict_key") == 0)
                candidate.server = BOOK_POLICY_STRICT;
            else if (strcmp(line + 7u, "round_robin") == 0)
                candidate.server = BOOK_POLICY_RR;
            else
                goto finish;
            seen |= 2u;
        } else if (strncmp(line, "client=", 7u) == 0) {
            if ((seen & 4u) != 0u)
                goto finish;
            if (strcmp(line + 7u, "strict_key") == 0)
                candidate.client = BOOK_POLICY_STRICT;
            else if (strcmp(line + 7u, "round_robin") == 0)
                candidate.client = BOOK_POLICY_RR;
            else
                goto finish;
            seen |= 4u;
        } else {
            goto finish;
        }
    }
    if (ferror(file) == 0 && seen == 7u) {
        *out = candidate;  /* failure-atomic publication of parsed values */
        status = BOOK_CONTROL_OK;
    }
finish:
    (void)fclose(file);
    return status;
}

static book_control_status book_owner_only(const book_host *host)
{
    if (host == NULL)
        return BOOK_CONTROL_INVALID;
    if (host->owner_token != NULL &&
        host->owner_token != cmeta_thread_current_token())
        return BOOK_CONTROL_FOREIGN;
    return BOOK_CONTROL_OK;
}

/* The Interface self is an individual, borrowed lease slot, NOT a host.
 * Never recycle a spent slot, even after a new generation is published:
 * otherwise a stale Interface copied earlier could regain authority (ABA).
 * The caller keeps the slot storage alive while any stale view is inspected.
 */
static int book_permit(const book_call_lease *lease)
{
    book_host *host;
    if (lease == NULL || lease->identity != lease ||
        !lease->live || lease->host == NULL)
        return SALTS_ESHUTDOWN;
    host = lease->host;
    if (book_owner_only(host) != BOOK_CONTROL_OK)
        return SALTS_EPERM;
    if (!host->open || lease->generation != host->generation ||
        host->leases == 0u)
        return SALTS_ESHUTDOWN;
    return SALTS_OK;
}

/* A negative key is invalid, rather than an implicit wrap to uint64_t. */
static int book_choose_owner(
    book_host *host, cnet_owner_placement_kind kind, int ticket, int *out)
{
    cnet_owner_placement_input input = {0};
    size_t chosen = SIZE_MAX;
    int rc;

    if (host == NULL || out == NULL || ticket < 0)
        return SALTS_EINVAL;
    if (book_owner_only(host) != BOOK_CONTROL_OK)
        return SALTS_EPERM;
    if (!host->open || host->leases == 0u)
        return SALTS_ESHUTDOWN;
    input.size = sizeof(input);
    input.version = CNET_OWNER_PLACEMENT_VERSION;
    input.kind = kind;
    input.owners = host->owners;
    input.owner_count = sizeof(host->owners) / sizeof(host->owners[0]);
    input.key_hash = (uint64_t)ticket;
    input.key_known = true;
    input.sequence = (uint64_t)ticket;

    rc = cnet_owner_placement_choose(&input, &chosen);
    if (rc == SALTS_OK)
        *out = (int)chosen;
    return rc; /* never silently select a backup for strict-key FULL */
}

static int book_server_strict(void *self, int ticket, int *out)
{
    book_call_lease *lease = (book_call_lease *)self;
    int rc = book_permit(lease);
    if (rc != SALTS_OK)
        return rc;
    if (lease->host->active.server != BOOK_POLICY_STRICT)
        return SALTS_EPROTO;
    return book_choose_owner(
        lease->host, CNET_OWNER_PLACE_STRICT_KEY, ticket, out);
}
static int book_server_rr(void *self, int ticket, int *out)
{
    book_call_lease *lease = (book_call_lease *)self;
    int rc = book_permit(lease);
    if (rc != SALTS_OK)
        return rc;
    if (lease->host->active.server != BOOK_POLICY_RR)
        return SALTS_EPROTO;
    return book_choose_owner(
        lease->host, CNET_OWNER_PLACE_ROUND_ROBIN, ticket, out);
}

/* The remote endpoint policy uses stable IDs, not final SG Owner indices. */
static int book_choose_client(
    book_host *host, cnet_destination_policy_kind kind, int ticket, int *out)
{
    cnet_destination_selection selection = {0};
    cnet_destination_result result = {0};
    int rc;

    if (host == NULL || out == NULL || ticket < 0)
        return SALTS_EINVAL;
    if (book_owner_only(host) != BOOK_CONTROL_OK)
        return SALTS_EPERM;
    if (!host->open || host->leases == 0u)
        return SALTS_ESHUTDOWN;
    selection.size = sizeof(selection);
    selection.version = CNET_DESTINATION_POLICY_VERSION;
    selection.kind = kind;
    selection.endpoints = host->endpoints;
    selection.endpoint_count =
        sizeof(host->endpoints) / sizeof(host->endpoints[0]);
    selection.snapshot_generation = 7u;
    selection.expires_at_ms = UINT64_MAX;
    selection.now_ms = 0u;
    selection.sequence = (uint64_t)ticket;
    selection.key_hash = (uint64_t)ticket;
    selection.key_known = true;

    rc = cnet_destination_choose(&selection, &result);
    if (rc == SALTS_OK) {
        /* This book fixture intentionally uses small, verified endpoint IDs. */
        if (result.endpoint_id > 33u)
            return SALTS_ERANGE;
        *out = (int)result.endpoint_id;
    }
    return rc;
}

static int book_client_strict(void *self, int ticket, int *out)
{
    book_call_lease *lease = (book_call_lease *)self;
    int rc = book_permit(lease);
    if (rc != SALTS_OK)
        return rc;
    if (lease->host->active.client != BOOK_POLICY_STRICT)
        return SALTS_EPROTO;
    return book_choose_client(
        lease->host, CNET_DESTINATION_STRICT_KEY, ticket, out);
}
static int book_client_rr(void *self, int ticket, int *out)
{
    book_call_lease *lease = (book_call_lease *)self;
    int rc = book_permit(lease);
    if (rc != SALTS_OK)
        return rc;
    if (lease->host->active.client != BOOK_POLICY_RR)
        return SALTS_EPROTO;
    return book_choose_client(
        lease->host, CNET_DESTINATION_ROUND_ROBIN, ticket, out);
}

CMETA_IMPLEMENTS(book_ace_server_policy, book_server_strict_impl, 0u,
                 .select = book_server_strict);
CMETA_IMPLEMENTS(book_ace_server_policy, book_server_rr_impl, 0u,
                 .select = book_server_rr);
CMETA_IMPLEMENTS(book_ace_client_policy, book_client_strict_impl, 0u,
                 .select = book_client_strict);
CMETA_IMPLEMENTS(book_ace_client_policy, book_client_rr_impl, 0u,
                 .select = book_client_rr);

static void book_publish(book_host *host, book_config config)
{
    /* No Interface self is published from a shared host. Each new lease binds
     * its own exact typed CMeta dispatch after complete config admission.
     */
    host->active = config;
    ++host->generation;
}

static book_control_status book_open(book_host *host, const char *path)
{
    book_config candidate = {0};
    book_control_status status = book_owner_only(host);
    if (status != BOOK_CONTROL_OK)
        return status;
    if (host->open || host->leases != 0u || host->generation == UINT64_MAX)
        return BOOK_CONTROL_BUSY;
    status = book_parse(path, &candidate);
    if (status != BOOK_CONTROL_OK)
        return status;

    for (size_t index = 0u; index < 3u; ++index) {
        host->owners[index].eligible = true;
        host->owners[index].pressure = 0u;
        host->endpoints[index].endpoint_id = (index + 1u) * 11u;
        host->endpoints[index].weight = 1u;
        host->endpoints[index].inflight = 0u;
        host->endpoints[index].eligible = true;
    }
    host->owner_token = cmeta_thread_current_token();
    book_publish(host, candidate);
    host->open = true;
    return BOOK_CONTROL_OK;
}

static book_control_status book_reload(book_host *host, const char *path)
{
    book_config candidate = {0};
    book_control_status status = book_owner_only(host);
    if (status != BOOK_CONTROL_OK)
        return status;
    if (!host->open)
        return BOOK_CONTROL_STALE;
    if (host->leases != 0u || host->generation == UINT64_MAX)
        return BOOK_CONTROL_BUSY;
    status = book_parse(path, &candidate);
    if (status != BOOK_CONTROL_OK)
        return status;
    book_publish(host, candidate);  /* quiescent dual-policy commit */
    return BOOK_CONTROL_OK;
}

static book_control_status book_acquire(
    book_host *host, book_call_lease *lease,
    book_ace_server_policy *server, book_ace_client_policy *client)
{
    book_control_status status = book_owner_only(host);
    if (status != BOOK_CONTROL_OK)
        return status;
    if (lease == NULL || server == NULL || client == NULL)
        return BOOK_CONTROL_INVALID;
    if (!host->open)
        return BOOK_CONTROL_STALE;
    if (lease->live)
        return BOOK_CONTROL_BUSY;
    if (lease->spent)
        return BOOK_CONTROL_STALE;
    if (host->leases == SIZE_MAX)
        return BOOK_CONTROL_BUSY;

    lease->host = host;
    lease->identity = lease;
    lease->generation = host->generation;
    lease->live = true;
    lease->spent = true;
    ++host->leases;
    *server = host->active.server == BOOK_POLICY_STRICT ?
        book_server_strict_impl_as_book_ace_server_policy(lease) :
        book_server_rr_impl_as_book_ace_server_policy(lease);
    *client = host->active.client == BOOK_POLICY_STRICT ?
        book_client_strict_impl_as_book_ace_client_policy(lease) :
        book_client_rr_impl_as_book_ace_client_policy(lease);
    /* Both Interfaces now borrow THIS one-shot slot, never another lease. */
    return BOOK_CONTROL_OK;
}

static book_control_status book_release(book_call_lease *lease)
{
    book_host *host;
    if (lease == NULL || lease->identity != lease ||
        !lease->live || lease->host == NULL)
        return BOOK_CONTROL_STALE;
    host = lease->host;
    if (book_owner_only(host) != BOOK_CONTROL_OK)
        return BOOK_CONTROL_FOREIGN;
    if (!host->open || host->generation != lease->generation ||
        host->leases == 0u)
        return BOOK_CONTROL_STALE;

    --host->leases;
    lease->host = NULL;
    lease->identity = NULL;
    lease->live = false;
    return BOOK_CONTROL_OK;
}

static book_control_status book_close(book_host *host)
{
    book_control_status status = book_owner_only(host);
    if (status != BOOK_CONTROL_OK)
        return status;
    if (!host->open)
        return BOOK_CONTROL_STALE;
    if (host->leases != 0u || host->generation == UINT64_MAX)
        return BOOK_CONTROL_BUSY;
    host->open = false;
    ++host->generation;
    return BOOK_CONTROL_OK;
}

typedef struct book_foreign_probe {
    book_host *host;
    book_call_lease lease;
    book_ace_server_policy server;
    book_ace_client_policy client;
    book_control_status acquire_status;
    book_control_status reload_status;
    int owner_status;
    int client_status;
    int out_owner;
    int out_client;
} book_foreign_probe;

static void book_foreign_try(void *user)
{
    book_foreign_probe *probe = (book_foreign_probe *)user;
    probe->acquire_status =
        book_acquire(probe->host, &probe->lease,
                     &probe->server, &probe->client);
    probe->reload_status =
        book_reload(probe->host, "ace_cnet_policy_rr.cfg");
    probe->owner_status =
        book_ace_server_policy_select(&probe->server, 5, &probe->out_owner);
    probe->client_status =
        book_ace_client_policy_select(&probe->client, 5, &probe->out_client);
}

int main(void)
{
    book_host host = {0};
    book_call_lease lease = {0}, copied = {0};
    book_call_lease overlapping = {0}, after_mixed = {0}, after_rr = {0};
    book_ace_server_policy server = {0}, retired_server = {0};
    book_ace_client_policy client = {0}, retired_client = {0};
    book_foreign_probe foreign = {0};
    cmeta_thread_t thread = NULL;
    book_config unchanged = {BOOK_POLICY_RR, BOOK_POLICY_STRICT};
    int owner = -1, remote = -1, result;
    uint64_t generation;

    BOOK_CHECK(cmeta_interface_desc_valid(book_ace_server_policy_interface()));
    BOOK_CHECK(cmeta_interface_desc_valid(book_ace_client_policy_interface()));
    BOOK_CHECK(book_ace_server_policy_interface()->methods[0].abi != NULL);
    BOOK_CHECK(book_ace_client_policy_interface()->methods[0].abi != NULL);
    BOOK_CHECK(book_ace_server_policy_interface()->methods[0].dispatch_arity == 2u);
    BOOK_CHECK(book_ace_client_policy_interface()->methods[0].dispatch_arity == 2u);

    BOOK_CHECK(book_parse("ace_cnet_policy_invalid.cfg", &unchanged) ==
               BOOK_CONTROL_INVALID);
    BOOK_CHECK(book_parse("ace_cnet_policy_missing.cfg", &unchanged) ==
               BOOK_CONTROL_INVALID);
    BOOK_CHECK(book_parse("ace_cnet_policy_unknown.cfg", &unchanged) ==
               BOOK_CONTROL_INVALID);
    BOOK_CHECK(unchanged.server == BOOK_POLICY_RR &&
               unchanged.client == BOOK_POLICY_STRICT);

    BOOK_CHECK(book_open(&host, "ace_cnet_policy_strict.cfg") == BOOK_CONTROL_OK);
    BOOK_CHECK(host.generation == 1u && host.open);
    BOOK_CHECK(book_acquire(&host, &lease, &server, &client) == BOOK_CONTROL_OK);

    BOOK_CHECK(book_ace_server_policy_select(&server, 5, &owner) == SALTS_OK);
    BOOK_CHECK(owner == 2);          /* STRICT_KEY key % 3, not a shard fallback */
    BOOK_CHECK(book_ace_client_policy_select(&client, 5, &remote) == SALTS_OK);
    BOOK_CHECK(remote == 11 || remote == 22 || remote == 33);
    BOOK_CHECK(owner != remote);      /* local Owner versus remote endpoint ID */

    /* A pinned FULL owner and unavailable remote winner must reject in place. */
    host.owners[2].eligible = false;
    owner = -27;
    BOOK_CHECK(book_ace_server_policy_select(&server, 5, &owner) == SALTS_ENOBUFS);
    BOOK_CHECK(owner == -27);
    host.owners[2].eligible = true;
    host.endpoints[(size_t)(remote / 11 - 1)].eligible = false;
    result = -27;
    BOOK_CHECK(book_ace_client_policy_select(&client, 5, &result) == SALTS_ENOBUFS);
    BOOK_CHECK(result == -27);
    host.endpoints[(size_t)(remote / 11 - 1)].eligible = true;

    generation = host.generation;
    BOOK_CHECK(book_reload(&host, "ace_cnet_policy_mixed.cfg") == BOOK_CONTROL_BUSY);
    BOOK_CHECK(book_close(&host) == BOOK_CONTROL_BUSY);
    BOOK_CHECK(host.generation == generation);

    foreign.host = &host;
    foreign.server = server;  /* test a copied Interface borrowed from caller */
    foreign.client = client;
    foreign.out_owner = foreign.out_client = -77;
    BOOK_CHECK(cmeta_thread_create(&thread, book_foreign_try, &foreign) == SALTS_OK);
    BOOK_CHECK(cmeta_thread_join(&thread) == SALTS_OK);
    cmeta_thread_destroy(&thread);
    BOOK_CHECK(foreign.acquire_status == BOOK_CONTROL_FOREIGN);
    BOOK_CHECK(foreign.reload_status == BOOK_CONTROL_FOREIGN);
    BOOK_CHECK(foreign.owner_status == SALTS_EPERM);
    BOOK_CHECK(foreign.client_status == SALTS_EPERM);
    BOOK_CHECK(foreign.out_owner == -77 && foreign.out_client == -77);
    BOOK_CHECK(host.leases == 1u);

    copied = lease;
    BOOK_CHECK(book_release(&copied) == BOOK_CONTROL_STALE);
    BOOK_CHECK(book_release(&lease) == BOOK_CONTROL_OK);
    BOOK_CHECK(book_release(&lease) == BOOK_CONTROL_STALE);
    retired_server = server; /* stale, but slot storage is still live */
    retired_client = client;
    owner = remote = -17;
    BOOK_CHECK(book_ace_server_policy_select(&retired_server, 5, &owner) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(book_ace_client_policy_select(&retired_client, 5, &remote) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(owner == -17 && remote == -17);

    /* Another unrelated lease cannot revive a retired CMeta self pointer.
     * Owner and generation are deliberately still the same at this point.
     */
    BOOK_CHECK(book_acquire(&host, &overlapping, &server, &client) ==
               BOOK_CONTROL_OK);
    owner = remote = -99;
    BOOK_CHECK(book_ace_server_policy_select(&retired_server, 5, &owner) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(book_ace_client_policy_select(&retired_client, 5, &remote) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(owner == -99 && remote == -99);
    BOOK_CHECK(book_ace_server_policy_select(&server, 5, &owner) == SALTS_OK);
    BOOK_CHECK(book_ace_client_policy_select(&client, 5, &remote) == SALTS_OK);
    BOOK_CHECK(book_acquire(&host, &lease, &server, &client) ==
               BOOK_CONTROL_STALE); /* one-shot lease storage cannot be reused */
    BOOK_CHECK(book_release(&overlapping) == BOOK_CONTROL_OK);
    BOOK_CHECK(book_reload(&host, "ace_cnet_policy_invalid.cfg") == BOOK_CONTROL_INVALID);
    BOOK_CHECK(book_reload(&host, "ace_cnet_policy_unknown.cfg") == BOOK_CONTROL_INVALID);
    BOOK_CHECK(host.generation == generation);

    /* Change client policy only. Server stays pinned; client uses RR ticket. */
    BOOK_CHECK(book_reload(&host, "ace_cnet_policy_mixed.cfg") == BOOK_CONTROL_OK);
    BOOK_CHECK(host.generation == generation + 1u);
    BOOK_CHECK(host.active.server == BOOK_POLICY_STRICT);
    BOOK_CHECK(host.active.client == BOOK_POLICY_RR);
    BOOK_CHECK(book_acquire(&host, &after_mixed, &server, &client) ==
               BOOK_CONTROL_OK);
    owner = remote = -1;
    BOOK_CHECK(book_ace_server_policy_select(&retired_server, 5, &owner) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(book_ace_client_policy_select(&retired_client, 5, &remote) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(owner == -1 && remote == -1);
    BOOK_CHECK(book_ace_server_policy_select(&server, 5, &owner) == SALTS_OK);
    BOOK_CHECK(owner == 2);
    BOOK_CHECK(book_ace_client_policy_select(&client, 5, &remote) == SALTS_OK);
    BOOK_CHECK(remote == 33); /* RR 5 % 3 => index 2, stable endpoint ID 33 */
    BOOK_CHECK(book_release(&after_mixed) == BOOK_CONTROL_OK);

    /* Then switch server too; RR can select next eligible capacity hint.
     * This is still not a real handoff.reserve() admission commitment.
     */
    BOOK_CHECK(book_reload(&host, "ace_cnet_policy_rr.cfg") == BOOK_CONTROL_OK);
    BOOK_CHECK(book_acquire(&host, &after_rr, &server, &client) ==
               BOOK_CONTROL_OK);
    host.owners[2].eligible = false;
    owner = -1;
    BOOK_CHECK(book_ace_server_policy_select(&server, 5, &owner) == SALTS_OK);
    BOOK_CHECK(owner == 0);
    host.endpoints[2].eligible = false;
    remote = -1;
    BOOK_CHECK(book_ace_client_policy_select(&client, 5, &remote) == SALTS_OK);
    BOOK_CHECK(remote == 11);
    BOOK_CHECK(host.leases == 1u); /* no capacity was acquired by choose() */
    BOOK_CHECK(book_release(&after_rr) == BOOK_CONTROL_OK);

    BOOK_CHECK(book_close(&host) == BOOK_CONTROL_OK);
    BOOK_CHECK(!host.open && host.leases == 0u);
    BOOK_CHECK(book_acquire(&host, &lease, &server, &client) == BOOK_CONTROL_STALE);
    return 0;
}
