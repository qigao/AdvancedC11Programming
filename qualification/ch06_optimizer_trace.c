#include <cflow/lower.h>
#include <cflow/opt.h>
#include <cflow/property.h>
#include <cflow/stream.h>
#include <cflow/verify.h>

cmeta_function(map, idempotent, int, book_clamp_nonnegative, (int x))
{
    return x < 0 ? 0 : x;
}

int main(void)
{
    cflow_stream source = {0};
    cflow_graph optimized = {0};
    cflow_graph optimized_again = {0};
    cflow_opt_trace trace = {0};
    cflow_opt_stats stats = {0};
    cflow_opt_stats stats_again = {0};
    cflow_opt_rewrite_event event = {0};
    const cflow_subgraph *optimized_root;
    const cflow_node *map = NULL;
    const char *error = NULL;
    uint64_t source_version;
    uint64_t optimized_version;
    size_t i;
    int rc = 0;

    optimized.root = CMETA_INVALID_ID;
    optimized_again.root = CMETA_INVALID_ID;

    if (!cflow_stream_init(&source, &cmeta_type_int)) {
        rc = 1;
        goto done;
    }

    source.map(&source, book_clamp_nonnegative)
          ->map(&source, book_clamp_nonnegative);

    if (!cflow_stream_ok(&source) ||
        !cflow_graph_validate(&source.graph, &error) ||
        !cflow_graph_is_normalized(&source.graph)) {
        rc = 2;
        goto done;
    }

    source_version = source.graph.version;

    if (!cflow_graph_optimize_with_trace(
            &optimized,
            &source.graph,
            (cflow_opt_options){CMETA_OPT_DEFAULT},
            &stats,
            &trace)) {
        rc = 3;
        goto done;
    }

    if (!cflow_graph_validate(&optimized, &error) ||
        !cflow_graph_is_normalized(&optimized)) {
        rc = 4;
        goto done;
    }

    if (stats.idempotent_maps_eliminated != 1u) {
        rc = 5;
        goto done;
    }

    if (cflow_opt_trace_count(&trace) != 1u ||
        !cflow_opt_trace_event_at(&trace, 0u, &event)) {
        rc = 6;
        goto done;
    }

    if (event.rule != CFLOW_OPT_RULE_IDEMPOTENT_MAP_ELIMINATION ||
        event.input_subgraph != source.graph.root ||
        event.retained_node == event.removed_node ||
        event.retained_callable_index != 0u ||
        event.removed_callable_index != 0u) {
        rc = 7;
        goto done;
    }

    if (!cflow_opt_trace_bound_to(
            &trace,
            &source.graph,
            &optimized)) {
        rc = 8;
        goto done;
    }

    optimized_root = cflow_graph_subgraph(&optimized, optimized.root);
    if (optimized_root == NULL) {
        rc = 9;
        goto done;
    }

    for (i = 0u; i < optimized_root->node_count; ++i) {
        if (optimized_root->nodes[i].op == CFLOW_OP_MAP) {
            map = &optimized_root->nodes[i];
            break;
        }
    }

    if (map == NULL ||
        map->fn_chain_count != 1u ||
        !cmeta_properties_include(
            cflow_node_properties(&optimized, map),
            CMETA_PROP_STABLE | CMETA_PROP_IDEMPOTENT)) {
        rc = 10;
        goto done;
    }

    optimized_version = optimized.version;

    if (!cflow_graph_optimize(
            &optimized_again,
            &optimized,
            (cflow_opt_options){CMETA_OPT_DEFAULT},
            &stats_again)) {
        rc = 11;
        goto done;
    }

    if (!cflow_graph_validate(&optimized_again, &error) ||
        !cflow_graph_is_normalized(&optimized_again)) {
        rc = 12;
        goto done;
    }

    if (stats_again.idempotent_maps_eliminated != 0u) {
        rc = 13;
        goto done;
    }

    if (!cflow_graph_structural_equal(
            &optimized,
            &optimized_again)) {
        rc = 14;
        goto done;
    }

    if (optimized_again.version == 0u ||
        optimized_again.version == optimized.version) {
        rc = 15;
        goto done;
    }

    if (!cflow_graph_take(&source.graph, 4u)) {
        rc = 16;
        goto done;
    }

    if (source.graph.version == source_version) {
        rc = 17;
        goto done;
    }

    if (optimized.version != optimized_version) {
        rc = 18;
        goto done;
    }

    if (cflow_opt_trace_bound_to(
            &trace,
            &source.graph,
            &optimized)) {
        rc = 19;
        goto done;
    }

done:
    cflow_opt_trace_destroy(&trace);
    cflow_graph_destroy(&optimized_again);
    cflow_graph_destroy(&optimized);
    cflow_stream_destroy(&source);
    return rc;
}
