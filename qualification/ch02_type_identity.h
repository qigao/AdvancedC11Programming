#ifndef BOOK_CH02_TYPE_IDENTITY_H
#define BOOK_CH02_TYPE_IDENTITY_H

#include <cmeta/cmeta.h>

typedef struct book_user {
    int id;
    long score;
} book_user;

const cmeta_type_desc *book_user_type_from_a(void);
const cmeta_type_desc *book_user_type_from_b(void);

#endif
