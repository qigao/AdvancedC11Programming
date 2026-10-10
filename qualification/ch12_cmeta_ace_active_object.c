/*
 * Installed Salts SDK, strict C11: ACE Active Object application boundary.
 *
 * A CMeta typed Strategy/Port is borrowed and exact. CFlow owns the Actor,
 * bounded mailbox, producer ref, scheduler, machine and shutdown lifecycle.
 * No CNet instance or second Reactor is introduced.
 */
#include <cflow/actor.h>
#include <cmeta/interface.h>
#include <salts/clock.h>
#include <salts/thread.h>

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define BOOK_ACE_PORT_METHODS(X, I) \
    X(I, FR1, int, send, stateful, \
      &cmeta_type_int, CMETA_ABI_SCALAR, CMETA_RESULT_VALUE, \
      (int, payload, CMETA_PARAM_IN, &cmeta_type_int, CMETA_ABI_SCALAR))

#define BOOK_ACE_WORK_METHODS(X, I) \
    X(I, FR1, int, transform, stateful, \
      &cmeta_type_int, CMETA_ABI_SCALAR, CMETA_RESULT_VALUE, \
      (int, payload, CMETA_PARAM_IN, &cmeta_type_int, CMETA_ABI_SCALAR))

CMETA_INTERFACE(book_ace_actor_port, BOOK_ACE_PORT_METHODS);
CMETA_INTERFACE(book_ace_actor_work, BOOK_ACE_WORK_METHODS);

typedef struct book_ace_results {
    atomic_int transform_calls;
    atomic_int deliveries;
    atomic_int errors;
    atomic_int value;
} book_ace_results;

typedef struct book_ace_machine_context {
    book_ace_actor_work *work; /* borrowed; outlives Actor invocation */
} book_ace_machine_context;

typedef struct book_ace_sender {
    cflow_actor_ref *ref; /* borrowed; actor_ref maintains stale identity */
} book_ace_sender;

static int book_transform(void *self, int payload)
{
    book_ace_results *results = (book_ace_results *)self;
    atomic_fetch_add(&results->transform_calls, 1);
    return payload + 100;
}

static int book_send(void *self, int payload)
{
    book_ace_sender *sender = (book_ace_sender *)self;
    const cflow_event_view event = {100u, &cmeta_type_int, &payload};
    return (int)cflow_actor_ref_try_send(sender->ref, &event);
}

CMETA_IMPLEMENTS(book_ace_actor_work, book_ace_work_impl, 0u,
                 .transform = book_transform);
CMETA_IMPLEMENTS(book_ace_actor_port, book_ace_port_impl, 0u,
                 .send = book_send);

static bool book_action(void *user, const void *state, const void *event,
                        void *out_state, void *out_observation,
                        const char **out_error)
{
    book_ace_machine_context *context = (book_ace_machine_context *)user;
    if (context == NULL || state == NULL || event == NULL ||
        out_state == NULL || out_observation == NULL || out_error == NULL ||
        !book_ace_actor_work_valid(context->work))
        return false;

    *(int *)out_state = *(const int *)state + 1;
    *(int *)out_observation =
        book_ace_actor_work_transform(context->work, *(const int *)event);
    *out_error = NULL;
    return true;
}

static bool book_delivery(void *user, const cmeta_type_desc *type,
                          const void *value)
{
    book_ace_results *results = (book_ace_results *)user;
    if (results == NULL || value == NULL ||
        !cmeta_type_equal(type, &cmeta_type_int))
        return false;
    atomic_store(&results->value, *(const int *)value);
    atomic_fetch_add(&results->deliveries, 1);
    return true;
}

static void book_error(void *user, const char *error)
{
    book_ace_results *results = (book_ace_results *)user;
    if (results != NULL && error != NULL)
        atomic_fetch_add(&results->errors, 1);
}

static void book_done(void *user) { (void)user; }

