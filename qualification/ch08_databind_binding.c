#include <data_bind_binding_plan.h>

#include "book_service.service_native.h"
#include "book_service_native.h"

#include <string.h>

typedef struct book_input {
    cserde_token token;
    int emitted;
} book_input;

static cserde_status book_next(void *context, cserde_token *out)
{
    book_input *input = (book_input *)context;

    if (input->emitted)
        return CSERDE_DONE;

    *out = input->token;
    input->emitted = 1;
    return CSERDE_OK;
}

static DataBindStatus book_open_input(
    void *context,
    const DataBindBindingPlanEntry *entry,
    cserde_reader *reader,
    DataBindBindingValueState *state,
    DataBindError *error)
{
    static const cserde_reader_ops ops = {
        sizeof(cserde_reader_ops),
        CSERDE_READER_OPS_ABI_VERSION,
        book_next
    };
    book_input *input = (book_input *)context;

    (void)error;

    if (entry == NULL || reader == NULL ||
        state == NULL || input == NULL)
        return DATA_BIND_ERR_INVALID_ARG;

    if (strcmp(entry->schema_field, "scale") == 0) {
        *state = DATA_BIND_VALUE_STATE_ABSENT;
        return DATA_BIND_OK;
    }

    input->token.kind = CSERDE_UINT;
    input->token.value.uint = 3u;
    input->emitted = 0;

    *state = DATA_BIND_VALUE_STATE_VALUE;
    return cserde_reader_init(reader, &ops, input) == CSERDE_OK
        ? DATA_BIND_OK
        : DATA_BIND_ERR_RUNTIME;
}

static DataBindStatus book_project_field(
    void *context,
    const DataBindServiceOperation *operation,
    const DataBindSchemaField *field,
    DataBindBindingDirection direction,
    DataBindBindingAddress *out,
    DataBindError *error)
{
    (void)context;
    (void)operation;
    (void)error;

    if (field == NULL || out == NULL)
        return DATA_BIND_ERR_INVALID_ARG;

    *out = (DataBindBindingAddress)DATA_BIND_BINDING_ADDRESS_INIT;
    out->binding_class = direction == DATA_BIND_BINDING_INGRESS
        ? DATA_BIND_BINDING_VALUE
        : DATA_BIND_BINDING_RESULT;
    out->space = "book-service";
    out->name = field->name;
    return DATA_BIND_OK;
}

int main(void)
{
    DataBind *codec = NULL;
    DataBindBindingPlan *plan = NULL;
    DataBindNativeTypeBinding request_binding = {0};
    DataBindNativeTypeBinding response_binding = {0};
    DataBindServiceNativeBinding native = {0};
    DataBindError error = DATA_BIND_ERROR_INIT;
    DataBindBindingPlanDiagnostic diagnostic =
        DATA_BIND_BINDING_PLAN_DIAGNOSTIC_INIT;
    DataBindBindingProjection projection =
        DATA_BIND_BINDING_PROJECTION_INIT;
    DataBindBindingProvider provider =
        DATA_BIND_BINDING_PROVIDER_INIT;
    DataBindBindingCallLifetime lifetime =
        DATA_BIND_BINDING_CALL_LIFETIME_INIT;
    DataBindBindingCallFrame frame =
        DATA_BIND_BINDING_CALL_FRAME_INIT;
    DataBindNativeOptions options =
        DATA_BIND_NATIVE_OPTIONS_INIT;
    book_input input = {{CSERDE_UINT}, 0};
    unsigned char workspace[4096];
    AddRequest_t request = {0};
    AddResponse_t response = {0};
    void *params[] = {&request, &response};
    const size_t sizes[] = {
        sizeof(request),
        sizeof(response)
    };
    const cmeta_function_desc *function;
    int rc = 0;

    if (databind_10_ServiceSdk_4_Calc_3_Add__databind_native_binding(
            &request_binding,
            &response_binding,
            &native,
            &error) != DATA_BIND_OK) {
        rc = 1;
        goto done;
    }

    if (ServiceSdk_codec_create(&codec, &error) != DATA_BIND_OK ||
        codec == NULL) {
        rc = 2;
        goto done;
    }

    projection.id = "book-service";
    projection.project_field = book_project_field;

    if (data_bind_binding_plan_compile_service(
            codec,
            "Calc",
            "Add",
            &projection,
            &native,
            &plan,
            &diagnostic) != DATA_BIND_OK ||
        plan == NULL) {
        rc = 3;
        goto done;
    }

    function = data_bind_binding_plan_function(plan);
    if (function == NULL ||
        function != native.function ||
        data_bind_binding_plan_ingress_count(plan) != 2u ||
        data_bind_binding_plan_egress_count(plan) != 1u) {
        rc = 4;
        goto done;
    }

    data_bind_free(codec);
    codec = NULL;

    if (data_bind_binding_plan_operation_id(plan) == NULL ||
        data_bind_binding_plan_operation_id(plan)[0] == '\0' ||
        strcmp(
            data_bind_binding_plan_projection_id(plan),
            "book-service") != 0 ||
        data_bind_binding_plan_function(plan) != function) {
        rc = 5;
        goto done;
    }

    options.workspace = workspace;
    options.workspace_bytes = sizeof(workspace);
    options.max_depth = 16u;
    options.max_items = 64u;
    options.max_owned_bytes = sizeof(workspace);

    provider.context = &input;
    provider.open_input = book_open_input;

    frame.request = &request;
    frame.request_bytes = sizeof(request);
    frame.params = params;
    frame.param_bytes = sizes;
    frame.param_count = 2u;

    if (data_bind_binding_plan_bind_call(
            plan,
            &provider,
            &options,
            &frame,
            &lifetime,
            &diagnostic) != DATA_BIND_OK) {
        rc = 6;
        goto done;
    }

    if (!data_bind_binding_call_is_live(&lifetime) ||
        request.left != 3u ||
        request.scale != 1u) {
        rc = 7;
        goto done;
    }

    if (databind_10_ServiceSdk_4_Calc_3_Add(
            &request,
            &response) != 0 ||
        response.sum != 4u) {
        rc = 8;
        goto done;
    }

    if (data_bind_binding_call_restore_zero(
            &lifetime,
            &diagnostic) != DATA_BIND_OK) {
        rc = 9;
        goto done;
    }

    if (data_bind_binding_call_is_live(&lifetime) ||
        request.left != 0u ||
        request.scale != 0u ||
        response.sum != 0u) {
        rc = 10;
        goto done;
    }

    if (data_bind_binding_call_restore_zero(
            &lifetime,
            &diagnostic) != DATA_BIND_ERR_INVALID_ARG) {
        rc = 11;
        goto done;
    }

done:
    if (data_bind_binding_call_is_live(&lifetime))
        (void)data_bind_binding_call_restore_zero(
            &lifetime,
            &diagnostic);
    data_bind_binding_plan_free(plan);
    data_bind_free(codec);
    return rc;
}
