/*
 * A small but real optional ACE Leader/Followers CPU-event application.
 *
 * This is NOT a NativeIO/CNet Reactor, a replacement for CFlow Actor, or an
 * installed universal Leader/Followers runtime. One application source owns
 * the bounded copied-event queue, Leader election, and worker shutdown.
 * CMeta owns exact reflected CPU handler types; Platform owns real threads,
 * mutex and condition. No source-tree/private SDK headers.
 */
#include <cmeta/interface.h>
#include <cmeta/status.h>
#include <salts/thread.h>
#include <salts/error_codes.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { BOOK_LF_WORKERS = 2, BOOK_LF_CAPACITY = 2, BOOK_LF_EVENT_IDS = 3 };
typedef enum book_role {
    BOOK_FOLLOWER = 0,
    BOOK_LEADER,
    BOOK_PROCESSING,
    BOOK_STOPPED
} book_role;

static const cmeta_type_desc book_lf_status_type = {
    "cmeta_status", sizeof(cmeta_status), CMETA_ALIGNOF(cmeta_status),
    CMETA_T_INTEGER, NULL, NULL, NULL
};

#define BOOK_LF_METHODS(X, I) \
    X(I, FR1, cmeta_status, handle, stateful, \
      &book_lf_status_type, CMETA_ABI_ENUM, CMETA_RESULT_VALUE, \
      (int, event, CMETA_PARAM_IN, &cmeta_type_int, CMETA_ABI_SCALAR))
CMETA_INTERFACE(book_ace_cpu_handler, BOOK_LF_METHODS);

typedef struct book_lf_pool book_lf_pool;
typedef struct book_lf_worker {
    book_lf_pool *pool;
    int id;
    cmeta_thread_t thread;
    book_role role;
} book_lf_worker;

struct book_lf_pool {
    cmeta_mutex_t mutex;
    cmeta_cond_t changed;
    book_ace_cpu_handler handler; /* borrowed until all workers join */
    book_lf_worker workers[BOOK_LF_WORKERS];
    int ring[BOOK_LF_CAPACITY];
    int accepted_order[BOOK_LF_EVENT_IDS];
    int dequeued_order[BOOK_LF_EVENT_IDS];
    unsigned observed[BOOK_LF_EVENT_IDS];
    int successor[BOOK_LF_EVENT_IDS];
    int front, back, queued;
    int active, peak_active;
    int leader, last_leader, ready, started, stopped, joined;
    int accepted, dequeued, settled, rejected, promotions, handoffs;
    int invariants_failed, callback_failed;
    bool closing, paused, first_entered, first_released;
};

static void book_lf_elect_locked(book_lf_pool *pool)
{
    if (pool->leader >= 0)
        return;
    for (int step = 1; step <= BOOK_LF_WORKERS; ++step) {
        const int id = (pool->last_leader + step) % BOOK_LF_WORKERS;
        book_lf_worker *candidate = &pool->workers[id];
        if (candidate->role == BOOK_FOLLOWER) {
            candidate->role = BOOK_LEADER;
            pool->leader = id;
            pool->last_leader = id;
            ++pool->promotions;
            cmeta_cond_broadcast(&pool->changed);
            return;
        }
    }
}

static void book_lf_assert_locked(book_lf_pool *pool)
{
    int count = 0, processing = 0;
    for (int i = 0; i < BOOK_LF_WORKERS; ++i) {
        count += pool->workers[i].role == BOOK_LEADER;
        processing += pool->workers[i].role == BOOK_PROCESSING;
    }
    if (count > 1 || count != (pool->leader >= 0) ||
        (pool->leader >= 0 &&
         pool->workers[pool->leader].role != BOOK_LEADER) ||
        processing != pool->active ||
        pool->queued < 0 || pool->queued > BOOK_LF_CAPACITY ||
        pool->accepted != pool->queued + pool->active + pool->settled ||
        pool->dequeued != pool->active + pool->settled)
        ++pool->invariants_failed;
}

/* Exact typed CMeta callback. Reenter the same mutex: it MUST be invoked
 * outside the Leader election critical section, or this test deadlocks.
 * Event 0 is deliberately held so successor event 1 must finish first.
 */
