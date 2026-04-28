#include "pool_service.h"

#define POOL_CANARY 0xDEADBEEF

#define KB 1024
#define POOL_SIZE 8 * KB

typedef struct pool
{
    size_t size;
    char *head;
    char *content_end;
    char *end;

    struct pool *next;
} pool_t;

pool_t *PoolCreate()
{
    pool_t *pool = (pool_t *)malloc(sizeof(pool_t));

    pool->head = calloc(0, POOL_SIZE);
    pool->content_end = pool->head;
    pool->size = POOL_SIZE;
    pool->end = pool->head + pool->size - 1;

    return pool;
}

void PoolWrite(pool_t *pool, char *data, size_t n)
{
    // if there is no enough space
    // allocate next
    if (pool->content_end + n > pool->end)
    {
        // allocate new pool
        pool->next = PoolCreate();
        pool = pool->next;
    }

    memcpy(pool->content_end, data, n);
    pool->content_end += n;
}
char *PoolContentEnd(pool_t *pool)
{
    return pool->content_end;
}
