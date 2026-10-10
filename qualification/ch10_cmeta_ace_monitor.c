/*
 * Strict C11, latest installed Salts SDK: ACE Monitor Object and
 * Thread-Specific Storage implemented through CMeta + Platform.
 *
 * CMeta defines an exact reflected public Interface; Platform alone owns
 * mutex/condition/TLS/threads. No Actor mailbox, scheduler, second I/O
 * engine, process registry or provider retention is introduced.
 */
#include <cmeta/interface.h>
#include <cmeta/status.h>
#include <salts/thread.h>
#include <salts/clock.h>
#include <salts/error_codes.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static const cmeta_type_desc book_monitor_status_type = {
    "cmeta_status", sizeof(cmeta_status), CMETA_ALIGNOF(cmeta_status),
    CMETA_T_INTEGER, NULL, NULL, NULL
};

#define BOOK_MONITOR_METHODS(X, I) \
    X(I, FR1, cmeta_status, put, stateful, \
      &book_monitor_status_type, CMETA_ABI_ENUM, CMETA_RESULT_VALUE, \
      (int, value, CMETA_PARAM_IN, &cmeta_type_int, CMETA_ABI_SCALAR)) \
    X(I, FR1, cmeta_status, take, stateful, \
      &book_monitor_status_type, CMETA_ABI_ENUM, CMETA_RESULT_VALUE, \
      (int *, out, CMETA_PARAM_OUT | CMETA_PARAM_BORROWED, \
       &cmeta_type_int_ptr, CMETA_ABI_OBJECT_POINTER)) \
    X(I, FV0, void, close, stateful, \
      &cmeta_type_void, CMETA_ABI_VOID)

CMETA_INTERFACE(book_ace_monitor_port, BOOK_MONITOR_METHODS);

typedef struct book_monitor {
    cmeta_mutex_t mutex;
    cmeta_cond_t changed;
    bool full;
    bool closed;
    unsigned waiters;
    int stored_value;
} book_monitor;

static void book_monitor_init(book_monitor *monitor)
{
    *monitor = (book_monitor){0};
    cmeta_mutex_init(&monitor->mutex);
    cmeta_cond_init(&monitor->changed);
}

static void book_monitor_destroy(book_monitor *monitor)
{
    /* Caller must close and join ALL borrowed consumers/producers first. */
    cmeta_cond_destroy(&monitor->changed);
    cmeta_mutex_destroy(&monitor->mutex);
}

static int book_monitor_wait(book_monitor *monitor)
{
    int rc;
    /* Mutex must be held before this call. Condition-wait atomically
     * releases and reacquires it, so each predicate is checked in a loop.
     * A hard deadline makes a missed wake a failed test, not an endless hang.
     */
    ++monitor->waiters;
    rc = cmeta_cond_timedwait(
        &monitor->changed, &monitor->mutex, UINT64_C(5000000000));
    --monitor->waiters;
    return rc;
}

static cmeta_status book_monitor_put(void *self, int value)
{
    book_monitor *monitor = (book_monitor *)self;
    cmeta_mutex_lock(&monitor->mutex);
    while (monitor->full && !monitor->closed) {
        if (book_monitor_wait(monitor) != 0) {
            cmeta_mutex_unlock(&monitor->mutex);
            return CMETA_BUSY;
        }
    }
    if (monitor->closed) {
        cmeta_mutex_unlock(&monitor->mutex);
        return CMETA_BUSY;
    }
    monitor->stored_value = value;
    monitor->full = true;
    cmeta_cond_broadcast(&monitor->changed);
    cmeta_mutex_unlock(&monitor->mutex);
    return CMETA_OK;
}

static cmeta_status book_monitor_take(void *self, int *out)
{
    book_monitor *monitor = (book_monitor *)self;
    if (out == NULL)
        return CMETA_INVALID_ARGUMENT; /* fail before any side effect */
    cmeta_mutex_lock(&monitor->mutex);
    while (!monitor->full && !monitor->closed) {
        if (book_monitor_wait(monitor) != 0) {
            cmeta_mutex_unlock(&monitor->mutex);
            return CMETA_BUSY; /* preserve caller's OUT on timeout */
        }
    }
    if (!monitor->full) {
        cmeta_mutex_unlock(&monitor->mutex);
        return CMETA_BUSY; /* closed and drained; preserve OUT */
    }
    *out = monitor->stored_value;
    monitor->full = false;
    cmeta_cond_broadcast(&monitor->changed);
    cmeta_mutex_unlock(&monitor->mutex);
    return CMETA_OK; /* close still permits draining one committed item */
}

static void book_monitor_close(void *self)
{
    book_monitor *monitor = (book_monitor *)self;
    cmeta_mutex_lock(&monitor->mutex);
    monitor->closed = true;
    cmeta_cond_broadcast(&monitor->changed);
    cmeta_mutex_unlock(&monitor->mutex);
}

