#include <cflow/machine_instance.h>

#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct book_machine_probe {
    atomic_int guards;
    atomic_int actions;
    atomic_int wakes;
} book_machine_probe;

static bool book_guard_enabled(
    void *user,
    const void *state,
    const void *event,
    bool *out_enabled,
    const char **out_error)
{
    book_machine_probe *probe = (book_machine_probe *)user;
    (void)state;
    (void)event;

    if (out_enabled == NULL || out_error == NULL)
        return false;

    *out_enabled = true;
    *out_error = NULL;

    if (probe != NULL)
        atomic_fetch_add(&probe->guards, 1);

    return true;
}

static bool book_action_success(
    void *user,
    const void *state,
    const void *event,
    void *out_target_state,
    void *out_observation,
    const char **out_error)
{
    book_machine_probe *probe = (book_machine_probe *)user;
    (void)event;

    if (state == NULL ||
        out_target_state == NULL ||
        out_observation == NULL ||
        out_error == NULL)
        return false;

    *(long *)out_target_state =
        (long)*(const int *)state + 1L;
    *(long *)out_observation = 70L;
    *out_error = NULL;

    if (probe != NULL)
        atomic_fetch_add(&probe->actions, 1);

    return true;
}

static bool book_action_fail(
    void *user,
    const void *state,
    const void *event,
    void *out_target_state,
    void *out_observation,
    const char **out_error)
{
    book_machine_probe *probe = (book_machine_probe *)user;
    (void)state;
    (void)event;
    (void)out_target_state;
    (void)out_observation;

    if (out_error == NULL)
        return false;

    *out_error = "book action failure";

    if (probe != NULL)
        atomic_fetch_add(&probe->actions, 1);

    return false;
}

static void book_wake(void *user)
{
    book_machine_probe *probe = (book_machine_probe *)user;

    if (probe != NULL)
        atomic_fetch_add(&probe->wakes, 1);
}

static cflow_machine_definition book_definition(
    cflow_machine_state *states,
    cflow_event_type *events,
    cflow_machine_guard *guards,
    cflow_machine_action *actions,
    cflow_machine_transition *transitions)
{
    states[0] = (cflow_machine_state){
        10u,
        &cmeta_type_int,
        CFLOW_MACHINE_STATE_ACTIVE
    };
    states[1] = (cflow_machine_state){
        20u,
        &cmeta_type_long,
        CFLOW_MACHINE_STATE_DONE
    };

    events[0] = (cflow_event_type){
        100u,
        &cmeta_type_bool
    };

    guards[0] = (cflow_machine_guard){
        200u,
        &cmeta_type_int,
        100u,
        &cmeta_type_bool,
        CMETA_EFFECT_PURE,
        CMETA_PROP_STABLE | CMETA_PROP_NO_ALIAS
    };

    actions[0] = (cflow_machine_action){
        300u,
        &cmeta_type_int,
        100u,
        &cmeta_type_bool,
        &cmeta_type_long,
        CMETA_EFFECT_MAY_FAIL,
        CMETA_PROP_DETERMINISTIC | CMETA_PROP_NO_ALIAS,
        CFLOW_MACHINE_ACTION_VALUE,
        &cmeta_type_long,
        0u
    };

    transitions[0] = (cflow_machine_transition){
        10u,
        100u,
        200u,
        300u,
        20u,
        7u
    };

    return (cflow_machine_definition){
        states,
        2u,
        10u,
        events,
        1u,
        guards,
        1u,
        actions,
        1u,
        transitions,
        1u
    };
}

static void book_destroy_resumable(cflow_resumable *resumable)
{
    if (resumable == NULL)
        return;

    if (resumable->ops != NULL &&
        resumable->ops->destroy != NULL)
        resumable->ops->destroy(resumable->state);

    *resumable = (cflow_resumable){0};
}

