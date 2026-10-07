#include <cmeta/pp.h>

#include <stddef.h>
#include <string.h>

#define BOOK_DECLARE_FIELD(type_, name_, context_) type_ name_;

typedef struct book_pair_record {
    CMETA_PP_PAIR_MAP_N(
        2,
        BOOK_DECLARE_FIELD,
        ~,
        int, left,
        long, right)
} book_pair_record;

#define BOOK_PAIR_NAME(type_, name_, context_) CMETA_PP_STRINGIFY(name_)

static const char *const book_pair_names[] = {
    CMETA_PP_PAIR_MAP_COMMA_N(
        2,
        BOOK_PAIR_NAME,
        ~,
        int, left,
        long, right)
};

#define BOOK_VALUE(item_, context_) item_

static const int book_values[] = {
    CMETA_PP_MAP_COMMA_N(3, BOOK_VALUE, ~, 2, 4, 8)
};

#define BOOK_IDENTIFIER generated_name
static const char book_expanded_name[] = CMETA_PP_STRINGIFY(BOOK_IDENTIFIER);

enum {
    book_tuple_middle = CMETA_PP_TUPLE_GET_1((11, 29, 47))
};

_Static_assert(book_tuple_middle == 29, "tuple projection must preserve the selected item");
_Static_assert(
    sizeof(((book_pair_record *)0)->left) == sizeof(int),
    "pair mapping must preserve the first field type");
_Static_assert(
    sizeof(((book_pair_record *)0)->right) == sizeof(long),
    "pair mapping must preserve the second field type");

#define BOOK_ZERO_ITEM(item_, context_) ++book_zero_count;
#define BOOK_ZERO_PAIR(type_, name_, context_) ++book_zero_count;

int main(void)
{
    int book_zero_count = 0;
    book_pair_record record = { .left = 3, .right = 9L };

    CMETA_PP_MAP_N(0, BOOK_ZERO_ITEM, ~, )
    CMETA_PP_PAIR_MAP_N(0, BOOK_ZERO_PAIR, ~, )

    if (book_zero_count != 0)
        return 1;

    if (record.left != 3 || record.right != 9L)
        return 2;

    if (sizeof(book_pair_names) / sizeof(book_pair_names[0]) != 2)
        return 3;

    if (strcmp(book_pair_names[0], "left") != 0 ||
        strcmp(book_pair_names[1], "right") != 0)
        return 4;

    if (book_values[0] != 2 || book_values[1] != 4 || book_values[2] != 8)
        return 5;

    if (strcmp(book_expanded_name, "generated_name") != 0)
        return 6;

    return 0;
}