CMETA_IMPLEMENTS(book_ace_monitor_port, book_monitor_impl, 0u,
    .put = book_monitor_put,
    .take = book_monitor_take,
    .close = book_monitor_close);

typedef struct book_wait_worker {
    book_ace_monitor_port port; /* borrowed from a living monitor */
    cmeta_status status;
    bool producer;
    int value;
} book_wait_worker;

static void book_wait_worker_run(void *arg)
{
    book_wait_worker *worker = (book_wait_worker *)arg;
    if (worker->producer)
        worker->status = book_ace_monitor_port_put(
            &worker->port, worker->value);
    else
        worker->status = book_ace_monitor_port_take(
            &worker->port, &worker->value);
}

static bool book_monitor_has_waiter(book_monitor *monitor)
{
    bool blocked;
    cmeta_mutex_lock(&monitor->mutex);
    blocked = monitor->waiters != 0u;
    cmeta_mutex_unlock(&monitor->mutex);
    return blocked;
}

static bool book_wait_for_blocked_worker(book_monitor *monitor)
{
    uint64_t until = cmeta_monotonic_ms() + UINT64_C(3500);
    while (cmeta_monotonic_ms() < until) {
        if (book_monitor_has_waiter(monitor))
            return true;
        cmeta_sleep_ms(1u);
    }
    return false;
}

#define BOOK_REQUIRE(expr, code) do {                                  \
    if (!(expr)) {                                                     \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);    \
        result = (code);                                               \
        goto cleanup;                                                  \
    }                                                                 \
} while (0)

static int book_monitor_application(void)
{
    book_monitor monitor = {0};
    book_ace_monitor_port port;
    book_wait_worker worker = {0};
    cmeta_thread_t thread = NULL;
    bool thread_live = false;
    int out = -71, result = 0;

    book_monitor_init(&monitor);
    port = book_monitor_impl_as_book_ace_monitor_port(&monitor);
    BOOK_REQUIRE(cmeta_interface_desc_valid(
        book_ace_monitor_port_interface()), 1);
    BOOK_REQUIRE(book_ace_monitor_port_interface()->method_count == 3u, 2);
    BOOK_REQUIRE(cmeta_interface_method_reflection_valid(
        &book_ace_monitor_port_interface()->methods[0]), 3);
    BOOK_REQUIRE(cmeta_interface_method_reflection_valid(
        &book_ace_monitor_port_interface()->methods[1]), 4);
    BOOK_REQUIRE(cmeta_interface_method_reflection_valid(
        &book_ace_monitor_port_interface()->methods[2]), 5);
    BOOK_REQUIRE(book_ace_monitor_port_valid(&port), 6);
    BOOK_REQUIRE(book_ace_monitor_port_take(&port, NULL) ==
                 CMETA_INVALID_ARGUMENT, 7);

    /* Empty: consumer must actually block on Platform condition variable. */
    worker = (book_wait_worker){port, CMETA_INVALID_ARGUMENT, false, -81};
    BOOK_REQUIRE(cmeta_thread_create(&thread, book_wait_worker_run,
                                      &worker) == SALTS_OK, 8);
    thread_live = true;
    BOOK_REQUIRE(book_wait_for_blocked_worker(&monitor), 9);
    BOOK_REQUIRE(book_ace_monitor_port_put(&port, 7) == CMETA_OK, 10);
    BOOK_REQUIRE(cmeta_thread_join(&thread) == SALTS_OK, 11);
    cmeta_thread_destroy(&thread);
    thread_live = false;
    BOOK_REQUIRE(worker.status == CMETA_OK && worker.value == 7, 12);

    /* Full: producer must block until the one capacity credit is returned. */
    BOOK_REQUIRE(book_ace_monitor_port_put(&port, 13) == CMETA_OK, 13);
    worker = (book_wait_worker){port, CMETA_INVALID_ARGUMENT, true, 29};
    BOOK_REQUIRE(cmeta_thread_create(&thread, book_wait_worker_run,
                                      &worker) == SALTS_OK, 14);
    thread_live = true;
    BOOK_REQUIRE(book_wait_for_blocked_worker(&monitor), 15);
    BOOK_REQUIRE(book_ace_monitor_port_take(&port, &out) == CMETA_OK, 16);
    BOOK_REQUIRE(out == 13, 17);
    BOOK_REQUIRE(cmeta_thread_join(&thread) == SALTS_OK, 18);
    cmeta_thread_destroy(&thread);
    thread_live = false;
    BOOK_REQUIRE(worker.status == CMETA_OK, 19);
    BOOK_REQUIRE(book_ace_monitor_port_take(&port, &out) == CMETA_OK, 20);
    BOOK_REQUIRE(out == 29, 21);

    /* Close broadcasts to an already-blocked consumer and forbids writes. */
    worker = (book_wait_worker){port, CMETA_INVALID_ARGUMENT, false, -91};
    BOOK_REQUIRE(cmeta_thread_create(&thread, book_wait_worker_run,
                                      &worker) == SALTS_OK, 22);
    thread_live = true;
    BOOK_REQUIRE(book_wait_for_blocked_worker(&monitor), 23);
    book_ace_monitor_port_close(&port);
    BOOK_REQUIRE(cmeta_thread_join(&thread) == SALTS_OK, 24);
    cmeta_thread_destroy(&thread);
    thread_live = false;
    BOOK_REQUIRE(worker.status == CMETA_BUSY && worker.value == -91, 25);
    BOOK_REQUIRE(book_ace_monitor_port_put(&port, 31) == CMETA_BUSY, 26);
    out = -77;
    BOOK_REQUIRE(book_ace_monitor_port_take(&port, &out) == CMETA_BUSY, 27);
    BOOK_REQUIRE(out == -77, 28);

cleanup:
    /* Even a failed qualification wakes a blocked worker before join; neither
     * native mutex nor condition storage is destroyed under a live borrower.
     */
    book_ace_monitor_port_close(&port);
    if (thread_live) {
        if (cmeta_thread_join(&thread) != SALTS_OK && result == 0)
            result = 29;
        cmeta_thread_destroy(&thread);
    }
    book_monitor_destroy(&monitor);
    return result;
}