static cmeta_status book_lf_handle(void *self, int event)
{
    book_lf_pool *pool = (book_lf_pool *)self;
    cmeta_mutex_lock(&pool->mutex);
    if (event < 0 || event >= BOOK_LF_EVENT_IDS) {
        ++pool->callback_failed;
        cmeta_mutex_unlock(&pool->mutex);
        return CMETA_INVALID_ARGUMENT;
    }
    if (event == 0) {
        pool->first_entered = true;
        cmeta_cond_broadcast(&pool->changed);
        while (!pool->first_released)
            cmeta_cond_wait(&pool->changed, &pool->mutex);
    }
    ++pool->observed[event];
    if (pool->observed[event] != 1u)
        ++pool->callback_failed;
    cmeta_cond_broadcast(&pool->changed);
    cmeta_mutex_unlock(&pool->mutex);
    return CMETA_OK;
}

CMETA_IMPLEMENTS(book_ace_cpu_handler, book_lf_handler_impl, 0u,
                 .handle = book_lf_handle);

static void book_lf_run(void *user)
{
    book_lf_worker *worker = (book_lf_worker *)user;
    book_lf_pool *pool = worker->pool;

    cmeta_mutex_lock(&pool->mutex);
    worker->role = BOOK_FOLLOWER;
    ++pool->ready;
    book_lf_elect_locked(pool);
    book_lf_assert_locked(pool);
    cmeta_cond_broadcast(&pool->changed);

    for (;;) {
        if (pool->closing && pool->queued == 0) {
            if (pool->leader == worker->id)
                pool->leader = -1;
            worker->role = BOOK_STOPPED;
            ++pool->stopped;
            book_lf_elect_locked(pool);
            book_lf_assert_locked(pool);
            cmeta_cond_broadcast(&pool->changed);
            cmeta_mutex_unlock(&pool->mutex);
            return;
        }

        if (worker->role == BOOK_LEADER &&
            pool->queued != 0 && !pool->paused) {
            const int event = pool->ring[pool->front];
            cmeta_status status;
            pool->front = (pool->front + 1) % BOOK_LF_CAPACITY;
            --pool->queued;
            pool->dequeued_order[pool->dequeued++] = event;
            worker->role = BOOK_PROCESSING;
            pool->leader = -1;
            ++pool->active;
            if (pool->active > pool->peak_active)
                pool->peak_active = pool->active;

            /* Hand off FIRST, before this thread executes its handler. */
            book_lf_elect_locked(pool);
            pool->successor[event] = pool->leader;
            if (pool->leader >= 0)
                ++pool->handoffs;
            book_lf_assert_locked(pool);
            cmeta_cond_broadcast(&pool->changed);
            cmeta_mutex_unlock(&pool->mutex);

            status = book_ace_cpu_handler_handle(&pool->handler, event);

            cmeta_mutex_lock(&pool->mutex);
            if (status != CMETA_OK || pool->observed[event] != 1u)
                ++pool->callback_failed;
            ++pool->settled;
            --pool->active;
            worker->role = BOOK_FOLLOWER;
            book_lf_elect_locked(pool);
            book_lf_assert_locked(pool);
            cmeta_cond_broadcast(&pool->changed);
            continue;
        }
        cmeta_cond_wait(&pool->changed, &pool->mutex);
    }
}

static bool book_lf_init(book_lf_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->leader = -1;
    pool->last_leader = -1;
    for (int i = 0; i < BOOK_LF_EVENT_IDS; ++i)
        pool->successor[i] = -1;
    cmeta_mutex_init(&pool->mutex);
    if (pool->mutex == NULL)
        return false;
    cmeta_cond_init(&pool->changed);
    if (pool->changed == NULL) {
        cmeta_mutex_destroy(&pool->mutex);
        return false;
    }
    pool->handler = book_lf_handler_impl_as_book_ace_cpu_handler(pool);
    for (int i = 0; i < BOOK_LF_WORKERS; ++i) {
        pool->workers[i].pool = pool;
        pool->workers[i].id = i;
        if (cmeta_thread_create(&pool->workers[i].thread, book_lf_run,
                                &pool->workers[i]) != SALTS_OK) {
            cmeta_mutex_lock(&pool->mutex);
            pool->closing = true;
            pool->first_released = true;
            cmeta_cond_broadcast(&pool->changed);
            cmeta_mutex_unlock(&pool->mutex);
            for (int j = 0; j < pool->started; ++j) {
                (void)cmeta_thread_join(&pool->workers[j].thread);
                cmeta_thread_destroy(&pool->workers[j].thread);
            }
            cmeta_cond_destroy(&pool->changed);
            cmeta_mutex_destroy(&pool->mutex);
            return false;
        }
        ++pool->started;
    }
    return true;
}

