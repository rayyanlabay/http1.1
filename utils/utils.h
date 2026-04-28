#ifndef __UTILS__H__
#define __UTILS__H__

#include <stddef.h>
void SizeTToStr(size_t value, char *buffer, size_t buffer_size);
size_t StringToSizeT(const char *str);

#endif //!__UTILS__H__