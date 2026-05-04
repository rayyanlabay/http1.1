#ifndef __PARSER__H__
#define __PARSER__H__

#include <stddef.h>
#include "http_server.h"

typedef struct
{
    int connection_fd;

    char *buffer;

    size_t stream_length;
    size_t bytes_left_not_read;

    size_t i;
    size_t crlf_start;
    size_t header_i;
    size_t window_start;

    http_request_t *http_msg;

    char *content_length;
    int chunked;

    http_status_t state;
    int first_line_read;

} parser_state_t;

void parse_loop(parser_state_t *p_state);
void parser_consume_firstline(parser_state_t *p_state);
void parser_consume_header(parser_state_t *p_state);

#endif //!__PARSER__H__
