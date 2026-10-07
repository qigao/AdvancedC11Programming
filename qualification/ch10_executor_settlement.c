#include <cflow/executor.h>

#include <stddef.h>

typedef struct book_task_state {
    size_t run_calls;
    size_t cancel_calls;
    size_t finalize_calls;
} book_task_state;

static void book_task_run(void *user)
{
    book_task_state *state = (book_task_state *)user;

    if (state != NULL)
        ++state->run_calls;
}

static void book_task_cancel(void *user)
{
    book_task_state *state = (book_task_state *)user;

    if (state != NULL)
        ++state->cancel_calls;
}

static void book_task_finalize(void *user)
{
    book_task_state *state = (book_task_state *)user;

    if (state != NULL)
        ++state->finalize_calls;
}

static int book_conserved(
    const cflow_executor_protocol_stats *stats)
{
    return stats != NULL &&
        stats->accepted ==
            stats->queued +
            stats->running +
            stats->completed +
            stats->cancelled;
}

int main(void)
{
    cflow_executor executor = {0};
    cflow_executor_control control = {0};
    cflow_executor_protocol_stats stats = {0};
    book_task_state accepted_state = {0};
    book_task_state rejected_full_state = {0};
    book_task_state rejected_closed_state = {0};
    cflow_executor_task accepted_task = {
        .run = book_task_run,
        .cancel = book_task_cancel,
        .finalize = book_task_finalize,
        .user = &accepted_state
    };
    cflow_executor_task rejected_full_task = {
        .run = book_task_run,
        .cancel = book_task_cancel,
        .finalize = book_task_finalize,
        .user = &rejected_full_state
    };
    cflow_executor_task rejected_closed_task = {
        .run = book_task_run,
        .cancel = book_task_cancel,
        .finalize = book_task_finalize,
        .user = &rejected_closed_state
    };
    cflow_admission_status admission;

    if (!cflow_executor_manual_init_with_capacity(
            &executor,
            1u))
        return 1;

    if (!cflow_executor_as_control(
            &executor,
            &control)) {
        cflow_executor_destroy(&executor);
        return 2;
    }

    admission = cflow_executor_try_post_task(
        &executor,
        &accepted_task);
    if (admission != CFLOW_ADMISSION_ACCEPTED)
        return 3;

    admission = cflow_executor_try_post_task(
        &executor,
        &rejected_full_task);
    if (admission != CFLOW_ADMISSION_FULL)
        return 4;

    if (rejected_full_state.run_calls != 0u ||
        rejected_full_state.cancel_calls != 0u ||
        rejected_full_state.finalize_calls != 0u)
        return 5;

    if (!cflow_executor_control_get_stats(
            &control,
            &stats))
        return 6;

    if (stats.capacity != 1u ||
        stats.accepted != 1u ||
        stats.queued != 1u ||
        stats.running != 0u ||
        stats.completed != 0u ||
        stats.cancelled != 0u ||
        stats.rejected_full != 1u ||
        !book_conserved(&stats))
        return 7;

    if (!cflow_executor_run_one(&executor))
        return 8;

    if (accepted_state.run_calls != 1u ||
        accepted_state.cancel_calls != 0u ||
        accepted_state.finalize_calls != 1u)
        return 9;

    if (!cflow_executor_control_get_stats(
            &control,
            &stats))
        return 10;

    if (stats.accepted != 1u ||
        stats.queued != 0u ||
        stats.running != 0u ||
        stats.completed != 1u ||
        stats.cancelled != 0u ||
        !book_conserved(&stats))
        return 11;

    if (!cflow_executor_control_shutdown(
            &control,
            CFLOW_EXECUTOR_SHUTDOWN_DRAIN))
        return 12;

    if (cflow_executor_control_wait_idle(
            &control) != CFLOW_EXECUTOR_WAIT_IDLE)
        return 13;

    admission = cflow_executor_try_post_task(
        &executor,
        &rejected_closed_task);
    if (admission != CFLOW_ADMISSION_CLOSED)
        return 14;

    if (rejected_closed_state.run_calls != 0u ||
        rejected_closed_state.cancel_calls != 0u ||
        rejected_closed_state.finalize_calls != 0u)
        return 15;

    if (!cflow_executor_control_get_stats(
            &control,
            &stats))
        return 16;

    if (stats.lifecycle != CFLOW_EXECUTOR_CLOSED ||
        stats.accepted != 1u ||
        stats.completed != 1u ||
        stats.cancelled != 0u ||
        stats.rejected_closed != 1u ||
        !book_conserved(&stats))
        return 17;

    cflow_executor_destroy(&executor);
    return 0;
}
