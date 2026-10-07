#include <cmeta/invoke_decl.h>

static int calls;

int increment(int input);

FunctionInvokeDecl(
    value,
    int,
    increment,
    (int, input, CMETA_PARAM_IN));

int increment(int input)
{
    ++calls;
    return input + 1;
}

int main(void)
{
    int input = 4;
    int output = -1;
    void *params[] = { &input };

    if (!FunctionInvoke(increment)(&output, params, 1))
        return 1;
    if (output != 5 || calls != 1)
        return 2;

    int alias = 4;
    void *alias_params[] = { &alias };

    if (!FunctionInvoke(increment)(&alias, alias_params, 1))
        return 3;
    if (alias != 5 || calls != 2)
        return 4;

    output = 77;

    if (FunctionInvoke(increment)(&output, params, 0))
        return 5;
    if (output != 77 || calls != 2)
        return 6;

    if (FunctionInvoke(increment)(&output, NULL, 1))
        return 7;
    if (output != 77 || calls != 2)
        return 8;

    void *null_arg[] = { NULL };

    if (FunctionInvoke(increment)(&output, null_arg, 1))
        return 9;
    if (output != 77 || calls != 2)
        return 10;

    if (FunctionInvoke(increment)(NULL, params, 1))
        return 11;
    if (calls != 2)
        return 12;

    return 0;
}
