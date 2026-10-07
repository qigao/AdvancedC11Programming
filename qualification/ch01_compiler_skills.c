#include <cmeta/compiler.h>
#include <cmeta/container_of.h>

typedef struct book_link {
    int tag;
} book_link;

typedef struct book_node {
    int value;
    book_link link;
} book_node;

static int book_calls;

static int book_next_value(void)
{
    ++book_calls;
    return 17;
}

static book_link *book_next_link(book_node *node)
{
    ++book_calls;
    return &node->link;
}

int main(void)
{
    book_node node = { .value = 9, .link = { .tag = 3 } };
    book_node *owner;
    int required_value = 5 + CMETA_CONST_REQUIRE(sizeof(book_node) >= sizeof(book_link));

    if (required_value != 5)
        return 1;

    book_calls = 0;
    if (!CMETA_TYPE_MATCHES(book_next_value(), int))
        return 2;
    if (book_calls != 0)
        return 3;

#if CMETA_HAS_SAME_TYPE
    book_calls = 0;
    if (!CMETA_SAME_TYPE(book_next_value(), node.value))
        return 4;
    if (book_calls != 0)
        return 5;
#endif

#if CMETA_HAS_AUTO
    book_calls = 0;
    CMETA_AUTO(value, book_next_value());
    if (book_calls != 1 || value != 17)
        return 6;
#endif

    book_calls = 0;
    owner = cmeta_container_of_as(
        book_next_link(&node),
        book_node,
        book_link,
        link);
    if (book_calls != 1 || owner != &node)
        return 7;

#if CMETA_HAS_CONTAINER_OF
    book_calls = 0;
    owner = cmeta_container_of(
        book_next_link(&node),
        book_node,
        link);
    if (book_calls != 1 || owner != &node)
        return 8;
#endif

    return 0;
}
