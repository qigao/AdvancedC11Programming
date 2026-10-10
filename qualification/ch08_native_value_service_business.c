#include "book_value.service_native.h"

/* Contract-only Native uses exact Record pointers, not Binary *_t views. */
int databind_10_BookNative_4_Calc_3_Add(
    const AddRequest *request, AddResponse *response)
{
    if (request == NULL || response == NULL)
        return -1;
    response->sum = request->left + request->scale;
    return 0;
}
