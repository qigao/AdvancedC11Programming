#include <cmeta/bind.h>

int book_add(int a, int b);

FunctionDeclAsAbiResult(
    value,
    int,
    &cmeta_type_int,
    CMETA_ABI_SCALAR,
    CMETA_RESULT_VALUE,
    book_add,
    (int, a, CMETA_PARAM_IN,
     &cmeta_type_int, CMETA_ABI_SCALAR),
    (int, b, CMETA_PARAM_IN,
     &cmeta_type_int, CMETA_ABI_SCALAR));

FunctionBindDeclAsAbiResult(
    value,
    int,
    &cmeta_type_int,
    CMETA_ABI_SCALAR,
    CMETA_RESULT_VALUE,
    book_plus10,
    book_add,
    CMETA_SIG_U_I_I,
    (value,
        (int, a, CMETA_PARAM_IN,
         &cmeta_type_int, CMETA_ABI_SCALAR)),
    (arg,
        (int, b, CMETA_PARAM_IN,
         &cmeta_type_int, CMETA_ABI_SCALAR)));

int book_add(int a, int b)
{
    return a + b;
}

int main(void)
{
    book_plus10_capture capture = { .a = 10 };
    cmeta_invokable invokable = CMETA_INVOKABLE_INIT;
    cmeta_invokable rejected = CMETA_INVOKABLE_INIT;
    int input;
    int output;
    const void *args[1];

    if (book_plus10_bind(&capture, &invokable) != CMETA_OK)
        return 1;

    if (!cmeta_invokable_valid(&invokable))
        return 2;

    if (invokable.function != FunctionMeta(book_plus10) ||
        invokable.callable.capture_size != sizeof(book_plus10_capture) ||
        invokable.callable.capture_size > CMETA_CAPTURE_INLINE)
        return 3;

    capture.a = 99;

    input = 7;
    output = -1;
    args[0] = &input;

    if (cmeta_invokable_invoke(
            &invokable,
            &output,
            args) != CMETA_OK)
        return 4;

    if (output != 17)
        return 5;

    input = 5;
    output = -1;

    if (cmeta_invokable_invoke_admitted(
            &invokable,
            &output,
            args) != CMETA_OK)
        return 6;

    if (output != 15)
        return 7;

    if (book_plus10_bind(NULL, &rejected) != CMETA_INVALID_ARGUMENT)
        return 8;

    if (rejected.function != NULL)
        return 9;

    return 0;
}
