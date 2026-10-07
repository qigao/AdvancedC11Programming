#include <cmeta/data_reflect.h>
#include <cmeta/type_traits.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

cmeta_struct(book_point,
    cmeta_field(int, x)
    cmeta_field(long, y)
);

typedef struct book_account {
    int id;
    double balance;
} book_account;

cmeta_reflect_data(book_account, "book.Account",
    cmeta_field(int, id)
    cmeta_field(double, balance)
);

typedef struct book_trait_value {
    int value;
} book_trait_value;

static bool book_trait_equal(const void *left, const void *right)
{
    const book_trait_value *a = (const book_trait_value *)left;
    const book_trait_value *b = (const book_trait_value *)right;
    return a != NULL && b != NULL && a->value == b->value;
}

static uint64_t book_trait_hash(const void *value)
{
    const book_trait_value *v = (const book_trait_value *)value;
    return v == NULL ? 0u : (uint64_t)(unsigned int)v->value;
}

cmeta_traits(book_value,
    (equal, book_trait_equal),
    (hash, book_trait_hash)
);

static const cmeta_type_identity book_trait_identity =
    CMETA_TYPE_ID_ATOM_INIT("book.TraitValue");

static const cmeta_type_desc book_trait_type = {
    .name = "book_trait_value",
    .size = sizeof(book_trait_value),
    .align = _Alignof(book_trait_value),
    .kind = CMETA_T_OBJECT,
    .pointee = NULL,
    .traits = &cmeta_traits_book_value,
    .identity = &book_trait_identity
};

_Static_assert(cmeta_has_trait(book_value, Equal),
    "declared equal trait must be visible at compile time");
_Static_assert(cmeta_has_trait(book_value, Hashable),
    "declared hash trait must be visible at compile time");

int main(void)
{
    const cmeta_struct_desc *point = StructMeta(book_point);
    const cmeta_field_desc *x;
    const cmeta_field_desc *y;
    const cmeta_struct_desc *account_struct = StructMeta(book_account);
    const cmeta_field_desc *id;
    const cmeta_field_desc *balance;
    const cmeta_data_desc *account_data = cmeta_reflected_data(book_account);
    const cmeta_type_desc *account_storage = cmeta_reflected_storage(book_account);
    book_trait_value a = { 11 };
    book_trait_value b = { 11 };
    book_trait_value c = { 12 };

    if (point == NULL ||
        point->size != sizeof(book_point) ||
        point->align != _Alignof(book_point) ||
        point->field_count != 2u)
        return 1;

    x = FieldFind(book_point, "x");
    y = FieldFind(book_point, "y");
    if (x == NULL || y == NULL)
        return 2;

    if (x->offset != offsetof(book_point, x) ||
        x->size != sizeof(((book_point *)0)->x) ||
        x->type != &cmeta_type_int)
        return 3;

    if (y->offset != offsetof(book_point, y) ||
        y->size != sizeof(((book_point *)0)->y) ||
        y->type != &cmeta_type_long)
        return 4;

    if (account_struct == NULL ||
        account_struct->field_count != 2u ||
        account_struct->size != sizeof(book_account) ||
        account_struct->align != _Alignof(book_account))
        return 5;

    id = FieldFind(book_account, "id");
    balance = FieldFind(book_account, "balance");
    if (id == NULL || balance == NULL)
        return 6;

    if (id->offset != offsetof(book_account, id) ||
        id->size != sizeof(((book_account *)0)->id) ||
        id->type != &cmeta_type_int)
        return 7;

    if (balance->offset != offsetof(book_account, balance) ||
        balance->size != sizeof(((book_account *)0)->balance) ||
        balance->type != &cmeta_type_double)
        return 8;

    if (account_data == NULL ||
        !cmeta_data_desc_valid(account_data) ||
        account_data->kind != CMETA_DATA_STRUCT ||
        account_data->storage_type != account_storage)
        return 9;

    if (account_storage == NULL ||
        account_storage->size != sizeof(book_account) ||
        account_storage->align != _Alignof(book_account) ||
        strcmp(account_data->stable_id, "book.Account") != 0)
        return 10;

    if (cmeta_traits_book_value.flags !=
        (CMETA_TRAIT_EQUAL | CMETA_TRAIT_HASH))
        return 11;

    if (cmeta_traits_book_value.equal != book_trait_equal ||
        cmeta_traits_book_value.hash != book_trait_hash ||
        cmeta_traits_book_value.compare != NULL)
        return 12;

    if (cmeta_type_require_traits(
            &book_trait_type,
            CMETA_TRAIT_EQUAL | CMETA_TRAIT_HASH) != CMETA_OK)
        return 13;

    if (cmeta_type_require_traits(
            &book_trait_type,
            CMETA_TRAIT_COMPARE) != CMETA_TRAIT_MISSING)
        return 14;

    if (!cmeta_traits_book_value.equal(&a, &b) ||
        cmeta_traits_book_value.equal(&a, &c))
        return 15;

    return 0;
}
