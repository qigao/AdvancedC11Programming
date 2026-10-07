#include <cflow/meta.h>
#include <cflow/stream.h>

cmeta_function(filter, value, bool, even, (int x))
{
    return (x % 2) == 0;
}

cmeta_function(map, value, long, square, (int x))
{
    return (long)x * (long)x;
}

int main(void)
{
    cflow_stream stream = {0};
    const cflow_graph *graph = NULL;
    const cflow_subgraph *root = NULL;
    const cflow_node *input = NULL;
    const cflow_node *filter = NULL;
    const cflow_node *map = NULL;
    const cflow_edge *first = NULL;
    const cflow_edge *second = NULL;
    const char *error = NULL;
    int rc = 0;

    if (!cflow_stream_init(&stream, &cmeta_type_int))
        return 1;

    stream.filter(&stream, even);
    stream.map(&stream, square);

    if (!cflow_stream_ok(&stream)) {
        rc = 2;
        goto done;
    }

    graph = cflow_stream_graph(&stream);

    if (!graph || !cflow_graph_validate(graph, &error)) {
        rc = 3;
        goto done;
    }

    root = cflow_graph_subgraph(graph, graph->root);

    if (!root ||
        root->node_count != 3u ||
        root->edge_count != 2u) {
        rc = 4;
        goto done;
    }

    input = cflow_subgraph_node(root, 0u);
    filter = cflow_subgraph_node(root, 1u);
    map = cflow_subgraph_node(root, 2u);
    first = cflow_subgraph_edge(root, 0u);
    second = cflow_subgraph_edge(root, 1u);

    if (!input || !filter || !map || !first || !second) {
        rc = 5;
        goto done;
    }

    if (input->op != CFLOW_OP_INPUT ||
        filter->op != CFLOW_OP_FILTER ||
        map->op != CFLOW_OP_MAP) {
        rc = 6;
        goto done;
    }

    if (!cmeta_type_equal(input->input_type, &cmeta_type_int) ||
        !cmeta_type_equal(input->output_type, &cmeta_type_int) ||
        !cmeta_type_equal(filter->input_type, &cmeta_type_int) ||
        !cmeta_type_equal(filter->output_type, &cmeta_type_int) ||
        !cmeta_type_equal(map->input_type, &cmeta_type_int) ||
        !cmeta_type_equal(map->output_type, &cmeta_type_long) ||
        !cmeta_type_equal(
            cflow_stream_output_type(&stream),
            &cmeta_type_long)) {
        rc = 7;
        goto done;
    }

    if (first->from != 0u || first->from_port != 0u ||
        first->to != 1u || first->to_port != 0u ||
        second->from != 1u || second->from_port != 0u ||
        second->to != 2u || second->to_port != 0u) {
        rc = 8;
        goto done;
    }

done:
    cflow_stream_destroy(&stream);
    return rc;
}