/* Source admission is separate from scheduling: no implicit retry and no
 * ownership transfer on FULL/CLOSED. Called from any submitting thread.
 */
static cmeta_status book_lf_submit(book_lf_pool *pool, int event)
{
    cmeta_status status;
    if (pool == NULL || event < 0 || event >= BOOK_LF_EVENT_IDS)
        return CMETA_INVALID_ARGUMENT;
    cmeta_mutex_lock(&pool->mutex);
    if (pool->closing || pool->queued >= BOOK_LF_CAPACITY) {
        ++pool->rejected;
        status = CMETA_BUSY;
    } else {
        pool->ring[pool->back] = event; /* copy admitted event */
        pool->back = (pool->back + 1) % BOOK_LF_CAPACITY;
        ++pool->queued;
        pool->accepted_order[pool->accepted++] = event;
        status = CMETA_OK;
    }
    book_lf_assert_locked(pool);
    cmeta_cond_broadcast(&pool->changed);
    cmeta_mutex_unlock(&pool->mutex);
    return status;
}

static bool book_lf_wait_for(book_lf_pool *pool, int kind)
{
    bool found = false;
    cmeta_mutex_lock(&pool->mutex);
    for (int n = 0; n < 50; ++n) {
        if (kind == 0)
            found = pool->ready == BOOK_LF_WORKERS && pool->leader >= 0;
        else if (kind == 1)
            found = pool->first_entered &&
                pool->settled >= 1 && pool->observed[1] == 1u &&
                pool->active == 1 && !pool->first_released;
        if (found)
            break;
        (void)cmeta_cond_timedwait(
            &pool->changed, &pool->mutex, UINT64_C(100000000));
    }
    cmeta_mutex_unlock(&pool->mutex);
    return found;
}

static bool book_lf_close_and_join(book_lf_pool *pool)
{
    bool result = true;
    cmeta_mutex_lock(&pool->mutex);
    if (pool->closing) {
        cmeta_mutex_unlock(&pool->mutex);
        return false;
    }
    pool->closing = true;
    pool->paused = false;         /* finish already accepted work */
    pool->first_released = true; /* unblock the test-held handler */
    cmeta_cond_broadcast(&pool->changed);
    cmeta_mutex_unlock(&pool->mutex);

    for (int i = 0; i < pool->started; ++i) {
        if (cmeta_thread_join(&pool->workers[i].thread) != SALTS_OK)
            result = false;
        cmeta_thread_destroy(&pool->workers[i].thread);
    }
    pool->joined = result;
    return result;
}

static bool book_lf_destroy(book_lf_pool *pool)
{
    if (!pool->joined || pool->queued != 0 || pool->active != 0 ||
        pool->stopped != pool->started)
        return false;
    pool->joined = false; /* duplicate destroy cannot release native locks */
    cmeta_cond_destroy(&pool->changed);
    cmeta_mutex_destroy(&pool->mutex);
    return true;
}

#define BOOK_REQUIRE(expr, code) do {                                  \
    if (!(expr)) {                                                     \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);    \
        result = (code);                                               \
        goto cleanup;                                                  \
    }                                                                  \
} while (0)

