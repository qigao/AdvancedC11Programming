#include <cflow/lower.h>
#include <cflow/stream.h>
#include <cflow/verify.h>

cmeta_function(map, value, long, book_square, (int x))
{
    return (long)x * (long)x;
}

cmeta_function(map, value, double, book_as_double, (int x))
{
    return (double)x + 0.25;
}

cmeta_function(zip, value, double, book_merge_long_double, (long a, double b))
{
    return (double)a + b;
}

static size_t book_count_op(const cflow_graph *graph, cflow_op op)
{
    size_t count = 0u;
    size_t s;
    size_t n;

    if (graph == NULL)
        return 0u;

    for (s = 0u; s < graph->subgraph_count; ++s)
        for (n = 0u; n < graph->subgraphs[s].node_count; ++n)
            if (graph->subgraphs[s].nodes[n].op == op)
                ++count;

    return count;
}

int main(void)
{
    cflow_stream left = {0};
    cflow_stream right = {0};
    cflow_graph normalized = {0};
    cflow_graph normalized_again = {0};
    const char *error = NULL;
    uint64_t source_version;
    uint64_t normalized_version;
    int rc = 0;

    normalized.root = CMETA_INVALID_ID;
    normalized_again.root = CMETA_INVALID_ID;

    if (!cflow_stream_init(&left, &cmeta_type_int) ||
        !cflow_stream_init(&right, &cmeta_type_int)) {
        rc = 1;
        goto done;
    }

    left.map(&left, book_square);
    right.map(&right, book_as_double);
    left.zip(&left, &right, book_merge_long_double);

    if (!cflow_stream_ok(&left) ||
        !cflow_stream_ok(&right)) {
        rc = 2;
        goto done;
    }

    if (!cflow_graph_validate(&left.graph, &error)) {
        rc = 3;
        goto done;
    }

    if (cflow_graph_is_normalized(&left.graph)) {
        rc = 4;
        goto done;
    }

    if (book_count_op(&left.graph, CFLOW_OP_ZIP) != 1u) {
        rc = 5;
        goto done;
    }

    source_version = left.graph.version;
    if (source_version == 0u) {
        rc = 6;
        goto done;
    }

    if (!cflow_graph_normalize(&normalized, &left.graph)) {
        rc = 7;
        goto done;
    }

    if (left.graph.version != source_version ||
        book_count_op(&left.graph, CFLOW_OP_ZIP) != 1u) {
        rc = 8;
        goto done;
    }

    if (!cflow_graph_validate(&left.graph, &error)) {
        rc = 9;
        goto done;
    }

    if (!cflow_graph_validate(&normalized, &error) ||
        !cflow_graph_is_normalized(&normalized)) {
        rc = 10;
        goto done;
    }

    if (book_count_op(&normalized, CFLOW_OP_ZIP) != 0u ||
        book_count_op(&normalized, CFLOW_OP_RELATION) == 0u) {
        rc = 11;
        goto done;
    }

    if (normalized.version == 0u ||
        normalized.version == source_version) {
        rc = 12;
        goto done;
    }

    normalized_version = normalized.version;

    if (!cmeta_type_equal(
            cflow_graph_input_type(&normalized),
            &cmeta_type_int) ||
        !cmeta_type_equal(
            cflow_graph_output_type(&normalized),
            &cmeta_type_double)) {
        rc = 13;
        goto done;
    }

    if (!cflow_graph_normalize(
            &normalized_again,
            &normalized)) {
        rc = 14;
        goto done;
    }

    if (normalized.version != normalized_version) {
        rc = 15;
        goto done;
    }

    if (!cflow_graph_validate(&normalized_again, &error) ||
        !cflow_graph_is_normalized(&normalized_again)) {
        rc = 16;
        goto done;
    }

    if (normalized_again.version == 0u ||
        normalized_again.version == normalized.version) {
        rc = 17;
        goto done;
    }

    if (!cflow_graph_structural_equal(
            &normalized,
            &normalized_again)) {
        rc = 18;
        goto done;
    }

    if (book_count_op(&normalized_again, CFLOW_OP_ZIP) != 0u ||
        book_count_op(&normalized_again, CFLOW_OP_RELATION) == 0u) {
        rc = 19;
        goto done;
    }

done:
    cflow_graph_destroy(&normalized_again);
    cflow_graph_destroy(&normalized);
    cflow_stream_destroy(&left);
    cflow_stream_destroy(&right);
    return rc;
}
