/*
 * ACE Service Configurator: exact typed CMeta Strategy + strict C11 config.
 *
 * The host is owner-affine and address-stable. A published Strategy borrows
 * its one-shot lease slot; only that explicitly live lease permits dispatch.
 * Reconfiguration is validate-then-publish, requires no live borrows, and
 * never creates threads, sockets, a plugin loader, or a hidden fallback.
 *
 * Grammar: exactly one each of version=1, strategy=add|scale,
 * operand=1..16; no unknown/duplicate/missing/oversized fields.
 */
#include <cmeta/interface.h>
#include <salts/thread.h>
#include <salts/error_codes.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOOK_ACE_CONFIG_METHODS(X, I) \
    X(I, FR2, int, apply, stateful, \
      &cmeta_type_int, CMETA_ABI_SCALAR, CMETA_RESULT_VALUE, \
      (int, value, CMETA_PARAM_IN, &cmeta_type_int, CMETA_ABI_SCALAR), \
      (int *, out, CMETA_PARAM_OUT | CMETA_PARAM_BORROWED, \
       &cmeta_type_int_ptr, CMETA_ABI_OBJECT_POINTER))

CMETA_INTERFACE(book_ace_config_strategy, BOOK_ACE_CONFIG_METHODS);

typedef enum book_mode {
    BOOK_MODE_NONE = 0,
    BOOK_MODE_ADD,
    BOOK_MODE_SCALE
} book_mode;

typedef enum book_result {
    BOOK_OK = 0,
    BOOK_INVALID,
    BOOK_BUSY,
    BOOK_STALE,
    BOOK_FOREIGN
} book_result;

typedef struct book_config {
    book_mode mode;
    int operand;
} book_config;

/* A CMeta Interface must borrow the INDIVIDUAL call lease, not the host.
 * This one-shot slot must remain allocated while any view of it is inspected.
 * Reusing its address is forbidden: a copied stale Interface must never gain
 * authority again under a later, unrelated successful acquisition (ABA).
 */
typedef struct book_host book_host;
typedef struct book_lease book_lease;
struct book_host {
    const void *owner_token;
    book_config active;
    uint64_t generation;
    size_t leases;
    bool open;
};
struct book_lease {
    book_host *host;
    const book_lease *identity;
    uint64_t generation;
    bool live;
    bool spent; /* permanent tombstone, never recycled */
};

static int book_permit(const book_lease *lease)
{
    const book_host *host;
    if (lease == NULL || lease->identity != lease ||
        !lease->live || lease->host == NULL)
        return SALTS_ESHUTDOWN;
    host = lease->host;
    if (host->owner_token != cmeta_thread_current_token())
        return SALTS_EPERM;
    if (!host->open || lease->generation != host->generation ||
        host->leases == 0u)
        return SALTS_ESHUTDOWN;
    return SALTS_OK;
}

/* Exact typed status/out separates legitimate arithmetic values from errors.
 * Reject invalid pointer, stale borrow, wrong Strategy or signed overflow
 * before touching caller-owned output.
 */
static int book_apply_add(void *self, int value, int *out)
{
    const book_lease *lease = (const book_lease *)self;
    int rc = book_permit(lease);
    int operand;
    if (rc != SALTS_OK)
        return rc;
    if (out == NULL)
        return SALTS_EINVAL;
    if (lease->host->active.mode != BOOK_MODE_ADD)
        return SALTS_EPROTO;
    operand = lease->host->active.operand;
    if (value > INT_MAX - operand)
        return SALTS_ERANGE;
    *out = value + operand;
    return SALTS_OK;
}

static int book_apply_scale(void *self, int value, int *out)
{
    const book_lease *lease = (const book_lease *)self;
    int rc = book_permit(lease);
    int operand;
    if (rc != SALTS_OK)
        return rc;
    if (out == NULL)
        return SALTS_EINVAL;
    if (lease->host->active.mode != BOOK_MODE_SCALE)
        return SALTS_EPROTO;
    operand = lease->host->active.operand;
    if (value > INT_MAX / operand || value < INT_MIN / operand)
        return SALTS_ERANGE;
    *out = value * operand;
    return SALTS_OK;
}

CMETA_IMPLEMENTS(book_ace_config_strategy, book_config_add_impl, 0u,
                 .apply = book_apply_add);
