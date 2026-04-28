#ifndef __HTTP_SERVER__H__
#define __HTTP_SERVER__H__

#include "pool_service.h"

#define METHOD_IDX 0
#define PATH_IDX 1
#define PROTOCOL_IDX 2

typedef enum
{
    HTTP_OK = 200,
    HTTP_CREATED = 201,
    HTTP_NO_CONTENT = 204,

    HTTP_BAD_REQUEST = 400,
    HTTP_UNAUTHORIZED = 401,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,

    HTTP_INTERNAL_SERVER_ERROR = 500
} http_status_t;

typedef struct
{
    char *key;
    char *val;
} http_header_entry_t;

typedef struct
{
    http_header_entry_t *entries_start;
    http_header_entry_t *entries_end;

    int count;
} header_list_t;

#define METHOD 0
#define PATH 1
#define PROTOCOL 2

typedef struct
{
    char *s[3];

    header_list_t headers;

    // later change to grab from memory pool
    char *body;
} http_request_t;

#endif //!__HTTP_SERVER__H__