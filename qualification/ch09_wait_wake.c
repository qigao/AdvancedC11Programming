#include <cflow/publishers.h>
#include <cflow/reactive.h>
#include <cflow/scheduler.h>

#include <stddef.h>

typedef struct book_wait_source {
    cflow_scheduler *scheduler;
    cflow_task_id task;
    cflow_waker waker;
    cflow_waker last_waker;
    int values[2];
    size_t index;
    size_t reads;
    size_t arms;
    size_t cancels;
    int ready;
} book_wait_source;

typedef struct book_wait_sink {
    int values[4];
    size_t count;
    size_t done_calls;
    size_t error_calls;
} book_wait_sink;

static void book_mark_ready(void *user)
{
    book_wait_source *source = (book_wait_source *)user;
    cflow_waker waker;

    source->task = 0u;
    source->ready = 1;
    waker = source->waker;
    source->waker = (cflow_waker){0};

    if (waker.wake != NULL)
        waker.wake(waker.user);
}

static cflow_read_status book_read(
    void *user,
    void *out_value,
    const char **error)
{
    book_wait_source *source = (book_wait_source *)user;
    (void)error;

    ++source->reads;

    if (source->index >= 2u)
        return CFLOW_READ_DONE;

    if (!source->ready)
        return CFLOW_READ_WOULD_BLOCK;

    source->ready = 0;
    *(int *)out_value = source->values[source->index++];

    return source->index == 2u
        ? CFLOW_READ_VALUE_AND_DONE
        : CFLOW_READ_VALUE;
}

static bool book_arm(void *user, cflow_waker waker)
{
    book_wait_source *source = (book_wait_source *)user;

    if (source->task != 0u)
        return false;

    ++source->arms;
    source->waker = waker;
    source->last_waker = waker;
    source->task = cflow_scheduler_post_after(
        source->scheduler,
        3u,
        book_mark_ready,
        source);

    return source->task != 0u;
}

static void book_cancel(void *user)
{
    book_wait_source *source = (book_wait_source *)user;

    ++source->cancels;

    if (source->task != 0u) {
        (void)cflow_scheduler_cancel(
            source->scheduler,
            source->task);
        source->task = 0u;
    }

    source->waker = (cflow_waker){0};
}

static bool book_on_value(
    void *user,
    const cmeta_type_desc *type,
    const void *value)
{
    book_wait_sink *sink = (book_wait_sink *)user;

    if (sink == NULL ||
        value == NULL ||
        !cmeta_type_equal(type, &cmeta_type_int) ||
        sink->count >= sizeof(sink->values) / sizeof(sink->values[0]))
        return false;

    sink->values[sink->count++] = *(const int *)value;
    return true;
}

static void book_on_error(void *user, const char *message)
{
    book_wait_sink *sink = (book_wait_sink *)user;
    (void)message;

    if (sink != NULL)
        ++sink->error_calls;
}

static void book_on_done(void *user)
{
    book_wait_sink *sink = (book_wait_sink *)user;

    if (sink != NULL)
        ++sink->done_calls;
}

int main(void)
{
    cflow_graph graph = {0};
    cflow_scheduler scheduler = {0};
    cflow_publisher publisher = {0};
    cflow_subscription subscription = {0};
    book_wait_source source = {0};
    book_wait_sink sink = {0};
    cflow_subscriber_callbacks callbacks = {
        book_on_value,
        book_on_error,
        book_on_done,
        &sink
    };
    cflow_subscriber subscriber =
        cflow_subscriber_from_callbacks(&callbacks);
    cflow_status_result request;
    size_t callbacks_before_stale_wake;

    source.values[0] = 7;
    source.values[1] = 9;

    cflow_graph_init(&graph, &cmeta_type_int);
    if (graph.error != NULL)
        return 1;

    if (!cflow_scheduler_test_init(&scheduler)) {
        cflow_graph_destroy(&graph);
        return 2;
    }

    source.scheduler = &scheduler;

    if (!cflow_publisher_from_readiness(
            &publisher,
            "book-readiness",
            &cmeta_type_int,
            book_read,
            book_arm,
            book_cancel,
            NULL,
            &source)) {
        cflow_scheduler_destroy(&scheduler);
        cflow_graph_destroy(&graph);
        return 3;
    }

    if (!cflow_subscribe(
            &subscription,
            &graph,
            &publisher,
            &scheduler,
            &subscriber))
        return 4;

    request = cflow_subscription_request_result(
        &subscription,
        1u);
    if (request.status != CFLOW_STATUS_OK)
        return 5;

    (void)cflow_scheduler_run_ready(&scheduler);

    if (sink.count != 0u ||
        source.arms != 1u ||
        cflow_subscription_outstanding_demand(
            &subscription) != 1u)
        return 6;

    (void)cflow_scheduler_advance(&scheduler, 3u);
    (void)cflow_scheduler_run_until_idle(&scheduler, 0u);

    if (sink.count != 1u ||
        sink.values[0] != 7 ||
        sink.done_calls != 0u ||
        sink.error_calls != 0u)
        return 7;

    if (cflow_subscription_outstanding_demand(
            &subscription) != 0u)
        return 8;

    cflow_subscription_wake(&subscription);
    (void)cflow_scheduler_run_until_idle(&scheduler, 0u);

    if (sink.count != 1u ||
        cflow_subscription_outstanding_demand(
            &subscription) != 0u)
        return 9;

    request = cflow_subscription_request_result(
        &subscription,
        1u);
    if (request.status != CFLOW_STATUS_OK)
        return 10;

    (void)cflow_scheduler_run_ready(&scheduler);

    if (source.arms != 2u ||
        cflow_subscription_outstanding_demand(
            &subscription) != 1u)
        return 11;

    (void)cflow_scheduler_advance(&scheduler, 3u);
    (void)cflow_scheduler_run_until_idle(&scheduler, 0u);

    if (sink.count != 2u ||
        sink.values[0] != 7 ||
        sink.values[1] != 9 ||
        sink.done_calls != 1u ||
        sink.error_calls != 0u)
        return 12;

    if (!cflow_subscription_is_done(&subscription) ||
        cflow_subscription_outstanding_demand(
            &subscription) != 0u)
        return 13;

    callbacks_before_stale_wake =
        sink.count + sink.done_calls + sink.error_calls;

    if (source.last_waker.wake != NULL)
        source.last_waker.wake(source.last_waker.user);

    (void)cflow_scheduler_run_until_idle(&scheduler, 0u);

    if (sink.count + sink.done_calls + sink.error_calls !=
        callbacks_before_stale_wake)
        return 14;

    if (!cflow_subscription_is_done(&subscription) ||
        cflow_subscription_outstanding_demand(
            &subscription) != 0u)
        return 15;

    cflow_subscription_close(&subscription);
    cflow_scheduler_destroy(&scheduler);
    cflow_graph_destroy(&graph);
    return 0;
}
