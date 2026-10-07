#include <cmeta/scope.h>

#include <stddef.h>

#if !CMETA_HAS_COUNTER
#error "book scope qualification requires the public unique-name capability"
#endif

typedef struct book_first_resource {
    int live;
} book_first_resource;

typedef struct book_second_resource {
    int live;
} book_second_resource;

static int book_events[16];
static size_t book_event_count;
static int book_body_calls;
static int book_fail_second;

static void book_record(int event)
{
    if (book_event_count < sizeof(book_events) / sizeof(book_events[0]))
        book_events[book_event_count++] = event;
}

static const cmeta_type_identity book_first_identity =
    CMETA_TYPE_ID_ATOM_INIT("book.ScopeFirst");

static const cmeta_type_desc book_first_type = {
    .name = "book_first_resource",
    .size = sizeof(book_first_resource),
    .align = _Alignof(book_first_resource),
    .kind = CMETA_T_OBJECT,
    .pointee = NULL,
    .traits = NULL,
    .identity = &book_first_identity
};

static cmeta_status book_first_init(book_first_resource *value)
{
    book_record(10);
    value->live = 1;
    return CMETA_OK;
}

static void book_first_restore(book_first_resource *value)
{
    book_record(50);
    value->live = 0;
}

static void book_first_move(
    book_first_resource *destination,
    book_first_resource *source)
{
    destination->live = source->live;
    source->live = 0;
}

CMETA_DEFINE_LIFECYCLE(
    book_first_resource,
    &book_first_type,
    book_first_init,
    book_first_restore,
    book_first_move,
    CMETA_LIFECYCLE_MOVABLE);

static const cmeta_type_identity book_second_identity =
    CMETA_TYPE_ID_ATOM_INIT("book.ScopeSecond");

static const cmeta_type_desc book_second_type = {
    .name = "book_second_resource",
    .size = sizeof(book_second_resource),
    .align = _Alignof(book_second_resource),
    .kind = CMETA_T_OBJECT,
    .pointee = NULL,
    .traits = NULL,
    .identity = &book_second_identity
};

static cmeta_status book_second_init(book_second_resource *value)
{
    book_record(20);
    value->live = 1;
    if (book_fail_second)
        return CMETA_INVALID_ARGUMENT;
    return CMETA_OK;
}

static void book_second_restore(book_second_resource *value)
{
    book_record(40);
    value->live = 0;
}

static void book_second_move(
    book_second_resource *destination,
    book_second_resource *source)
{
    destination->live = source->live;
    source->live = 0;
}

CMETA_DEFINE_LIFECYCLE(
    book_second_resource,
    &book_second_type,
    book_second_init,
    book_second_restore,
    book_second_move,
    CMETA_LIFECYCLE_MOVABLE);

static cmeta_status book_scope_body(
    book_first_resource *first,
    book_second_resource *second)
{
    ++book_body_calls;
    book_record(30);

    if (first == NULL || second == NULL ||
        first->live != 1 || second->live != 1)
        return CMETA_INVALID_ARGUMENT;

    return CMETA_TYPE_MISMATCH;
}

static void book_reset(void)
{
    size_t i;

    book_event_count = 0u;
    book_body_calls = 0;
    for (i = 0u; i < sizeof(book_events) / sizeof(book_events[0]); ++i)
        book_events[i] = 0;
}

static int book_expect(
    const int *expected,
    size_t count)
{
    size_t i;

    if (book_event_count != count)
        return 0;

    for (i = 0u; i < count; ++i)
        if (book_events[i] != expected[i])
            return 0;

    return 1;
}

int main(void)
{
    cmeta_status status;
    static const int normal_order[] = {10, 20, 30, 40, 50};
    static const int partial_order[] = {10, 20, 40, 50};

    book_reset();
    book_fail_second = 0;

    cmeta_scope(
        status,
        cmeta_autos(
            (book_first_resource, first),
            (book_second_resource, second)),
        cmeta_body(book_scope_body(&first, &second)));

    if (status != CMETA_TYPE_MISMATCH)
        return 1;
    if (book_body_calls != 1)
        return 2;
    if (!book_expect(
            normal_order,
            sizeof(normal_order) / sizeof(normal_order[0])))
        return 3;

    book_reset();
    book_fail_second = 1;

    cmeta_scope(
        status,
        cmeta_autos(
            (book_first_resource, first),
            (book_second_resource, second)),
        cmeta_body(book_scope_body(&first, &second)));

    if (status != CMETA_INVALID_ARGUMENT)
        return 4;
    if (book_body_calls != 0)
        return 5;
    if (!book_expect(
            partial_order,
            sizeof(partial_order) / sizeof(partial_order[0])))
        return 6;

    return 0;
}
