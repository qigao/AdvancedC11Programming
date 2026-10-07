#include <data_bind.h>

int main(void)
{
    if (DATA_BIND_ABI_VERSION <= 0)
        return 1;

    if (data_bind_library_version() != DATA_BIND_VERSION)
        return 2;

    return 0;
}
