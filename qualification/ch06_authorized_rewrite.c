#include <cflow/opt.h>
#include <cflow/property.h>
#include <cflow/stream.h>
#include <cflow/verify.h>

cmeta_function(map, idempotent, int, book_clamp, (int value))
{
    return value < 0 ? 0 : value;
}

cmeta_function(map, value, int, book_clamp_unproven, (int value))
{
    return value < 0 ? 0 : value;
}

static const cflow_node *book_find_map(const cflow_graph *graph)
{
    const cflow_subgraph *root;
    size_t i;

    if (graph == NULL)
        return NULL;

    root = cflow_graph_subgraph(graph, graph->root);
    if (root == NULL)
        return NULL;

    for (i = 0u; i < root->node_count; ++i)
        if (root->nodes[i].op == CFLOW_OP_MAP)
            return &root->nodes[i];

    return NULL;
}

static int book_positive(void)
{
    cflow_stream stream = {0};
    cflow_graph optimized = {0};
    cflow_graph optimized2 = {0};
    cflow_opt_trace trace = {0};
    cflow_opt_stats stats = {0};
    cflow_opt_stats stats2 = {0};
    cflow_opt_rewrite_event event = {0};
    const cflow_node *map;
    int rc = 0;

    optimized.root = CMETA_INVALID_ID;
    optimized2.root = CMETA_INVALID_ID;

    if (!cflow_stream_init(&stream, &cmeta_type_int))
        return 1;

    stream.map(&stream, book_clamp);
    stream.map(&stream, book_clamp);

    if (!cflow_stream_ok(&stream) ||
        !cflow_graph_is_normalized(&stream.graph)) {
        rc = 2;
        goto done;
    }

    if (!cflow_graph_optimize_with_trace(
            &optimized,
            &stream.graph,
            (cflow_opt_options){ CMETA_OPT_DEFAULT },
            &stats,
            &trace)) {
        rc = 3;
        goto done;
    }

    if (stats.idempotent_maps_eliminated != 1u ||
        cflow_opt_trace_count(&trace) != 1u) {
        rc = 4;
        goto done;
    }

    if (!cflow_opt_trace_event_at(&trace, 0u, &event) ||
        event.rule != CFLOW_OPT_RULE_IDEMPOTENT_MAP_ELIMINATION) {
        rc = 5;
        goto done;
    }

    if (!cflow_opt_trace_bound_to(
            &trace,
            &stream.graph,
            &optimized)) {
        rc = 6;
        goto done;
    }

    map = book_find_map(&optimized);
    if (map == NULL ||
        map->fn_chain_count != 1u ||
        !cmeta_properties_include(
            cflow_node_properties(&optimized, map),
            CMETA_PROP_STABLE | CMETA_PROP_IDEMPOTENT)) {
        rc = 7;
        goto done;
    }

    if (!cflow_graph_optimize(
            &optimized2,
            &optimized,
            (cflow_opt_options){ CMETA_OPT_DEFAULT },
            &stats2)) {
        rc = 8;
        goto done;
    }

    if (stats2.idempotent_maps_eliminated != 0u ||
        !cflow_graph_structural_equal(
            &optimized,
            &optimized2)) {
        rc = 9;
        goto done;
    }

done:
    cflow_opt_trace_destroy(&trace);
    cflow_graph_destroy(&optimized2);
    cflow_graph_destroy(&optimized);
    cflow_stream_destroy(&stream);
    return rc;
}

static int book_negative(void)
{
    cflow_stream stream = {0};
    cflow_graph optimized = {0};
    cflow_opt_trace trace = {0};
    cflow_opt_stats stats = {0};
    const cflow_node *map;
    int rc = 0;

    optimized.root = CMETA_INVALID_ID;

    if (!cflow_stream_init(&stream, &cmeta_type_int))
        return 20;

    stream.map(&stream, book_clamp_unproven);
    stream.map(&stream, book_clamp_unproven);

    if (!cflow_stream_ok(&stream) ||
        !cflow_graph_is_normalized(&stream.graph)) {
        rc = 21;
        goto done;
    }

    if (!cflow_graph_optimize_with_trace(
            &optimized,
            &stream.graph,
            (cflow_opt_options){ CMETA_OPT_DEFAULT },
            &stats,
            &trace)) {
        rc = 22;
        goto done;
    }

    if (stats.idempotent_maps_eliminated != 0u ||
        stats.property_blocked_idempotent_eliminations == 0u) {
        rc = 23;
        goto done;
    }

    if (cflow_opt_trace_count(&trace) != 0u) {
        rc = 24;
        goto done;
    }

    map = book_find_map(&optimized);
    if (map == NULL ||
        map->fn_chain_count != 2u) {
        rc = 25;
        goto done;
    }

done:
    cflow_opt_trace_destroy(&trace);
    cflow_graph_destroy(&optimized);
    cflow_stream_destroy(&stream);
    return rc;
}

int main(void)
{
    int rc = book_positive();

    if (rc != 0)
        return rc;

    return book_negative();
}
