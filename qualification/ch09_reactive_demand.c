#include <cflow/publishers.h>
#include <cflow/reactive.h>
#include <cflow/scheduler.h>

#include <stddef.h>

typedef struct book_sink_state {
    int values[4];
    size_t count;
    size_t done_calls;
    size_t error_calls;
} book_sink_state;

static bool book_on_value(
    void *user,
    const cmeta_type_desc *type,
    const void *value)
{
    book_sink_state *state = (book_sink_state *)user;

    if (state == NULL ||
        value == NULL ||
        !cmeta_type_equal(type, &cmeta_type_int) ||
        state->count >= sizeof(state->values) / sizeof(state->values[0]))
        return false;

    state->values[state->count++] = *(const int *)value;
    return true;
}

static void book_on_error(void *user, const char *message)
{
    book_sink_state *state = (book_sink_state *)user;
    (void)message;

    if (state != NULL)
        ++state->error_calls;
}

static void book_on_done(void *user)
{
    book_sink_state *state = (book_sink_state *)user;

    if (state != NULL)
        ++state->done_calls;
}

int main(void)
{
    const int input[] = { 11, 22, 33 };
    cflow_graph graph = {0};
    cflow_publisher publisher = {0};
    cflow_scheduler scheduler = {0};
    cflow_subscription subscription = {0};
    book_sink_state state = {0};
    cflow_subscriber_callbacks callbacks = {
        book_on_value,
        book_on_error,
        book_on_done,
        &state
    };
    cflow_subscriber subscriber =
        cflow_subscriber_from_callbacks(&callbacks);
    cflow_status_result request;
    size_t callbacks_before_terminal_recheck;

    cflow_graph_init(&graph, &cmeta_type_int);
    if (graph.error != NULL)
        return 1;

    if (!cflow_publisher_from_array(
            &publisher,
            &cmeta_type_int,
            input,
            sizeof(input) / sizeof(input[0])))
        return 2;

    if (!cflow_scheduler_test_init(&scheduler)) {
        cflow_publisher_destroy(&publisher);
        cflow_graph_destroy(&graph);
        return 3;
    }

    if (!cflow_subscribe(
            &subscription,
            &graph,
            &publisher,
            &scheduler,
            &subscriber)) {
        cflow_scheduler_destroy(&scheduler);
        cflow_publisher_destroy(&publisher);
        cflow_graph_destroy(&graph);
        return 4;
    }

    request = cflow_subscription_request_result(
        &subscription,
        1u);
    if (request.status != CFLOW_STATUS_OK)
        return 5;

    if (cflow_subscription_outstanding_demand(
            &subscription) != 1u)
        return 6;

    (void)cflow_scheduler_run_until_idle(&scheduler, 0u);

    if (state.count != 1u ||
        state.values[0] != 11 ||
        state.done_calls != 0u ||
        state.error_calls != 0u)
        return 7;

    if (cflow_subscription_outstanding_demand(
            &subscription) != 0u)
        return 8;

    if (cflow_subscription_is_done(&subscription))
        return 9;

    request = cflow_subscription_request_result(
        &subscription,
        2u);
    if (request.status != CFLOW_STATUS_OK)
        return 10;

    if (cflow_subscription_outstanding_demand(
            &subscription) != 2u)
        return 11;

    (void)cflow_scheduler_run_until_idle(&scheduler, 0u);

    if (state.count != 3u ||
        state.values[0] != 11 ||
        state.values[1] != 22 ||
        state.values[2] != 33)
        return 12;

    if (state.done_calls != 1u ||
        state.error_calls != 0u)
        return 13;

    if (!cflow_subscription_is_done(&subscription))
        return 14;

    if (cflow_subscription_outstanding_demand(
            &subscription) != 0u)
        return 15;

    callbacks_before_terminal_recheck =
        state.count + state.done_calls + state.error_calls;

    request = cflow_subscription_request_result(
        &subscription,
        1u);
    if (request.status != CFLOW_STATUS_CLOSED)
        return 16;

    (void)cflow_scheduler_run_until_idle(&scheduler, 0u);

    if (state.count + state.done_calls + state.error_calls !=
        callbacks_before_terminal_recheck)
        return 17;

    if (cflow_subscription_outstanding_demand(
            &subscription) != 0u)
        return 18;

    cflow_subscription_close(&subscription);
    cflow_scheduler_destroy(&scheduler);
    cflow_graph_destroy(&graph);
    return 0;
}
