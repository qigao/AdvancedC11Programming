#include <cmeta/operation.h>

typedef struct book_counter {
    int value;
} book_counter;

static const cmeta_type_identity book_counter_identity =
    CMETA_TYPE_ID_ATOM_INIT("book.Counter");

static const cmeta_type_desc book_counter_type = {
    .name = "book_counter",
    .size = sizeof(book_counter),
    .align = _Alignof(book_counter),
    .kind = CMETA_T_OBJECT,
    .pointee = NULL,
    .traits = NULL,
    .identity = &book_counter_identity
};

static const cmeta_type_desc book_counter_pointer = {
    .name = "book_counter *",
    .size = sizeof(void *),
    .align = _Alignof(void *),
    .kind = CMETA_T_POINTER,
    .pointee = &book_counter_type,
    .traits = NULL,
    .identity = NULL
};

#define BOOK_COUNTER_RECEIVER     (CMETA_PARAM_INOUT | CMETA_PARAM_BORROWED | CMETA_PARAM_RECEIVER)

FunctionDeclAsAbiResult(
    stateful,
    int,
    &cmeta_type_int,
    CMETA_ABI_SCALAR,
    CMETA_RESULT_VALUE,
    book_counter_add,
    (void *, self, BOOK_COUNTER_RECEIVER,
     &book_counter_pointer, CMETA_ABI_OBJECT_POINTER),
    (int, delta, CMETA_PARAM_IN,
     &cmeta_type_int, CMETA_ABI_SCALAR));

int book_counter_add(void *self, int delta)
{
    book_counter *counter = (book_counter *)self;
    counter->value += delta;
    return counter->value;
}

static const cmeta_receiver_operation book_counter_operations[] = {
    { "add", FunctionAbi(book_counter_add) }
};

static const cmeta_receiver_operation_set book_counter_operation_set = {
    .size = sizeof(cmeta_receiver_operation_set),
    .receiver_type = &book_counter_type,
    .operations = book_counter_operations,
    .operation_count = 1u,
    .owner = NULL
};

int main(void)
{
    const cmeta_type_desc *int_args[] = { &cmeta_type_int };
    const cmeta_type_desc *long_args[] = { &cmeta_type_long };
    cmeta_receiver_resolution resolution = CMETA_RECEIVER_RESOLUTION_INIT;
    cmeta_receiver_resolve_status status;

    if (!cmeta_receiver_operation_set_valid(&book_counter_operation_set))
        return 1;

    status = cmeta_receiver_operation_resolve(
        &book_counter_operation_set,
        &book_counter_type,
        NULL,
        "add",
        int_args,
        1u,
        &resolution);
    if (status != CMETA_RECEIVER_RESOLVE_OK)
        return 2;

    if (resolution.operation != &book_counter_operations[0] ||
        resolution.argument_index != CMETA_RECEIVER_ARGUMENT_NONE)
        return 3;

    resolution = (cmeta_receiver_resolution)CMETA_RECEIVER_RESOLUTION_INIT;
    status = cmeta_receiver_operation_resolve(
        &book_counter_operation_set,
        &cmeta_type_long,
        NULL,
        "add",
        int_args,
        1u,
        &resolution);
    if (status != CMETA_RECEIVER_RESOLVE_RECEIVER_TYPE_MISMATCH ||
        resolution.operation != NULL)
        return 4;

    resolution = (cmeta_receiver_resolution)CMETA_RECEIVER_RESOLUTION_INIT;
    status = cmeta_receiver_operation_resolve(
        &book_counter_operation_set,
        &book_counter_type,
        NULL,
        "missing",
        int_args,
        1u,
        &resolution);
    if (status != CMETA_RECEIVER_RESOLVE_OPERATION_NOT_FOUND ||
        resolution.operation != NULL)
        return 5;

    resolution = (cmeta_receiver_resolution)CMETA_RECEIVER_RESOLUTION_INIT;
    status = cmeta_receiver_operation_resolve(
        &book_counter_operation_set,
        &book_counter_type,
        NULL,
        "add",
        long_args,
        1u,
        &resolution);
    if (status != CMETA_RECEIVER_RESOLVE_ARGUMENT_TYPE_MISMATCH ||
        resolution.operation != NULL ||
        resolution.argument_index != 0u)
        return 6;

    return 0;
}
