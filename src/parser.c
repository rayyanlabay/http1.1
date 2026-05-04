#include "parser.h"

void parser_consume_header(parser_state_t *p_state)
{
    http_request_t *http_msg = p_state->http_msg;

    char *buffer = p_state->buffer;

    size_t crlf_start = p_state->crlf_start;
    size_t window_start = p_state->window_start;

    size_t header_i = p_state->header_i;

    char *content_length = p_state->content_length;
    size_t chunked = p_state->chunked;

    http_status_t state = p_state->state;

    char *str = buffer + window_start;
    char *s1 = strchr(str, ':');

    if (!s1)
    {
        return HTTP_BAD_REQUEST;
    }

    size_t key_size = s1 - str;

    if (key_size > 0)
    {
        http_msg->headers[header_i].key.size = key_size;
        http_msg->headers[header_i].key.data = str;
    }
    else
    {
        return HTTP_BAD_REQUEST;
    }

    if (0 == str + crlf_start - (s1 + 1))
    {
        return HTTP_BAD_REQUEST;
    }

    while (s1 < str + crlf_start && 0 != isspace(*(s1++)))
        ;

    char *val_end = str + crlf_start - 1;

    while (val_end > s1 && 0 != isspace(*val_end--))
        ;
    ++val_end;

    if (val_end < s1)
    {
        return HTTP_BAD_REQUEST;
    }

    char *s2 = s1;
    size_t val_size = val_end - s2 + 1;

    http_msg->headers[header_i].val.size = val_size;
    http_msg->headers[header_i].val.data = s2;

    char *header_key = http_msg->headers[header_i].key.data;
    size_t header_size = http_msg->headers[header_i].key.size;

    if (0 == strncmp_lower(header_key, "content-length", header_size))
    {
        content_length = http_msg->headers[header_i].val.data;
    }
    else if (0 == strncmp_lower(header_key, "transfer-encoding", header_size))
    {
        chunked = 1;
    }

    ++header_i;

    state = PARSE_FIRSTLINE_HEADERS;
    window_start = crlf_start + 2;
}

void parser_consume_firstline(parser_state_t *p_state)
{
    char *str = p_state->buffer + p_state->window_start;
    char *s1 = NULL, *s2 = NULL, *s_end = str + p_state->crlf_start;
    s1 = strchr(str, ' ');
    s2 = s1 ? strchr(s1 + 1, ' ') : NULL;
    if (!s1 || !s2)
    {
        return HTTP_BAD_REQUEST;
    }

    p_state->http_msg->s[METHOD].data = str;
    p_state->http_msg->s[METHOD].size = s1 - str;

    p_state->http_msg->s[PATH].data = s1 + 1;
    p_state->http_msg->s[PATH].size = s2 - (s1 + 1);

    p_state->http_msg->s[PROTOCOL].data = s2 + 1;
    p_state->http_msg->s[PROTOCOL].size = s_end - (s2 + 1);

    p_state->first_line_read = 1;
    p_state->state = PARSE_FIRSTLINE_HEADERS;
    p_state->window_start = p_state->crlf_start + 2;
}

void parse_loop(parser_state_t *p_state)
{
    size_t bytes_read = 0;

    while (p_state->bytes_left_not_read--)
    {
        if (p_state->buffer[p_state->i] == '\r' && p_state->buffer[p_state->i + 1] == '\n')
        {
            p_state->crlf_start = p_state->i;

            if (p_state->i < p_state->stream_length - 3 && (p_state->buffer[p_state->i + 2] == '\r' && p_state->buffer[p_state->i + 3] == '\n'))
            {
                p_state->i += 4;
                p_state->state = PARSE_BODY;
            }
            else
            {
                p_state->i += 2;
                p_state->state = p_state->first_line_read ? CONSUME_HEADER : CONSUME_FIRST_LINE;
            }
            break;
        }
        ++p_state->i;
    }

    while (p_state->state == PARSE_FIRSTLINE_HEADERS &&
           (bytes_read = read(p_state->connection_fd, p_state->buffer + p_state->stream_length, BUFSIZE - p_state->stream_length)) > 0)
    {
        p_state->stream_length += bytes_read;
        bytes_read = 0;

        for (; p_state->i < p_state->stream_length - 1; ++p_state->i)
        {
            if (p_state->buffer[p_state->i] == '\r' && p_state->buffer[p_state->i + 1] == '\n')
            {
                p_state->crlf_start = p_state->i;

                p_state->bytes_left_not_read = p_state->stream_length - p_state->i - 2;

                if (p_state->i < p_state->stream_length - 3 && (p_state->buffer[p_state->i + 2] == '\r' && p_state->buffer[p_state->i + 3] == '\n'))
                {
                    p_state->i += 4;
                    p_state->state = PARSE_BODY;
                }
                else
                {
                    p_state->i += 2;
                    p_state->state = p_state->first_line_read ? CONSUME_HEADER : CONSUME_FIRST_LINE;
                }

                break;
            }
        }
    }
}