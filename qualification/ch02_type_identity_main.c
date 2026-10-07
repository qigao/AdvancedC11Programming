#include "ch02_type_identity.h"

static const cmeta_type_identity book_other_identity =
    CMETA_TYPE_ID_ATOM_INIT("book.Other");

static const cmeta_type_desc book_other_type = {
    .name = "book_user",
    .size = sizeof(book_user),
    .align = _Alignof(book_user),
    .kind = CMETA_T_OBJECT,
    .pointee = NULL,
    .traits = NULL,
    .identity = &book_other_identity
};

int main(void)
{
    const cmeta_type_desc *a = book_user_type_from_a();
    const cmeta_type_desc *b = book_user_type_from_b();
    const cmeta_type_identity *a_id;
    const cmeta_type_identity *b_id;

    if (a == b)
        return 1;

    if (!cmeta_type_desc_valid(a) || !cmeta_type_desc_valid(b))
        return 2;

    a_id = cmeta_type_identity_of(a);
    b_id = cmeta_type_identity_of(b);

    if (a_id == NULL || b_id == NULL)
        return 3;

    if (a_id == b_id)
        return 4;

    if (!cmeta_type_identity_equal(a_id, b_id))
        return 5;

    if (!cmeta_type_equal(a, b))
        return 6;

    if (!cmeta_type_desc_valid(&book_other_type))
        return 7;

    if (cmeta_type_identity_equal(a_id, &book_other_identity))
        return 8;

    if (cmeta_type_equal(a, &book_other_type))
        return 9;

    return 0;
}
