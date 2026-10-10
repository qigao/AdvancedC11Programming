/*
 * Installed SDK, strict C11: CNet Strategy decisions and CMeta typed handlers.
 *
 * A CNet SG Owner placement decision is neither an endpoint dial nor a
 * capacity reservation. A remote destination choice is a different axis.
 * This qualification opens no socket or worker and adds no Actor runtime.
 */
#include <cnet/owner_placement.h>
#include <cnet/destination_policy.h>
#include <cnet/recovery_policy.h>
#include <cmeta/interface.h>
#include <salts/error_codes.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define BOOK_CHECK(cond) do {                                       \
    if (!(cond)) {                                                  \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);   \
        return 1;                                                   \
    }                                                               \
} while (0)

/* ACE-style Strategy callback: typed CMeta Interface, ordinary C function. */
#define BOOK_SELECTED_OWNER_METHODS(X, I) \
    X(I, FV1, void, selected, stateful, \
      &cmeta_type_void, CMETA_ABI_VOID, \
      (int, owner, CMETA_PARAM_IN, &cmeta_type_int, CMETA_ABI_SCALAR))

CMETA_INTERFACE(book_owner_handler, BOOK_SELECTED_OWNER_METHODS);

typedef struct book_probe {
    unsigned callbacks;
    int last_owner;
} book_probe;

static void book_selected_impl(void *self, int owner)
{
    book_probe *probe = (book_probe *)self;
    ++probe->callbacks;
    probe->last_owner = owner;
}

CMETA_IMPLEMENTS(book_owner_handler, book_probe_handler, 0u,
                 .selected = book_selected_impl);

static int qualify_owner_and_handler(void)
{
    cnet_owner_placement_hint hints[3] = {
        {true, 9u}, {true, 1u}, {true, 3u}
    };
    cnet_owner_placement_input decision = {0};
    size_t chosen = SIZE_MAX;
    book_probe probe = {0};
    book_owner_handler handler =
        book_probe_handler_as_book_owner_handler(&probe);

    decision.size = sizeof(decision);
    decision.version = CNET_OWNER_PLACEMENT_VERSION;
    decision.kind = CNET_OWNER_PLACE_EXPLICIT;
    decision.owners = hints;
    decision.owner_count = 3u;
    decision.explicit_owner = 1u;

    BOOK_CHECK(cnet_owner_placement_choose(&decision, &chosen) == SALTS_OK);
    BOOK_CHECK(chosen == 1u);
    book_owner_handler_selected(&handler, (int)chosen);
    BOOK_CHECK(probe.callbacks == 1u && probe.last_owner == 1);

    /* Pinning never authorizes movement to another Owner on FULL. */
    hints[1].eligible = false;
    chosen = 20u;
    BOOK_CHECK(cnet_owner_placement_choose(&decision, &chosen) ==
               SALTS_ENOBUFS);
    BOOK_CHECK(chosen == SIZE_MAX);
    BOOK_CHECK(probe.callbacks == 1u); /* No spurious strategy callback. */

    /* RR is an explicit *different* policy; it may skip ineligible peers. */
    decision.kind = CNET_OWNER_PLACE_ROUND_ROBIN;
    decision.sequence = 1u;
    BOOK_CHECK(cnet_owner_placement_choose(&decision, &chosen) == SALTS_OK);
    BOOK_CHECK(chosen == 2u);

    /* Strict-key placement is stable modulo final Owner count. */
    decision.kind = CNET_OWNER_PLACE_STRICT_KEY;
    decision.key_known = true;
    decision.key_hash = 4u; /* 4 % 3 = 1: marked ineligible. */
    BOOK_CHECK(cnet_owner_placement_choose(&decision, &chosen) ==
               SALTS_ENOBUFS);
    BOOK_CHECK(chosen == SIZE_MAX);
    decision.key_known = false;
    BOOK_CHECK(cnet_owner_placement_choose(&decision, &chosen) == SALTS_EINVAL);
    return 0;
}

