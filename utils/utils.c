#include "utils.h"

static size_t StringToSizeT(const char *str)
{
    if (!str)
        return 0;

    errno = 0;
    char *end = NULL;

    unsigned long long val = strtoull(str, &end, 10);

    // no digits parsed
    if (end == str)
        return 0;

    // overflow or invalid
    if (errno == ERANGE || val > SIZE_MAX)
        return 0;

    return (size_t)val;
}
