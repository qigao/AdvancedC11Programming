#include "ch02_type_identity.h"

static const cmeta_type_identity book_user_identity_a =
    CMETA_TYPE_ID_ATOM_INIT("book.User");

static const cmeta_type_desc book_user_type_a = {
    .name = "book_user",
    .size = sizeof(book_user),
    .align = _Alignof(book_user),
    .kind = CMETA_T_OBJECT,
    .pointee = NULL,
    .traits = NULL,
    .identity = &book_user_identity_a
};

const cmeta_type_desc *book_user_type_from_a(void)
{
    return &book_user_type_a;
}
