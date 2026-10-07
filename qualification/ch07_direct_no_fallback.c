#include <cflow/direct.h>
#include <cflow/plan.h>

cmeta_function(filter, value, bool, book_even, (int value))
{
    return (value % 2) == 0;
}

cmeta_function(map, value, long, book_square, (int value))
{
    return (long)value * (long)value;
}

cmeta_function(map, stateful, long, book_stateful_square, (int value))
{
    return (long)value * (long)value;
}

#define BookDirectSteps(M)     CFlowDirectSteps(M,         (filter, int, int, book_even),         (map, int, long, book_square))

cflow_direct_pipeline(
    book_direct,
    int,
    &cmeta_type_int,
    long,
    2,
    BookDirectSteps);

#define BookStatefulDirectSteps(M)     CFlowDirectSteps(M,         (map, int, long, book_stateful_square))

cflow_direct_pipeline(
    book_stateful_direct,
    int,
    &cmeta_type_int,
    long,
    1,
    BookStatefulDirectSteps);

static int book_plan_result_ok(
    const cflow_result *result,
    long first,
    long second)
{
    const long *values;

    if (result == NULL ||
        result->count != 2u ||
        result->data == NULL ||
        !cmeta_type_equal(result->type, &cmeta_type_long))
        return 0;

    values = (const long *)result->data;
    return values[0] == first && values[1] == second;
}

int main(void)
{
    const int inputs[] = {1, 2, 3, 4};
    long outputs[4] = {0};
    long small_outputs[3] = {0};
    long rejected_outputs[4] = {0};
    size_t output_count = 0u;
    const cflow_aot_pipeline_ir *ir;
    const char *error = NULL;
    cflow_stream stateful_stream = {0};
    cflow_plan stateful_plan = {0};
    cflow_result stateful_result = {0};
    const int stateful_inputs[] = {2, 3};
    int rc = 0;

    if (!book_direct_eligible()) {
        rc = 1;
        goto done;
    }

    ir = book_direct_ir();
    if (ir == NULL ||
        !cflow_aot_pipeline_ir_validate(ir, &error) ||
        !cflow_aot_pipeline_ir_inline_eligible(ir, &error)) {
        rc = 2;
        goto done;
    }

    if (ir->stage_count != 2u ||
        ir->stages[0].dispatch != CFLOW_AOT_DISPATCH_STATIC_TARGET ||
        ir->stages[1].dispatch != CFLOW_AOT_DISPATCH_STATIC_TARGET) {
        rc = 3;
        goto done;
    }

    output_count = 99u;
    if (book_direct_eval_array(
            inputs,
            4u,
            outputs,
            4u,
            &output_count) != CFLOW_DIRECT_OK) {
        rc = 4;
        goto done;
    }

    if (output_count != 2u ||
        outputs[0] != 4L ||
        outputs[1] != 16L) {
        rc = 5;
        goto done;
    }

    output_count = 99u;
    if (book_direct_eval_array(
            inputs,
            4u,
            small_outputs,
            3u,
            &output_count) != CFLOW_DIRECT_CAPACITY_EXCEEDED) {
        rc = 6;
        goto done;
    }

    if (output_count != 0u) {
        rc = 7;
        goto done;
    }

    if (book_stateful_direct_eligible()) {
        rc = 8;
        goto done;
    }

    output_count = 99u;
    if (book_stateful_direct_eval_array(
            inputs,
            4u,
            rejected_outputs,
            4u,
            &output_count) != CFLOW_DIRECT_INELIGIBLE) {
        rc = 9;
        goto done;
    }

    if (output_count != 0u) {
        rc = 10;
        goto done;
    }

    if (!cflow_stream_init(
            &stateful_stream,
            &cmeta_type_int)) {
        rc = 11;
        goto done;
    }

    stateful_stream.map(
        &stateful_stream,
        book_stateful_square);

    if (!cflow_stream_ok(&stateful_stream)) {
        rc = 12;
        goto done;
    }

    if (!cflow_plan_compile_surface(
            &stateful_plan,
            &stateful_stream.graph,
            NULL)) {
        rc = 13;
        goto done;
    }

    if (!cflow_plan_eval_array(
            &stateful_plan,
            stateful_inputs,
            2u,
            &stateful_result)) {
        rc = 14;
        goto done;
    }

    if (!book_plan_result_ok(
            &stateful_result,
            4L,
            9L)) {
        rc = 15;
        goto done;
    }

done:
    cflow_result_destroy(&stateful_result);
    cflow_plan_destroy(&stateful_plan);
    cflow_stream_destroy(&stateful_stream);
    return rc;
}