static int book_monitor_drain_after_close(void)
{
    book_monitor monitor = {0};
    int value = -4;
    book_monitor_init(&monitor);
    book_ace_monitor_port port =
        book_monitor_impl_as_book_ace_monitor_port(&monitor);
    cmeta_status admitted = book_ace_monitor_port_put(&port, 41);
    book_ace_monitor_port_close(&port);
    cmeta_status consumed = book_ace_monitor_port_take(&port, &value);
    int next = -17;
    cmeta_status drained = book_ace_monitor_port_take(&port, &next);
    book_monitor_destroy(&monitor);
    return admitted == CMETA_OK && consumed == CMETA_OK && value == 41 &&
           drained == CMETA_BUSY && next == -17 ? 0 : 1;
}

/* Thread-Specific Storage is true Platform TLS, not a shared Local merely
 * tagged with an owner token; each worker gets a distinct per-thread value.
 */
static SALTS_THREAD_LOCAL int book_tls_value;
static int book_tls_nested_add(int n)
{
    book_tls_value += n;
    return book_tls_value;
}
typedef struct book_tls_probe {
    int seed;
    int initial;
    int nested;
    int final;
    const void *token;
} book_tls_probe;

static void book_tls_run(void *arg)
{
    book_tls_probe *probe = (book_tls_probe *)arg;
    probe->initial = book_tls_value;
    probe->token = cmeta_thread_current_token();
    book_tls_value = probe->seed;
    probe->nested = book_tls_nested_add(2);
    probe->final = book_tls_value;
    book_tls_value = 0; /* explicit worker-local lifecycle */
}

static int book_tls_application(void)
{
    cmeta_thread_t threads[2] = {NULL, NULL};
    book_tls_probe probes[2] = {
        {11, -1, -1, -1, NULL},
        {29, -1, -1, -1, NULL}
    };
    const void *const main_owner = cmeta_thread_current_token();
    unsigned created = 0u;
    int result = 0;

    book_tls_value = 101;
    for (size_t i = 0u; i < 2u; ++i) {
        if (cmeta_thread_create(&threads[i], book_tls_run,
                                &probes[i]) != SALTS_OK) {
            result = 1;
            break;
        }
        ++created;
    }
    for (size_t i = 0u; i < created; ++i) {
        if (cmeta_thread_join(&threads[i]) != SALTS_OK)
            result = 2;
        cmeta_thread_destroy(&threads[i]);
        if (probes[i].initial != 0 ||
            probes[i].nested != probes[i].seed + 2 ||
            probes[i].final != probes[i].seed + 2 ||
            probes[i].token == NULL || probes[i].token == main_owner)
            result = 3;
    }
    if (book_tls_value != 101 || book_tls_nested_add(3) != 104)
        result = 4;
    book_tls_value = 0;
    return result;
}

int main(void)
{
    int stage = book_monitor_application();
    if (stage != 0) {
        fprintf(stderr, "CMeta ACE Monitor gate failed at %d\n", stage);
        return stage;
    }
    stage = book_monitor_drain_after_close();
    if (stage != 0) {
        fprintf(stderr, "CMeta ACE closed Monitor drain failed\n");
        return 30 + stage;
    }
    stage = book_tls_application();
    if (stage != 0) {
        fprintf(stderr, "Platform TLS isolation stage failed at %d\n", stage);
        return 40 + stage;
    }
    return 0;
}
