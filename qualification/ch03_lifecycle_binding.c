#include <cmeta/lifecycle.h>

typedef struct book_value {
    int payload;
} book_value;

static const cmeta_type_identity book_value_identity =
    CMETA_TYPE_ID_ATOM_INIT("book.Value");

static const cmeta_type_desc book_value_type = {
    .name = "book_value",
    .size = sizeof(book_value),
    .align = _Alignof(book_value),
    .kind = CMETA_T_OBJECT,
    .pointee = NULL,
    .traits = NULL,
    .identity = &book_value_identity
};

static cmeta_status book_value_init_zero(void *storage)
{
    book_value *value = (book_value *)storage;
    value->payload = 0;
    return CMETA_OK;
}

static void book_value_restore_zero(void *storage)
{
    book_value *value = (book_value *)storage;
    value->payload = 0;
}

static void book_value_move(void *destination, void *source)
{
    book_value *dst = (book_value *)destination;
    book_value *src = (book_value *)source;

    dst->payload = src->payload;
    src->payload = 0;
}

static const cmeta_data_construct_ops book_value_construct_ops = {
    .struct_size = sizeof(cmeta_data_construct_ops),
    .abi_version = CMETA_DATA_CONSTRUCT_OPS_ABI_VERSION,
    .storage_type = &book_value_type,
    .init_zero = book_value_init_zero,
    .restore_zero = book_value_restore_zero,
    .move = book_value_move,
    .flags = CMETA_LIFECYCLE_INIT_NOFAIL | CMETA_LIFECYCLE_MOVABLE
};

static const unsigned char book_value_shape = 0;

static const cmeta_data_desc book_value_data = {
    .struct_size = sizeof(cmeta_data_desc),
    .abi_version = CMETA_DATA_DESC_ABI_VERSION,
    .stable_id = "book.Value.data",
    .display_name = "book_value",
    .kind = CMETA_DATA_CUSTOM,
    .storage_type = &book_value_type,
    .shape = &book_value_shape,
    .buffer_ops = NULL,
    .enum_ops = NULL,
    .variant_ops = NULL,
    .fixed_ops = NULL,
    .enum_bits_ops = NULL,
    .collection_ops = NULL,
    .map_ops = NULL,
    .construct_ops = &book_value_construct_ops
};

int main(void)
{
    cmeta_lifecycle_binding binding = CMETA_LIFECYCLE_BINDING_INIT;
    cmeta_lifecycle_binding rejected = CMETA_LIFECYCLE_BINDING_INIT;
    book_value source = { 41 };
    book_value destination = { 99 };
    cmeta_status status;

    status = cmeta_lifecycle_admit(
        &book_value_data,
        sizeof(book_value),
        _Alignof(book_value),
        &binding);
    if (status != CMETA_OK)
        return 1;

    if (binding.data != &book_value_data ||
        binding.ops != &book_value_construct_ops)
        return 2;

    status = cmeta_lifecycle_init(&binding, &destination);
    if (status != CMETA_OK || destination.payload != 0)
        return 3;

    status = cmeta_lifecycle_move(&binding, &destination, &source);
    if (status != CMETA_OK ||
        destination.payload != 41 ||
        source.payload != 0)
        return 4;

    status = cmeta_lifecycle_restore(&binding, &destination);
    if (status != CMETA_OK || destination.payload != 0)
        return 5;

    status = cmeta_lifecycle_move(&binding, &destination, &destination);
    if (status != CMETA_INVALID_ARGUMENT)
        return 6;

    rejected.data = &book_value_data;
    rejected.ops = &book_value_construct_ops;

    status = cmeta_lifecycle_admit(
        &book_value_data,
        sizeof(book_value) + 1u,
        _Alignof(book_value),
        &rejected);
    if (status != CMETA_TYPE_MISMATCH)
        return 7;

    if (rejected.data != NULL || rejected.ops != NULL)
        return 8;

    return 0;
}
