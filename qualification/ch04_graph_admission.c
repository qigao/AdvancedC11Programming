#include <cflow/graph.h>
#include <cflow/meta.h>
#include <cflow/verify.h>

cmeta_function(map, value, long, book_square, (int x))
{
    return (long)x * (long)x;
}

cmeta_function(filter, value, bool, book_even, (int x))
{
    return (x % 2) == 0;
}

static int book_bad_graph(void)
{
    cflow_graph graph = {0};
    const cflow_subgraph *root;
    cflow_node_id map_node = CMETA_INVALID_ID;
    cflow_node_id filter_node = CMETA_INVALID_ID;
    uint64_t before_bad_connect;
    const char *error = NULL;
    int rc = 0;

    cflow_graph_init(&graph, &cmeta_type_int);
    if (graph.error != NULL || graph.version == 0u) {
        rc = 1;
        goto done;
    }

    root = cflow_graph_subgraph(&graph, graph.root);
    if (root == NULL || root->entry == CMETA_INVALID_ID) {
        rc = 2;
        goto done;
    }

    if (!cflow_graph_create_node(
            &graph,
            graph.root,
            CFLOW_OP_MAP,
            book_square.fn,
            NULL,
            0u,
            &map_node)) {
        rc = 3;
        goto done;
    }

    if (!cflow_graph_create_node(
            &graph,
            graph.root,
            CFLOW_OP_FILTER,
            book_even.fn,
            NULL,
            0u,
            &filter_node)) {
        rc = 4;
        goto done;
    }

    root = cflow_graph_subgraph(&graph, graph.root);
    if (root == NULL) {
        rc = 5;
        goto done;
    }

    if (!cflow_graph_connect(
            &graph,
            graph.root,
            root->entry,
            0u,
            map_node,
            0u)) {
        rc = 6;
        goto done;
    }

    before_bad_connect = graph.version;

    if (cflow_graph_connect(
            &graph,
            graph.root,
            map_node,
            0u,
            filter_node,
            0u)) {
        rc = 7;
        goto done;
    }

    if (graph.version != before_bad_connect) {
        rc = 8;
        goto done;
    }

    if (cflow_graph_validate(&graph, &error)) {
        rc = 9;
        goto done;
    }

done:
    cflow_graph_destroy(&graph);
    return rc;
}

static int book_good_graph(void)
{
    cflow_graph graph = {0};
    cflow_graph clone = {0};
    const cflow_subgraph *clone_root;
    uint64_t initial_version;
    uint64_t after_square;
    uint64_t source_before_extra;
    uint64_t clone_version;
    size_t clone_node_count;
    const char *error = NULL;
    int rc = 0;

    clone.root = CMETA_INVALID_ID;

    cflow_graph_init(&graph, &cmeta_type_int);
    if (graph.error != NULL || graph.version == 0u) {
        rc = 20;
        goto done;
    }

    initial_version = graph.version;

    if (!cflow_graph_map(&graph, book_square.fn)) {
        rc = 21;
        goto done;
    }

    if (graph.version == initial_version) {
        rc = 22;
        goto done;
    }

    after_square = graph.version;

    if (!cflow_graph_take(&graph, 8u)) {
        rc = 23;
        goto done;
    }

    if (graph.version == after_square) {
        rc = 24;
        goto done;
    }

    if (!cflow_graph_validate(&graph, &error)) {
        rc = 25;
        goto done;
    }

    if (!cmeta_type_equal(
            cflow_graph_input_type(&graph),
            &cmeta_type_int) ||
        !cmeta_type_equal(
            cflow_graph_output_type(&graph),
            &cmeta_type_long)) {
        rc = 26;
        goto done;
    }

    if (!cflow_graph_clone(&clone, &graph)) {
        rc = 27;
        goto done;
    }

    if (clone.version == 0u || clone.version == graph.version) {
        rc = 28;
        goto done;
    }

    if (!cflow_graph_validate(&clone, &error) ||
        !cflow_graph_structural_equal(&graph, &clone)) {
        rc = 29;
        goto done;
    }

    clone_root = cflow_graph_subgraph(&clone, clone.root);
    if (clone_root == NULL) {
        rc = 30;
        goto done;
    }

    clone_node_count = clone_root->node_count;
    clone_version = clone.version;
    source_before_extra = graph.version;

    if (!cflow_graph_skip(&graph, 1u)) {
        rc = 31;
        goto done;
    }

    if (graph.version == source_before_extra) {
        rc = 32;
        goto done;
    }

    if (clone.version != clone_version) {
        rc = 33;
        goto done;
    }

    clone_root = cflow_graph_subgraph(&clone, clone.root);
    if (clone_root == NULL ||
        clone_root->node_count != clone_node_count) {
        rc = 34;
        goto done;
    }

    if (cflow_graph_structural_equal(&graph, &clone)) {
        rc = 35;
        goto done;
    }

    if (!cflow_graph_validate(&graph, &error) ||
        !cflow_graph_validate(&clone, &error)) {
        rc = 36;
        goto done;
    }

done:
    cflow_graph_destroy(&clone);
    cflow_graph_destroy(&graph);
    return rc;
}

int main(void)
{
    int rc = book_bad_graph();

    if (rc != 0)
        return rc;

    return book_good_graph();
}