CMETA_IMPLEMENTS(book_ace_config_strategy, book_config_scale_impl, 0u,
                 .apply = book_apply_scale);

#define BOOK_CHECK(expr) do {                                          \
    if (!(expr)) {                                                     \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);     \
        return 1;                                                      \
    }                                                                  \
} while (0)

static book_result book_parse(const char *path, book_config *out)
{
    char line[128];
    book_config candidate = {0};
    unsigned seen = 0u;
    FILE *file = NULL;
    book_result status = BOOK_INVALID;

    if (path == NULL || out == NULL)
        return BOOK_INVALID;
    file = fopen(path, "rb");
    if (file == NULL)
        return BOOK_INVALID;

    while (fgets(line, sizeof(line), file) != NULL) {
        size_t n = strlen(line);
        if (n == 0u || (n == sizeof(line) - 1u && line[n - 1u] != '\n'))
            goto done;
        if (line[n - 1u] == '\n')
            line[--n] = '\0';
        if (n != 0u && line[n - 1u] == '\r')
            line[--n] = '\0';
        if (n == 0u)
            goto done;

        if (strcmp(line, "version=1") == 0) {
            if ((seen & 1u) != 0u)
                goto done;
            seen |= 1u;
        } else if (strncmp(line, "strategy=", 9u) == 0) {
            if ((seen & 2u) != 0u)
                goto done;
            if (strcmp(line + 9u, "add") == 0)
                candidate.mode = BOOK_MODE_ADD;
            else if (strcmp(line + 9u, "scale") == 0)
                candidate.mode = BOOK_MODE_SCALE;
            else
                goto done;
            seen |= 2u;
        } else if (strncmp(line, "operand=", 8u) == 0) {
            char *end = NULL;
            long parsed;
            if ((seen & 4u) != 0u)
                goto done;
            errno = 0;
            parsed = strtol(line + 8u, &end, 10);
            if (errno != 0 || end == line + 8u || *end != '\0' ||
                parsed < 1 || parsed > 16)
                goto done;
            candidate.operand = (int)parsed;
            seen |= 4u;
        } else {
            goto done;
        }
    }

    if (ferror(file) == 0 && seen == 7u) {
        *out = candidate; /* publish only after complete validation */
        status = BOOK_OK;
    }
done:
    (void)fclose(file);
    return status;
}

static book_result book_owner_check(const book_host *host)
{
    if (host == NULL)
        return BOOK_INVALID;
    if (host->owner_token != NULL &&
        host->owner_token != cmeta_thread_current_token())
        return BOOK_FOREIGN;
    return BOOK_OK;
}

static book_ace_config_strategy book_bind(book_lease *lease)
{
    if (lease->host->active.mode == BOOK_MODE_ADD)
        return book_config_add_impl_as_book_ace_config_strategy(lease);
    return book_config_scale_impl_as_book_ace_config_strategy(lease);
}

static book_result book_host_open(book_host *host, const char *path)
{
    book_config candidate = {0};
    book_result status;

    if (host == NULL || path == NULL)
        return BOOK_INVALID;
    status = book_owner_check(host);
    if (status != BOOK_OK)
        return status;
    if (host->open || host->leases != 0u || host->generation == UINT64_MAX)
        return BOOK_BUSY;
    status = book_parse(path, &candidate);
    if (status != BOOK_OK)
        return status;

    host->active = candidate;
    host->generation++;
    host->owner_token = cmeta_thread_current_token();
    host->open = true;
    return BOOK_OK;
}

static book_result book_host_reload(book_host *host, const char *path)
{
    book_config candidate = {0};
    book_result status = book_owner_check(host);

    if (status != BOOK_OK)
        return status;
    if (!host->open)
        return BOOK_STALE;
    if (host->generation == UINT64_MAX)
        return BOOK_BUSY;

    /* A failed new file never mutates the published CMeta capability. */
    status = book_parse(path, &candidate);
    if (status != BOOK_OK)
        return status;
    if (host->leases != 0u)
        return BOOK_BUSY;

    host->active = candidate; /* stable, caller-owned storage */
    host->generation++;
    return BOOK_OK;
}

