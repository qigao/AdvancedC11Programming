#include <cflow/certificate.h>
#include <cflow/lower.h>
#include <cflow/plan.h>
#include <cflow/stream.h>

cmeta_function(map, value, long, book_square, (int x))
{
    return (long)x * (long)x;
}

static int book_check_result(
    const cflow_result *result,
    long first,
    long second)
{
    const long *values;

    if (result == NULL ||
        result->count != 2u ||
        !cmeta_type_equal(result->type, &cmeta_type_long) ||
        result->data == NULL)
        return 0;

    values = (const long *)result->data;
    return values[0] == first && values[1] == second;
}

int main(void)
{
    cflow_stream source = {0};
    cflow_plan plan = {0};
    cflow_plan_certificate certificate = {0};
    cflow_plan_compile_stats stats = {0};
    cflow_result before = {0};
    cflow_result after = {0};
    const char *error = NULL;
    int inputs[] = {2, 3};
    uint64_t graph_version;
    int rc = 0;

    if (!cflow_stream_init(&source, &cmeta_type_int)) {
        rc = 1;
        goto done;
    }

    source.map(&source, book_square);

    if (!cflow_stream_ok(&source) ||
        !cflow_graph_validate(&source.graph, &error) ||
        !cflow_graph_is_normalized(&source.graph)) {
        rc = 2;
        goto done;
    }

    graph_version = source.graph.version;

    if (!cflow_plan_compile(
            &plan,
            &source.graph,
            &stats)) {
        rc = 3;
        goto done;
    }

    if (!cmeta_type_equal(plan.input_type, &cmeta_type_int) ||
        !cmeta_type_equal(plan.output_type, &cmeta_type_long) ||
        stats.instructions == 0u) {
        rc = 4;
        goto done;
    }

    if (!cflow_plan_eval_array(
            &plan,
            inputs,
            2u,
            &before)) {
        rc = 5;
        goto done;
    }

    if (!book_check_result(&before, 4L, 9L)) {
        rc = 6;
        goto done;
    }

    if (!cflow_plan_certificate_build(
            &certificate,
            &source.graph,
            &plan,
            CFLOW_CERTIFIED_PATH_SEQUENTIAL)) {
        rc = 7;
        goto done;
    }

    if (certificate.version != CFLOW_PLAN_CERTIFICATE_V1 ||
        certificate.path != CFLOW_CERTIFIED_PATH_SEQUENTIAL ||
        certificate.graph_version != graph_version ||
        certificate.row_count != stats.instructions) {
        rc = 8;
        goto done;
    }

    if (!cflow_plan_certificate_check(
            &certificate,
            &source.graph,
            &plan,
            &error)) {
        rc = 9;
        goto done;
    }

    if (!cflow_graph_take(&source.graph, 1u)) {
        rc = 10;
        goto done;
    }

    if (source.graph.version == graph_version) {
        rc = 11;
        goto done;
    }

    error = NULL;
    if (cflow_plan_certificate_check(
            &certificate,
            &source.graph,
            &plan,
            &error)) {
        rc = 12;
        goto done;
    }

    if (error == NULL) {
        rc = 13;
        goto done;
    }

    if (!cflow_plan_eval_array(
            &plan,
            inputs,
            2u,
            &after)) {
        rc = 14;
        goto done;
    }

    if (!book_check_result(&after, 4L, 9L)) {
        rc = 15;
        goto done;
    }

done:
    cflow_result_destroy(&after);
    cflow_result_destroy(&before);
    cflow_plan_certificate_destroy(&certificate);
    cflow_plan_destroy(&plan);
    cflow_stream_destroy(&source);
    return rc;
}
