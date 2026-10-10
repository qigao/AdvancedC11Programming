/*
 * Installed Salts SDK, strict C11: CMeta ACE Strategy / Interceptor /
 * Strategized Locking + NativeIO one-shot Proactor completion identity.
 *
 * CMeta generates exact typed, borrowed dispatch. Platform owns mutexes and
 * worker threads. NativeIO owns operation/completion identity and real I/O.
 * No extra Reactor, Actor, silent compatibility layer or implicit allocation.
 */
#include <cmeta/interface.h>
#include <cmeta/ace_interceptor.h>
#include <cmeta/ace_synchronization.h>
#include <salts/native_io_ace_token.h>
#include <salts/thread.h>
#include <salts/error_codes.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define BOOK_ASSERT(cond) do {                                         \
    if (!(cond)) {                                                    \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);    \
        return 1;                                                     \
    }                                                                 \
} while (0)

#define BOOK_STRATEGY_METHODS(X, I) \
    X(I, FR1, int, apply, stateful, \
      &cmeta_type_int, CMETA_ABI_SCALAR, CMETA_RESULT_VALUE, \
      (int, value, CMETA_PARAM_IN, &cmeta_type_int, CMETA_ABI_SCALAR))
CMETA_INTERFACE(book_ace_strategy, BOOK_STRATEGY_METHODS);

typedef struct book_ace_policy {
    int add;
    unsigned calls;
} book_ace_policy;

static int book_apply(void *self, int value)
{
    book_ace_policy *policy = (book_ace_policy *)self;
    ++policy->calls;
    return value + policy->add;
}
CMETA_IMPLEMENTS(book_ace_strategy, book_ace_strategy_impl, 0u,
                 .apply = book_apply);

typedef struct book_ace_trace {
    char events[32];
    size_t used;
    unsigned target_calls;
    bool reject_inner;
} book_ace_trace;
typedef struct book_hook_state {
    book_ace_trace *trace;
    char name;
} book_hook_state;
typedef struct book_target_state {
    book_ace_trace *trace;
    book_ace_strategy *strategy;
} book_target_state;

CMETA_INTERCEPTOR_TYPE(book_ace_interceptor, int, int);

static void book_record(book_ace_trace *trace, char event)
{
    if (trace->used + 1u < sizeof(trace->events)) {
        trace->events[trace->used++] = event;
        trace->events[trace->used] = '\0';
    }
}
static cmeta_status book_before(void *user, const int *request, bool *proceed)
{
    book_hook_state *state = (book_hook_state *)user;
    (void)request;
    book_record(state->trace, state->name);
    *proceed = !(state->name == 'B' && state->trace->reject_inner);
    return CMETA_OK;
}
static void book_after(void *user, const int *request, const int *response)
{
    book_hook_state *state = (book_hook_state *)user;
    (void)request;
    (void)response;
    book_record(state->trace, state->name == 'A' ? 'a' : 'b');
}
static void book_error(void *user, const int *request, cmeta_status reason)
{
    book_hook_state *state = (book_hook_state *)user;
    (void)request;
    (void)reason;
    book_record(state->trace, state->name == 'A' ? '1' : '2');
}
static cmeta_status book_target(void *user, const int *request, int *response)
{
    book_target_state *state = (book_target_state *)user;
    book_record(state->trace, 'T');
    ++state->trace->target_calls;
    *response = book_ace_strategy_apply(state->strategy, *request);
    return CMETA_OK;
}
CMETA_STATIC_ASSERT(
    CMETA_TYPE_MATCHES(&book_target, book_ace_interceptor_target_fn),
    "ACE Interceptor retains the exact native C target signature");

static int book_interceptor_application(void)
{
    book_ace_policy policy = {10, 0u};
    book_ace_strategy strategy =
        book_ace_strategy_impl_as_book_ace_strategy(&policy);
    book_ace_trace trace = {0};
    book_target_state target = {&trace, &strategy};
    book_hook_state outer = {&trace, 'A'}, inner = {&trace, 'B'};
    book_ace_interceptor_hook hooks[2] = {
        {&outer, book_before, book_after, book_error},
        {&inner, book_before, book_after, book_error}
    };
    book_ace_interceptor chain = {&target, book_target, hooks, 2u};
    int request = 5, response = -1;

    BOOK_ASSERT(cmeta_interface_desc_valid(book_ace_strategy_interface()));
    BOOK_ASSERT(book_ace_strategy_valid(&strategy));
    BOOK_ASSERT(book_ace_strategy_interface()->methods[0].abi != NULL);
    BOOK_ASSERT(book_ace_strategy_interface()->methods[0].function != NULL);
    BOOK_ASSERT(book_ace_interceptor_invoke(&chain, &request, &response) ==
                CMETA_OK);
    BOOK_ASSERT(response == 15 && policy.calls == 1u);
    BOOK_ASSERT(trace.target_calls == 1u && strcmp(trace.events, "ABTba") == 0);

    /* Short-circuit does not invoke the target or successful after callbacks.
     * All entered hooks receive exactly one error in reverse order.
     */
    trace.used = 0u;
    trace.events[0] = '\0';
    trace.reject_inner = true;
    response = -123;
    BOOK_ASSERT(book_ace_interceptor_invoke(&chain, &request, &response) ==
                CMETA_CALLBACK_ERROR);
    BOOK_ASSERT(strcmp(trace.events, "AB21") == 0);
    BOOK_ASSERT(trace.target_calls == 1u && policy.calls == 1u);
    BOOK_ASSERT(response == -123);

    /* Validate entire borrowed hook chain before changing anything. */
    {
        book_ace_interceptor_hook malformed = {0};
        trace.used = 0u;
        trace.events[0] = '\0';
        chain.hooks = &malformed;
        chain.hook_count = 1u;
        BOOK_ASSERT(book_ace_interceptor_invoke(&chain, &request, &response) ==
                    CMETA_INVALID_ARGUMENT);
        BOOK_ASSERT(trace.used == 0u && trace.target_calls == 1u);
    }
    return 0;
}

