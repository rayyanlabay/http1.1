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

#define METHOD 0
#define PATH 1
#define PROTOCOL 2

#define PARSE_FIRST_LINE 0
#define PARSE_HEADERS 1
#define PARSE_BODY 2

typedef struct
{
    char *data;
    size_t size;
} slice_t;
typedef struct
{
    slice_t *key;
    slice_t *val;
} header_t;

// must change later to linked list style of fixed size arrays
#define MAXHEADER_NUM 128

typedef struct
{
    slice_t s[3];
    header_t headers[MAXHEADER_NUM];
    char *body;
} http_request_t;

#endif //!__HTTP_SERVER__H__