int main(void)
{
    cflow_machine machine = {0};
    cflow_executor executor = {0};
    cflow_scheduler scheduler = {0};
    cflow_actor actor = {0};
    cflow_actor_ref producer = {0};
    book_ace_results results = {0};
    int initial_state = 0;
    int result = 0;
    const cflow_machine_state states[] = {
        {10u, &cmeta_type_int, CFLOW_MACHINE_STATE_ACTIVE}
    };
    const cflow_event_type events[] = {
        {100u, &cmeta_type_int}
    };
    const cflow_machine_action actions[] = {
        {300u, &cmeta_type_int, 100u, &cmeta_type_int,
         &cmeta_type_int, CMETA_EFFECT_MAY_FAIL,
         CMETA_PROP_DETERMINISTIC | CMETA_PROP_NO_ALIAS,
         CFLOW_MACHINE_ACTION_VALUE, &cmeta_type_int, 0u}
    };
    const cflow_machine_transition transitions[] = {
        {10u, 100u, 0u, 300u, 10u, 1u}
    };
    const cflow_machine_definition definition = {
        states, 1u, 10u, events, 1u,
        NULL, 0u, actions, 1u, transitions, 1u
    };
    book_ace_actor_work work =
        book_ace_work_impl_as_book_ace_actor_work(&results);
    book_ace_machine_context action_context = {&work};
    cflow_machine_action_binding bindings[] = {
        {300u, book_action, &action_context}
    };
    book_ace_sender sender = {&producer};
    book_ace_actor_port port =
        book_ace_port_impl_as_book_ace_actor_port(&sender);
    cflow_actor_config config = {0};
    uint64_t deadline;

    if (!cmeta_interface_desc_valid(book_ace_actor_port_interface()) ||
        !cmeta_interface_desc_valid(book_ace_actor_work_interface()) ||
        book_ace_actor_port_interface()->methods[0].abi == NULL ||
        !book_ace_actor_port_valid(&port) ||
        !book_ace_actor_work_valid(&work)) {
        result = 1; goto finish;
    }
    if (cflow_machine_build(&machine, &definition) != CFLOW_MACHINE_OK) {
        result = 2; goto finish;
    }
    if (!cflow_executor_serial_init(&executor)) {
        result = 3; goto finish;
    }
    if (!cflow_scheduler_worker_init(&scheduler, 1u)) {
        result = 4; goto finish;
    }

    config.machine = (cflow_machine_instance_config) {
        &machine, &initial_state, &cmeta_type_int,
        NULL, 0u, bindings, 1u, 2u, &executor
    };
    config.scheduler = &scheduler;
    config.callbacks = (cflow_subscriber_callbacks) {
        book_delivery, book_error, book_done, &results
    };
    if (cflow_actor_init(&actor, &config).status != CFLOW_ACTOR_OK) {
        result = 5; goto finish;
    }
    if (cflow_actor_start(&actor) != CFLOW_ACTOR_OK ||
        !cflow_actor_ref_acquire(&actor, &producer)) {
        result = 6; goto finish;
    }
    if (book_ace_actor_port_send(&port, 7) !=
        (int)CFLOW_ACTOR_SEND_ACCEPTED) {
        result = 7; goto finish;
    }

    deadline = cmeta_monotonic_ms() + UINT64_C(5000);
    while (atomic_load(&results.deliveries) == 0 &&
           cmeta_monotonic_ms() < deadline)
        cmeta_sleep_ms(1u);
    if (atomic_load(&results.deliveries) != 1 ||
        atomic_load(&results.transform_calls) != 1 ||
        atomic_load(&results.value) != 107 ||
        atomic_load(&results.errors) != 0) {
        result = 8; goto finish;
    }

    if (cflow_actor_request_stop(&actor) != CFLOW_ACTOR_OK ||
        cflow_actor_wait(&actor) != CFLOW_ACTOR_STATE_STOPPED ||
        book_ace_actor_port_send(&port, 8) !=
        (int)CFLOW_ACTOR_SEND_STOPPED) {
        result = 9; goto finish;
    }

finish:
    cflow_actor_destroy(&actor);
    if (producer.impl != NULL) {
        if (result == 0 &&
            book_ace_actor_port_send(&port, 9) !=
                (int)CFLOW_ACTOR_SEND_STALE)
            result = 10;
        cflow_actor_ref_release(&producer);
    }
    if (cflow_scheduler_valid(&scheduler))
        cflow_scheduler_destroy(&scheduler);
    if (cflow_executor_valid(&executor))
        cflow_executor_destroy(&executor);
    cflow_machine_destroy(&machine);
    if (result != 0)
        fprintf(stderr, "ACE Active Object application stage: %d\n", result);
    return result;
}
