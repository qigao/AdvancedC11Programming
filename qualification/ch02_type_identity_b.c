#include "ch02_type_identity.h"

static const cmeta_type_identity book_user_identity_b =
    CMETA_TYPE_ID_ATOM_INIT("book.User");

static const cmeta_type_desc book_user_type_b = {
    .name = "book_user",
    .size = sizeof(book_user),
    .align = _Alignof(book_user),
    .kind = CMETA_T_OBJECT,
    .pointee = NULL,
    .traits = NULL,
    .identity = &book_user_identity_b
};

const cmeta_type_desc *book_user_type_from_b(void)
{
    return &book_user_type_b;
}
