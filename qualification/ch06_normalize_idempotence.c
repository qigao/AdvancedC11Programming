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

cmeta_function(zip, value, double, book_merge, (long left, double right))
{
    return (double)left + right;
}

static size_t book_count_op(const cflow_graph *graph, cflow_op op)
{
    size_t count = 0u;
    size_t s;
    size_t n;

    if (graph == NULL)
        return 0u;

    for (s = 0u; s < graph->subgraph_count; ++s) {
        const cflow_subgraph *subgraph = &graph->subgraphs[s];

        for (n = 0u; n < subgraph->node_count; ++n)
            if (subgraph->nodes[n].op == op)
                ++count;
    }

    return count;
}

int main(void)
{
    cflow_stream left = {0};
    cflow_stream right = {0};
    cflow_graph normalized = {0};
    cflow_graph normalized2 = {0};
    uint64_t source_version;
    uint64_t normalized_version;
    const char *error = NULL;
    int rc = 0;

    normalized.root = CMETA_INVALID_ID;
    normalized2.root = CMETA_INVALID_ID;

    if (!cflow_stream_init(&left, &cmeta_type_int) ||
        !cflow_stream_init(&right, &cmeta_type_int))
        return 1;

    left.map(&left, book_square);
    right.map(&right, book_as_double);
    left.zip(&left, &right, book_merge);

    if (!cflow_stream_ok(&left) ||
        !cflow_stream_ok(&right)) {
        rc = 2;
        goto done;
    }

    if (!cflow_graph_validate(&left.graph, &error)) {
        rc = 3;
        goto done;
    }

    if (cflow_graph_is_normalized(&left.graph) ||
        book_count_op(&left.graph, CFLOW_OP_ZIP) != 1u) {
        rc = 4;
        goto done;
    }

    source_version = left.graph.version;
    if (source_version == 0u) {
        rc = 5;
        goto done;
    }

    if (!cflow_graph_normalize(&normalized, &left.graph)) {
        rc = 6;
        goto done;
    }

    if (left.graph.version != source_version ||
        book_count_op(&left.graph, CFLOW_OP_ZIP) != 1u) {
        rc = 7;
        goto done;
    }

    if (!cflow_graph_validate(&normalized, &error) ||
        !cflow_graph_is_normalized(&normalized)) {
        rc = 8;
        goto done;
    }

    if (book_count_op(&normalized, CFLOW_OP_ZIP) != 0u ||
        book_count_op(&normalized, CFLOW_OP_RELATION) == 0u) {
        rc = 9;
        goto done;
    }

    if (!cmeta_type_equal(
            cflow_graph_input_type(&normalized),
            &cmeta_type_int) ||
        !cmeta_type_equal(
            cflow_graph_output_type(&normalized),
            &cmeta_type_double)) {
        rc = 10;
        goto done;
    }

    normalized_version = normalized.version;
    if (normalized_version == 0u ||
        normalized_version == source_version) {
        rc = 11;
        goto done;
    }

    if (!cflow_graph_normalize(&normalized2, &normalized)) {
        rc = 12;
        goto done;
    }

    if (!cflow_graph_validate(&normalized2, &error) ||
        !cflow_graph_is_normalized(&normalized2)) {
        rc = 13;
        goto done;
    }

    if (!cflow_graph_structural_equal(
            &normalized,
            &normalized2)) {
        rc = 14;
        goto done;
    }

    if (normalized2.version == 0u ||
        normalized2.version == normalized.version) {
        rc = 15;
        goto done;
    }

    if (left.graph.version != source_version ||
        normalized.version != normalized_version) {
        rc = 16;
        goto done;
    }

done:
    cflow_graph_destroy(&normalized2);
    cflow_graph_destroy(&normalized);
    cflow_stream_destroy(&left);
    cflow_stream_destroy(&right);
    return rc;
}
