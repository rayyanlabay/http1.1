
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils.h"

void SizeTToStr(size_t value, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0)
        return;

    snprintf(buffer, buffer_size, "%zu", value);
}

size_t StringToSizeT(const char *str)
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