static book_result book_host_acquire(
    book_host *host, book_lease *lease, book_ace_config_strategy *out)
{
    book_result status = book_owner_check(host);
    if (status != BOOK_OK)
        return status;
    if (lease == NULL || out == NULL)
        return BOOK_INVALID;
    if (!host->open)
        return BOOK_STALE;
    if (lease->live)
        return BOOK_BUSY;
    if (lease->spent)
        return BOOK_STALE;
    if (host->leases == SIZE_MAX)
        return BOOK_BUSY;

    lease->host = host;
    lease->identity = lease; /* copying the lease never duplicates authority */
    lease->generation = host->generation;
    lease->live = true;
    lease->spent = true;
    ++host->leases;
    *out = book_bind(lease); /* self borrows this ONE-SHOT lease slot */
    return BOOK_OK;
}

static book_result book_host_release(book_lease *lease)
{
    book_host *host;
    if (lease == NULL || lease->identity != lease || !lease->live ||
        lease->host == NULL)
        return BOOK_STALE;
    host = lease->host;
    if (book_owner_check(host) != BOOK_OK)
        return BOOK_FOREIGN;
    if (!host->open || lease->generation != host->generation ||
        host->leases == 0u)
        return BOOK_STALE;
    --host->leases;
    lease->host = NULL;
    lease->identity = NULL;
    lease->live = false;
    return BOOK_OK;
}

static book_result book_host_close(book_host *host)
{
    book_result status = book_owner_check(host);
    if (status != BOOK_OK)
        return status;
    if (!host->open)
        return BOOK_STALE;
    if (host->leases != 0u || host->generation == UINT64_MAX)
        return BOOK_BUSY;
    host->open = false;
    host->generation++;
    return BOOK_OK;
}

typedef struct book_foreign_probe {
    book_host *host;
    book_lease lease;
    book_ace_config_strategy view;
    book_result result;
    book_result reload_result;
    int direct_call_result;
    int direct_output;
} book_foreign_probe;

static void book_foreign_try_acquire(void *user)
{
    book_foreign_probe *probe = (book_foreign_probe *)user;
    probe->result = book_host_acquire(probe->host, &probe->lease, &probe->view);
    probe->reload_result = book_host_reload(
        probe->host, "ace_configurator_scale.cfg");
    probe->direct_call_result =
        book_ace_config_strategy_apply(&probe->view, 7,
                                       &probe->direct_output);
}