static int qualify_remote_destination(void)
{
    cnet_destination_hint hints[] = {
        {11u, 1u, 3u, true}, {22u, 2u, 3u, true},
        {33u, 3u, 3u, true}, {44u, 4u, 3u, true}
    };
    cnet_destination_selection decision = {0};
    cnet_destination_result result = {0};
    cnet_destination_result kept = {0};
    cnet_destination_hint compact[3];
    const uint64_t kept_generation = 8u;
    size_t removed_index;

    decision.size = sizeof(decision);
    decision.version = CNET_DESTINATION_POLICY_VERSION;
    decision.kind = CNET_DESTINATION_EXPLICIT;
    decision.endpoints = hints;
    decision.endpoint_count = 4u;
    decision.snapshot_generation = 7u;
    decision.expires_at_ms = 1000u;
    decision.now_ms = 10u;
    decision.explicit_endpoint_id = 33u;

    BOOK_CHECK(cnet_destination_choose(&decision, &result) == SALTS_OK);
    BOOK_CHECK(result.endpoint_id == 33u && result.index == 2u);
    BOOK_CHECK(result.snapshot_generation == 7u);

    hints[2].eligible = false;
    BOOK_CHECK(cnet_destination_choose(&decision, &result) == SALTS_ENOBUFS);
    BOOK_CHECK(result.index == SIZE_MAX && result.endpoint_id == 0u);
    hints[2].eligible = true;

    decision.kind = CNET_DESTINATION_STRICT_KEY;
    decision.key_known = true;
    decision.key_hash = 77u;
    BOOK_CHECK(cnet_destination_choose(&decision, &result) == SALTS_OK);

    /*
     * Rendezvous hashing uses stable endpoint IDs: remove an unrelated
     * candidate; a previously selected endpoint keeps its identity.
     */
    removed_index = result.index == 0u ? 1u : 0u;
    {
        size_t out = 0u;
        for (size_t i = 0u; i < 4u; ++i) {
            if (i != removed_index)
                compact[out++] = hints[i];
        }
        BOOK_CHECK(out == 3u);
    }
    decision.endpoints = compact;
    decision.endpoint_count = 3u;
    decision.snapshot_generation = kept_generation;
    BOOK_CHECK(cnet_destination_choose(&decision, &kept) == SALTS_OK);
    BOOK_CHECK(kept.endpoint_id == result.endpoint_id);
    BOOK_CHECK(kept.snapshot_generation == kept_generation);

    /* STRICT_KEY never redirects away from the selected ID on health loss. */
    compact[kept.index].eligible = false;
    BOOK_CHECK(cnet_destination_choose(&decision, &kept) == SALTS_ENOBUFS);
    BOOK_CHECK(kept.index == SIZE_MAX);
    /* Failed selection still never authorizes a different remote peer. */
    decision.now_ms = decision.expires_at_ms;
    BOOK_CHECK(cnet_destination_choose(&decision, &kept) == SALTS_ETIMEDOUT);
    BOOK_CHECK(kept.index == SIZE_MAX);

    return 0;
}

static int qualify_request_retry_denial(void)
{
    cnet_retry_input request = {0};
    cnet_retry_result result = {0};

    request.size = sizeof(request);
    request.version = CNET_RECOVERY_POLICY_VERSION;
    request.attempts_used = 1u;
    request.max_attempts = 3u;
    request.now_ms = 10u;
    request.deadline_ms = 100u;

    /* Network reconnection policy does not authorize logical DATA replay. */
    BOOK_CHECK(cnet_retry_evaluate(&request, &result) == SALTS_OK);
    BOOK_CHECK(!result.allowed && result.reason == CNET_RETRY_DISABLED);
    return 0;
}

int main(void)
{
    if (qualify_owner_and_handler() != 0)
        return 1;
    if (qualify_remote_destination() != 0)
        return 2;
    if (qualify_request_retry_denial() != 0)
        return 3;
    return 0;
}
