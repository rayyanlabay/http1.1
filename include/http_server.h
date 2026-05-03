#ifndef __HTTP_SERVER__H__
#define __HTTP_SERVER__H__

#define MAXHEADER_NUM 128

#define METHOD 0
#define PATH 1
#define PROTOCOL 2

typedef enum
{
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS
} http_method_t;

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

typedef enum
{
    PARSE_FIRSTLINE_HEADERS,
    CONSUME_FIRST_LINE,
    CONSUME_HEADER,

    PARSE_BODY,
    CONSUME_BODY
} state_machine_t;

typedef struct
{
    char *data;
    size_t size;
} slice_t;

typedef struct
{
    slice_t key;
    slice_t val;
} header_t;

typedef struct
{
    slice_t s[3];
    header_t headers[MAXHEADER_NUM];
    char *body;
} http_request_t;

#endif //!__HTTP_SERVER__H__