int main(void)
{
    book_host host = {0};
    book_config unmodified = {BOOK_MODE_SCALE, 7};
    book_ace_config_strategy view = {0};
    book_lease lease = {0};
    book_lease overlapping = {0};
    book_lease after_reload = {0};
    book_lease duplicate = {0};
    book_ace_config_strategy current = {0};
    book_foreign_probe worker = {0};
    cmeta_thread_t thread = NULL;
    uint64_t generation;
    int output = -77;
    const char *const invalid_files[] = {
        "ace_configurator_invalid.cfg",
        "ace_configurator_missing.cfg",
        "ace_configurator_unknown.cfg",
        "ace_configurator_range.cfg"
    };

    BOOK_CHECK(cmeta_interface_desc_valid(book_ace_config_strategy_interface()));
    BOOK_CHECK(book_ace_config_strategy_interface()->methods[0].abi != NULL);

    /* Invalid configuration is not published even during initialization. */
    for (size_t i = 0u; i < sizeof(invalid_files)/sizeof(invalid_files[0]); ++i) {
        BOOK_CHECK(book_parse(invalid_files[i], &unmodified) == BOOK_INVALID);
        BOOK_CHECK(unmodified.mode == BOOK_MODE_SCALE && unmodified.operand == 7);
    }
    BOOK_CHECK(book_host_open(&host, "ace_configurator_add.cfg") == BOOK_OK);
    BOOK_CHECK(host.open && host.generation == 1u);

    BOOK_CHECK(book_host_acquire(&host, &lease, &view) == BOOK_OK);
    BOOK_CHECK(book_ace_config_strategy_apply(&view, 7, &output) == SALTS_OK);
    BOOK_CHECK(output == 10);
    output = -77;
    BOOK_CHECK(book_ace_config_strategy_apply(&view, INT_MAX, &output) ==
               SALTS_ERANGE);
    BOOK_CHECK(output == -77); /* overflow must fail before writing output */
    BOOK_CHECK(book_ace_config_strategy_apply(&view, 7, NULL) ==
               SALTS_EINVAL);
    generation = host.generation;
    BOOK_CHECK(book_host_reload(&host, "ace_configurator_scale.cfg") ==
               BOOK_BUSY);
    BOOK_CHECK(book_host_close(&host) == BOOK_BUSY);
    BOOK_CHECK(host.generation == generation);
    BOOK_CHECK(book_ace_config_strategy_apply(&view, 7, &output) == SALTS_OK);
    BOOK_CHECK(output == 10);

    /* A foreign thread cannot obtain a borrowed strategy or a lease. */
    worker.host = &host;
    worker.view = view; /* pass copied Interface, not new ownership */
    worker.direct_output = -77;
    BOOK_CHECK(cmeta_thread_create(&thread, book_foreign_try_acquire, &worker)
               == SALTS_OK);
    BOOK_CHECK(cmeta_thread_join(&thread) == SALTS_OK);
    cmeta_thread_destroy(&thread);
    BOOK_CHECK(worker.result == BOOK_FOREIGN);
    BOOK_CHECK(worker.reload_result == BOOK_FOREIGN);
    BOOK_CHECK(worker.direct_call_result == SALTS_EPERM);
    BOOK_CHECK(worker.direct_output == -77);
    BOOK_CHECK(!worker.lease.live && host.leases == 1u);

    duplicate = lease; /* an ordinary struct copy cannot duplicate authority */
    BOOK_CHECK(book_host_release(&duplicate) == BOOK_STALE);
    BOOK_CHECK(host.leases == 1u);
    BOOK_CHECK(book_host_release(&lease) == BOOK_OK);
    BOOK_CHECK(book_host_release(&lease) == BOOK_STALE);
    BOOK_CHECK(host.leases == 0u);
    output = -77;
    BOOK_CHECK(book_ace_config_strategy_apply(&view, 7, &output) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(output == -77);

    /* An unrelated live lease must never revive the old copied Interface. */
    BOOK_CHECK(book_host_acquire(&host, &overlapping, &current) == BOOK_OK);
    BOOK_CHECK(book_ace_config_strategy_apply(&current, 7, &output) == SALTS_OK);
    BOOK_CHECK(output == 10);
    output = -77;
    BOOK_CHECK(book_ace_config_strategy_apply(&view, 7, &output) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(output == -77);
    BOOK_CHECK(book_host_acquire(&host, &lease, &view) == BOOK_STALE);
    BOOK_CHECK(book_host_release(&overlapping) == BOOK_OK);
    BOOK_CHECK(book_ace_config_strategy_apply(&current, 7, &output) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(output == -77);

    for (size_t i = 0u; i < sizeof(invalid_files)/sizeof(invalid_files[0]); ++i) {
        BOOK_CHECK(book_host_reload(&host, invalid_files[i]) == BOOK_INVALID);
        BOOK_CHECK(host.generation == generation);
    }
    BOOK_CHECK(book_host_reload(&host, "ace_configurator_scale.cfg") == BOOK_OK);
    BOOK_CHECK(host.generation == generation + 1u);
    BOOK_CHECK(book_host_acquire(&host, &after_reload, &current) == BOOK_OK);
    BOOK_CHECK(book_ace_config_strategy_apply(&view, 7, &output) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(output == -77);
    BOOK_CHECK(book_ace_config_strategy_apply(&current, 7, &output) == SALTS_OK);
    BOOK_CHECK(output == 14);
    output = -77;
    BOOK_CHECK(book_ace_config_strategy_apply(&current, INT_MAX, &output) ==
               SALTS_ERANGE);
    BOOK_CHECK(output == -77); /* multiplication cannot overflow silently */
    BOOK_CHECK(book_host_release(&after_reload) == BOOK_OK);
    BOOK_CHECK(book_ace_config_strategy_apply(&current, 7, &output) ==
               SALTS_ESHUTDOWN);
    BOOK_CHECK(output == -77);

    BOOK_CHECK(book_host_close(&host) == BOOK_OK);
    BOOK_CHECK(!host.open && host.leases == 0u);
    BOOK_CHECK(book_host_acquire(&host, &lease, &view) == BOOK_STALE);
    BOOK_CHECK(book_host_close(&host) == BOOK_STALE);
    /* No live handles, no implicit socket migration, retry, or resource leak. */
    return 0;
}
