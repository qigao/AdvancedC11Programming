#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool book_parse_u64(const char *text, uint64_t *out)
{
    if (text == NULL || out == NULL || *text == '\0')
        return false;

    uint64_t value = 0;

    for (const char *p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9')
            return false;

        unsigned digit = (unsigned)(*p - '0');

        if (value > (UINT64_MAX - digit) / 10u)
            return false;

        value = value * 10u + digit;
    }

    *out = value;
    return true;
}

static int rejected_unchanged(const char *text)
{
    uint64_t out = UINT64_C(123456789);

    return !book_parse_u64(text, &out) &&
           out == UINT64_C(123456789);
}

int main(void)
{
    uint64_t out = UINT64_C(999);

    if (!book_parse_u64("0", &out) || out != UINT64_C(0))
        return 1;

    if (!book_parse_u64("42", &out) || out != UINT64_C(42))
        return 2;

    if (!book_parse_u64("18446744073709551615", &out) ||
        out != UINT64_MAX)
        return 3;

    if (!rejected_unchanged("18446744073709551616"))
        return 4;
    if (!rejected_unchanged(""))
        return 5;
    if (!rejected_unchanged("+1"))
        return 6;
    if (!rejected_unchanged(" 1"))
        return 7;
    if (!rejected_unchanged("1x"))
        return 8;

    out = UINT64_C(7);
    if (book_parse_u64(NULL, &out) || out != UINT64_C(7))
        return 9;
    if (book_parse_u64("1", NULL))
        return 10;

    return 0;
}