static int book_lf_baton_application(void)
{
    book_lf_pool pool = {0};
    bool joined = false;
    int result = 0;

    if (!book_lf_init(&pool))
        return 1;
    BOOK_REQUIRE(book_lf_wait_for(&pool, 0), 2);
    BOOK_REQUIRE(cmeta_interface_desc_valid(
        book_ace_cpu_handler_interface()), 3);
    BOOK_REQUIRE(cmeta_interface_method_reflection_valid(
        &book_ace_cpu_handler_interface()->methods[0]), 4);
    BOOK_REQUIRE(book_ace_cpu_handler_valid(&pool.handler), 5);

    cmeta_mutex_lock(&pool.mutex);
    pool.paused = true;
    cmeta_mutex_unlock(&pool.mutex);

    BOOK_REQUIRE(book_lf_submit(&pool, 0) == CMETA_OK, 6);
    BOOK_REQUIRE(book_lf_submit(&pool, 1) == CMETA_OK, 7);
    BOOK_REQUIRE(book_lf_submit(&pool, 2) == CMETA_BUSY, 8);
    cmeta_mutex_lock(&pool.mutex);
    pool.paused = false;
    cmeta_cond_broadcast(&pool.changed);
    cmeta_mutex_unlock(&pool.mutex);

    /* Worker A holds event 0. Its successor must consume/settle event 1
     * without waiting for event 0's handler to return: genuine baton passing.
     */
    BOOK_REQUIRE(book_lf_wait_for(&pool, 1), 9);
    cmeta_mutex_lock(&pool.mutex);
    const bool observed_handoff = pool.successor[0] >= 0 &&
        pool.handoffs >= 1 && pool.settled == 1 &&
        pool.observed[0] == 0u && pool.queued == 0 &&
        pool.accepted == 2 && pool.rejected == 1 &&
        pool.invariants_failed == 0;
    cmeta_mutex_unlock(&pool.mutex);
    BOOK_REQUIRE(observed_handoff, 10);

cleanup:
    joined = book_lf_close_and_join(&pool);
    if (!joined && !result)
        result = 11;
    if (joined) {
        if (pool.accepted != pool.settled || pool.accepted != 2 ||
            pool.dequeued != 2 || pool.queued != 0 || pool.active != 0 ||
            pool.leader != -1 || pool.stopped != pool.started ||
            pool.observed[0] != 1u || pool.observed[1] != 1u ||
            pool.accepted_order[0] != pool.dequeued_order[0] ||
            pool.accepted_order[1] != pool.dequeued_order[1] ||
            pool.callback_failed != 0 || pool.invariants_failed != 0 ||
            book_lf_submit(&pool, 2) != CMETA_BUSY) {
            if (!result)
                result = 12;
        }
        if (!book_lf_destroy(&pool) && !result)
            result = 13;
    }
    return result;
}

static int book_lf_close_empty(void)
{
    book_lf_pool pool = {0};
    int result = 0;
    if (!book_lf_init(&pool))
        return 1;
    if (!book_lf_wait_for(&pool, 0))
        result = 2;
    if (!book_lf_close_and_join(&pool) && !result)
        result = 3;
    if (pool.accepted != 0 || pool.dequeued != 0 ||
        pool.settled != 0 || pool.stopped != BOOK_LF_WORKERS ||
        pool.invariants_failed != 0)
        result = 4;
    if (pool.joined && !book_lf_destroy(&pool) && !result)
        result = 5;
    return result;
}

static int book_lf_close_drains_accepted(void)
{
    book_lf_pool pool = {0};
    int result = 0;
    if (!book_lf_init(&pool))
        return 1;
    if (!book_lf_wait_for(&pool, 0))
        result = 2;
    cmeta_mutex_lock(&pool.mutex);
    pool.paused = true;
    cmeta_mutex_unlock(&pool.mutex);
    if (book_lf_submit(&pool, 1) != CMETA_OK ||
        book_lf_submit(&pool, 2) != CMETA_OK)
        result = 3;
    if (!book_lf_close_and_join(&pool) && !result)
        result = 4;
    if (pool.joined) {
        if (pool.accepted != 2 || pool.dequeued != 2 ||
            pool.settled != 2 || pool.queued != 0 || pool.active != 0 ||
            pool.observed[1] != 1u || pool.observed[2] != 1u ||
            pool.callback_failed != 0 || pool.invariants_failed != 0 ||
            pool.accepted_order[0] != pool.dequeued_order[0] ||
            pool.accepted_order[1] != pool.dequeued_order[1] ||
            book_lf_submit(&pool, 0) != CMETA_BUSY) {
            if (!result)
                result = 5;
        }
        if (!book_lf_destroy(&pool) && !result)
            result = 6;
    }
    return result;
}

int main(void)
{
    int rc = book_lf_baton_application();
    if (rc != 0) {
        fprintf(stderr, "ACE Leader/Followers baton gate failed: %d\n", rc);
        return rc;
    }
    rc = book_lf_close_empty();
    if (rc != 0) {
        fprintf(stderr, "ACE Leader/Followers empty-close gate failed: %d\n", rc);
        return rc;
    }
    rc = book_lf_close_drains_accepted();
    if (rc != 0) {
        fprintf(stderr, "ACE Leader/Followers close/drain gate failed: %d\n", rc);
        return rc;
    }
    return 0;
}