static void book_lock(void *user)
{
    cmeta_mutex_lock((cmeta_mutex_t *)user);
}
static void book_unlock(void *user)
{
    cmeta_mutex_unlock((cmeta_mutex_t *)user);
}
CMETA_IMPLEMENTS(cmeta_ace_lockable, book_mutex_strategy_impl, 0u,
                 .acquire = book_lock, .release = book_unlock);

typedef struct book_counter { int value; } book_counter;
CMETA_ACE_SYNCHRONIZED(book_counter_gate, book_counter);

static cmeta_status book_counter_add(book_counter *counter)
{
    ++counter->value;
    return CMETA_OK;
}

typedef struct book_counter_worker {
    const cmeta_ace_lockable *policy;  /* borrowed until all workers join */
    book_counter *counter;             /* borrowed; one shared guarded state */
    unsigned failures;
} book_counter_worker;

static void book_counter_run(void *user)
{
    book_counter_worker *worker = (book_counter_worker *)user;
    for (int i = 0; i < 100; ++i) {
        if (book_counter_gate_run(worker->policy, worker->counter,
                                  book_counter_add) != CMETA_OK)
            ++worker->failures;
    }
}

static int book_scoped_lock_application(void)
{
    cmeta_mutex_t mutex = NULL;
    cmeta_ace_guard guard = {0};
    cmeta_thread_t workers[4] = {0};
    book_counter_worker ctx[4] = {{0}};
    book_counter counter = {0};
    size_t started = 0u;
    int status = 0;

    cmeta_mutex_init(&mutex);
    cmeta_ace_lockable policy =
        book_mutex_strategy_impl_as_cmeta_ace_lockable(&mutex);
    if (!cmeta_ace_lockable_valid(&policy) ||
        cmeta_ace_guard_enter(&guard, &policy) != CMETA_OK ||
        cmeta_ace_guard_enter(&guard, &policy) != CMETA_BUSY ||
        cmeta_ace_guard_leave(&guard) != CMETA_OK ||
        cmeta_ace_guard_leave(&guard) != CMETA_INVALID_ARGUMENT)
        status = 1;

    if (status == 0) {
        for (size_t i = 0u; i < 4u; ++i) {
            ctx[i].policy = &policy;
            ctx[i].counter = &counter;
            if (cmeta_thread_create(&workers[i], book_counter_run,
                                    &ctx[i]) != SALTS_OK) {
                status = 2;
                break;
            }
            ++started;
        }
    }
    for (size_t i = 0u; i < started; ++i) {
        if (cmeta_thread_join(&workers[i]) != SALTS_OK)
            status = 3;
        cmeta_thread_destroy(&workers[i]);
    }
    for (size_t i = 0u; i < started; ++i)
        if (ctx[i].failures != 0u)
            status = 4;
    if (status == 0 && counter.value != 400)
        status = 5;
    /* Workers no longer borrow the policy or mutex. */
    cmeta_mutex_destroy(&mutex);
    return status;
}

NATIVE_IO_ACE_TOKEN_TYPE(book_ace_completion_token, int);

static int book_proactor_identity(void)
{
    native_io_request request = {1u, 2u};
    native_io_endpoint endpoint = {2u, 3u};
    native_io_completion completion = {0};
    book_ace_completion_token token = {0};
    int caller_owned_state = 31;
    int *settled = NULL;

    completion.request = request;
    completion.endpoint = endpoint;
    completion.kind = NATIVE_IO_COMPLETION_OK;
    completion.user_data = 51u;
    BOOK_ASSERT(book_ace_completion_token_bind(
        &token, request, endpoint, 51u, &caller_owned_state) == SALTS_OK);
    BOOK_ASSERT(book_ace_completion_token_settle(
        &token, &completion, &settled) == SALTS_OK);
    BOOK_ASSERT(settled == &caller_owned_state && *settled == 31);
    settled = NULL;
    BOOK_ASSERT(book_ace_completion_token_settle(
        &token, &completion, &settled) == SALTS_EALREADY);
    BOOK_ASSERT(settled == NULL);
    /* Completion tokens bind ownership; they do not submit or poll native I/O. */
    return 0;
}

int main(void)
{
    int status = book_interceptor_application();
    if (status != 0) return status;
    status = book_scoped_lock_application();
    if (status != 0) return 10 + status;
    status = book_proactor_identity();
    if (status != 0) return 20 + status;
    return 0;
}
