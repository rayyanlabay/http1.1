#ifndef __POOL_SERVICE__H__
#define __POOL_SERVICE__H__

#include <stddef.h>

typedef struct pool pool_t;

pool_t *PoolCreate();

void PoolWrite();

char *PoolContentEnd(pool_t *pool);

#endif //!__POOL_SERVICE__H__