static int book_run_failure(
    const cflow_machine *machine,
    cflow_executor *executor)
{
    cflow_machine_instance instance = {0};
    cflow_resumable resumable = {0};
    book_machine_probe probe;
    const cflow_machine_guard_binding guards[] = {
        {200u, book_guard_enabled, &probe}
    };
    const cflow_machine_action_binding actions[] = {
        {300u, book_action_fail, &probe}
    };
    const int initial = 7;
    const bool payload = true;
    const cflow_event_view event = {
        100u,
        &cmeta_type_bool,
        &payload
    };
    const cflow_machine_instance_config config = {
        machine,
        &initial,
        &cmeta_type_long,
        guards,
        1u,
        actions,
        1u,
        4u,
        executor
    };
    cflow_publish_context context = {0};
    cflow_step step;
    cflow_machine_instance_stats stats = {0};
    const cmeta_type_desc *state_type = NULL;
    int state_value = -1;
    long output = -1L;
    int rc = 0;

    atomic_init(&probe.guards, 0);
    atomic_init(&probe.actions, 0);
    atomic_init(&probe.wakes, 0);

    if (cflow_machine_instance_init(
            &instance,
            &config) != CFLOW_MACHINE_INSTANCE_OK)
        return 20;

    if (!cflow_machine_instance_as_resumable(
            &instance,
            &resumable)) {
        rc = 21;
        goto done;
    }

    if (cflow_machine_instance_try_send(
            &instance,
            &event) != CFLOW_MAILBOX_OK) {
        rc = 22;
        goto done;
    }

    step = resumable.ops->resume(
        resumable.state,
        &context,
        &output);
    if (step.kind != CFLOW_STEP_WAIT) {
        rc = 23;
        goto done;
    }

    if (!cflow_waitable_arm(
            &step.waitable,
            (cflow_waker){book_wake, &probe})) {
        rc = 24;
        goto done;
    }

    if (!cflow_executor_wait_idle(executor)) {
        rc = 25;
        goto done;
    }

    step = resumable.ops->resume(
        resumable.state,
        &context,
        &output);

    if (step.kind != CFLOW_STEP_ERROR ||
        step.error == NULL ||
        strcmp(step.error, "book action failure") != 0) {
        rc = 26;
        goto done;
    }

    if (cflow_machine_instance_current_state(
            &instance) != 10u) {
        rc = 27;
        goto done;
    }

    if (!cflow_machine_instance_copy_state(
            &instance,
            &state_type,
            &state_value,
            sizeof(state_value)) ||
        !cmeta_type_equal(state_type, &cmeta_type_int) ||
        state_value != 7) {
        rc = 28;
        goto done;
    }

    if (atomic_load(&probe.guards) != 1 ||
        atomic_load(&probe.actions) != 1) {
        rc = 29;
        goto done;
    }

    if (!cflow_machine_instance_get_stats(
            &instance,
            &stats) ||
        stats.accepted != 1u ||
        stats.failed != 1u ||
        stats.completed != 0u ||
        stats.current_state != 10u) {
        rc = 30;
        goto done;
    }

done:
    book_destroy_resumable(&resumable);
    cflow_machine_instance_destroy(&instance);
    return rc;
}

