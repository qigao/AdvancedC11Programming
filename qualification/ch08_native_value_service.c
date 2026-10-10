#include "book_value.service_native.h"
#include <data_bind_native.h>

#include <string.h>

/*
 * SaltsUtils 4.3+ Contract-only Native Service: no Binary codec or
 * generated *_native.c. Caller owns the binding metadata and call storage.
 * Optional/nullable VIEW overlays are deliberately outside VALUE admission.
 */
int main(void)
{
    databind_10_BookNative_4_Calc_3_Add__native_owner owner = {0};
    DataBindServiceNativeBinding binding = {0};
    DataBindError error = DATA_BIND_ERROR_INIT;
    DataBindNativeOptions options = DATA_BIND_NATIVE_OPTIONS_INIT;
    DataBindNativeDiagnostic diagnostic = DATA_BIND_NATIVE_DIAGNOSTIC_INIT;
    DataBindNativePlan *plan = NULL;
    AddRequest request = {0};
    AddResponse response = {0};
    void *params[] = {&request, &response};
    unsigned char workspace[4096] = {0};
    const DataBindNativeExecution *execution =
        databind_10_BookNative_4_Calc_3_Add__databind_execution();
    int returned = -1;
    int rc = 1;

    if (execution == NULL || !data_bind_native_execution_valid(execution))
        goto done;

    if (databind_10_BookNative_4_Calc_3_Add__databind_native_binding(
            &owner, &binding, &error) != DATA_BIND_OK ||
        binding.request != &owner.request_binding ||
        binding.response != &owner.response_binding ||
        binding.function != execution->function ||
        binding.request->data != &owner.request_metadata.data ||
        binding.response->data != &owner.response_metadata.data ||
        strcmp(binding.request->idl_type_name, "AddRequest") != 0)
        goto done;

    options.workspace = workspace;
    options.workspace_bytes = sizeof(workspace);
    options.max_depth = 16u;
    options.max_items = 64u;
    options.max_owned_bytes = sizeof(workspace);

    if (data_bind_native_plan_compile(
            &options, binding.request->data, &plan, &diagnostic) != DATA_BIND_OK ||
        plan == NULL)
        goto done;

    if (data_bind_native_plan_init(
            plan, &options, &request, sizeof(request), &diagnostic) != DATA_BIND_OK)
        goto done;

    request.left = 3u;
    request.scale = 1u;
    if (!execution->invoke(execution->context, &returned, params, 2u) ||
        returned != 0 || response.sum != 4u)
        goto done;

    if (data_bind_native_plan_clear(
            plan, &options, &request, sizeof(request), &diagnostic) != DATA_BIND_OK ||
        request.left != 0u || request.scale != 0u)
        goto done;

    rc = 0;

done:
    data_bind_native_plan_free(plan);
    return rc;
}