static int book_run_success(
    const cflow_machine *machine,
    cflow_executor *executor)
{
    cflow_machine_instance instance = {0};
    cflow_resumable resumable = {0};
    book_machine_probe probe;
    const cflow_machine_guard_binding guards[] = {
        {200u, book_guard_enabled, &probe}
    };
    const cflow_machine_action_binding actions[] = {
        {300u, book_action_success, &probe}
    };
    const int initial = 7;
    const bool payload = true;
    const cflow_event_view event = {
        100u,
        &cmeta_type_bool,
        &payload
    };
    const cflow_machine_instance_config config = {
        machine,
        &initial,
        &cmeta_type_long,
        guards,
        1u,
        actions,
        1u,
        4u,
        executor
    };
    cflow_publish_context context = {0};
    cflow_step step;
    cflow_machine_instance_stats stats = {0};
    const cmeta_type_desc *state_type = NULL;
    long state_value = -1L;
    long output = -1L;
    int actions_before_terminal_send;
    int rc = 0;

    atomic_init(&probe.guards, 0);
    atomic_init(&probe.actions, 0);
    atomic_init(&probe.wakes, 0);

    if (cflow_machine_instance_init(
            &instance,
            &config) != CFLOW_MACHINE_INSTANCE_OK)
        return 40;

    if (!cflow_machine_instance_as_resumable(
            &instance,
            &resumable)) {
        rc = 41;
        goto done;
    }

    if (cflow_machine_instance_try_send(
            &instance,
            &event) != CFLOW_MAILBOX_OK) {
        rc = 42;
        goto done;
    }

    step = resumable.ops->resume(
        resumable.state,
        &context,
        &output);
    if (step.kind != CFLOW_STEP_WAIT) {
        rc = 43;
        goto done;
    }

    if (!cflow_waitable_arm(
            &step.waitable,
            (cflow_waker){book_wake, &probe})) {
        rc = 44;
        goto done;
    }

    if (!cflow_executor_wait_idle(executor)) {
        rc = 45;
        goto done;
    }

    step = resumable.ops->resume(
        resumable.state,
        &context,
        &output);

    if (step.kind != CFLOW_STEP_VALUE_AND_DONE ||
        output != 70L) {
        rc = 46;
        goto done;
    }

    if (cflow_machine_instance_current_state(
            &instance) != 20u) {
        rc = 47;
        goto done;
    }

    if (!cflow_machine_instance_copy_state(
            &instance,
            &state_type,
            &state_value,
            sizeof(state_value)) ||
        !cmeta_type_equal(state_type, &cmeta_type_long) ||
        state_value != 8L) {
        rc = 48;
        goto done;
    }

    if (atomic_load(&probe.guards) != 1 ||
        atomic_load(&probe.actions) != 1) {
        rc = 49;
        goto done;
    }

    if (!cflow_machine_instance_get_stats(
            &instance,
            &stats) ||
        stats.accepted != 1u ||
        stats.completed != 1u ||
        stats.failed != 0u ||
        stats.current_state != 20u ||
        !stats.done) {
        rc = 50;
        goto done;
    }

    actions_before_terminal_send =
        atomic_load(&probe.actions);

    /*
     * A transition into DONE commits state, marks the instance terminal, and
     * cancels the instance-owned mailbox. Producer admission therefore
     * observes CANCELLED rather than running another small step.
     */
    if (cflow_machine_instance_try_send(
            &instance,
            &event) != CFLOW_MAILBOX_CANCELLED) {
        rc = 51;
        goto done;
    }

    if (atomic_load(&probe.actions) !=
        actions_before_terminal_send ||
        cflow_machine_instance_current_state(&instance) != 20u) {
        rc = 52;
        goto done;
    }

    state_value = -1L;
    if (!cflow_machine_instance_copy_state(
            &instance,
            &state_type,
            &state_value,
            sizeof(state_value)) ||
        state_value != 8L) {
        rc = 53;
        goto done;
    }

done:
    book_destroy_resumable(&resumable);
    cflow_machine_instance_destroy(&instance);
    return rc;
}

int main(void)
{
    cflow_machine_state states[2];
    cflow_event_type events[1];
    cflow_machine_guard guards[1];
    cflow_machine_action actions[1];
    cflow_machine_transition transitions[1];
    cflow_machine_definition definition =
        book_definition(
            states,
            events,
            guards,
            actions,
            transitions);
    cflow_machine machine = {0};
    cflow_executor executor = {0};
    int rc;

    if (cflow_machine_build(
            &machine,
            &definition) != CFLOW_MACHINE_OK)
        return 1;

    if (!cflow_executor_serial_init(&executor)) {
        cflow_machine_destroy(&machine);
        return 2;
    }

    rc = book_run_failure(
        &machine,
        &executor);

    if (rc == 0)
        rc = book_run_success(
            &machine,
            &executor);

    cflow_executor_destroy(&executor);
    cflow_machine_destroy(&machine);

    if (rc != 0)
        fprintf(stderr, "machine staged commit gate rc=%d\n", rc);

    return rc;
}
