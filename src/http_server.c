#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "http_server.h"
#include "utils.h"

#define PORT 8080
#define SERVER_RUNNING 1

#define METHOD_STRING_START 0
#define PATH_STRING_START 1
#define PROTOCOL_STRING_START 2

#define KB 1024

char *FindCRLF(ssize_t n_bytes, char *s)
{
    if (n_bytes < 2)
    {
        return NULL;
    }

    char *start = s;
    while ((s - start) + 1 < n_bytes && !('\r' == s[0] && '\n' == s[1]))
    {
        ++s;
    }

    if ((s - start) + 1 >= n_bytes)
        return NULL;

    return s;
}

int FoundCRLF(char *buf, size_t n_bytes, size_t *current_location)
{
    for (size_t i = 0; i < n_bytes - 1; ++i)
    {
        if (buf[*current_location] == '\r' && buf[*current_location + 1] == '\n')
        {
            return *current_location;
        }
        ++(*(current_location));
    }

    return 0;
}

int ParseFirstLine(char *str, http_request_t *http_msg, size_t crlf_index)
{

    char *s1 = NULL, *s2 = NULL, *s_end = str + crlf_index;
    s1 = strchr(str, ' ');
    s2 = s1 ? strchr(s1 + 1, ' ') : NULL;
    if (!s1 || !s2)
    {
        return HTTP_BAD_REQUEST;
    }

    http_msg->s[METHOD].data = str;
    http_msg->s[METHOD].size = s1 - str;

    http_msg->s[PATH].data = s1 + 1;
    http_msg->s[PATH].size = s2 - (s1 + 1);

    http_msg->s[PROTOCOL].data = s2 + 1;
    http_msg->s[PROTOCOL].size = s_end - (s2 + 1);

    return HTTP_OK;
}

int ParseHeaders(char *str, http_request_t *http_msg, size_t crlf_index, size_t header_i)
{
    // get key
    char *s1 = strchr(str, ':');
    // the format is not ok
    if (!s1)
    {
        return HTTP_BAD_REQUEST;
    }
    // there was key
    if (s1 - str > 0)
    {
        size_t key_size = s1 - str;

        // later check whether key is valid format
        // size_t i = 0;
        // while (i < key_size)
        // {
        //     // if key contains space
        //     if (!(0 != isspace(s1[i])))
        //     {
        //         return HTTP_BAD_REQUEST;
        //     }
        // }

        http_msg->headers[header_i].key->size = key_size;
        http_msg->headers[header_i].key->data = str;
    }
    // there is no value
    if (0 == str + crlf_index - s1 - 1)
    {
        return HTTP_BAD_REQUEST;
    }
    // there is some value, is it valid ?

    ++s1;
    while (s1 < str + crlf_index && 0 != isspace(*(s1++)))
        ;
    // value was all spaces
    if (s1 == str + crlf_index)
    {
        return HTTP_BAD_REQUEST;
    }
    char *val_end = str + crlf_index - 1;
    while (val_end > s1 && 0 != isspace(*val_end--))
        ;
    // trim left and right done
    char *s2 = s1;
    size_t val_size = val_end - s2 + 1;

    http_msg->headers[header_i].val->size = val_size;
    http_msg->headers[header_i].val->data = s2;

    return HTTP_OK;
}

http_status_t
ParseHttp(int connection_fd, char *buffer, http_request_t *http_msg)
{
    // crlf_index points at cr-lf in relation to parse_portion
    size_t crlf_index = 0;
    char *parse_portion = buffer;

    char *read_point = parse_portion;

    size_t buf_size = sizeof(parse_portion);

    size_t total_read = 0;

    ssize_t n_bytes = 0;

    while (n_bytes = (read(connection_fd, read_point, buf_size) > 0))
    {
        total_read = read_point + n_bytes - parse_portion;
        if (0 == FoundCRLF(parse_portion, total_read - crlf_index - 1, &crlf_index))
        {
            buf_size -= n_bytes;
            read_point += n_bytes;

            continue;
        }

        break;
    }

    if (HTTP_BAD_REQUEST == ParseFirstLine(parse_portion, http_msg, crlf_index))
    {
        return HTTP_BAD_REQUEST;
    }

    // next portion
    parse_portion = buffer + crlf_index + 2;
    crlf_index = 0;

    int state = PARSE_HEADERS;
    size_t header_i = 0;

    while (state != PARSE_BODY)
    {
        while (n_bytes = (read(connection_fd, read_point, buf_size) > 0))
        {
            if (0 == FoundCRLF(buffer, total_read - crlf_index - 1, &crlf_index))
            {
                buf_size -= n_bytes;
                read_point += n_bytes;

                continue;
            }

            break;
        }

        if (crlf_index == 0)
        {
            return HTTP_BAD_REQUEST;
        }
        ++header_i;
        if (HTTP_BAD_REQUEST == ParseHeaders(parse_portion, http_msg, crlf_index, header_i))
        {
            return HTTP_BAD_REQUEST;
        }

        // next portion
        parse_portion = parse_portion + crlf_index + 2;
        crlf_index = 0;
    }

    return HTTP_OK;
}

int main(int argc, char **argv)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int port = atoi(argv[1]);
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // bind server file descriptor to socket
    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // SOMAXCONN is max connections allowed
    listen(server_fd, SOMAXCONN);

    // this will hold the connection
    int connection_fd = 0;
    struct sockaddr_in client_addr = {0};
    socklen_t client_len = sizeof(client_addr);

    char buffer[4 * KB] = {0};
    ssize_t bytes = 0;
    while (SERVER_RUNNING)
    {

        connection_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        http_request_t http_msg = {0};

        memset(&http_msg, 0, sizeof(http_msg));

        ParseHttp(connection_fd, buffer, &http_msg);

        size_t n_bytes = PrintHttpRequest(&http_msg);

        memset(buffer, 0, sizeof(buffer));

        close(connection_fd);
    }

    return 0;
}

static char *SubStr(const char *s, size_t i, size_t j)
{
    if (j < i)
        return NULL;

    size_t len = j - i; // [i, j)
    char *out = malloc(len + 1);
    if (!out)
        return NULL;

    memcpy(out, s + i, len);
    out[len] = '\0';

    return out;
}

static char *HttpRequestToString(http_request_t *http_msg, char *buffer)
{
    size_t i = 0;
    for (i = 0; i < MAXHEADER_NUM; ++i)
    {
        sprintf(buffer,
                "%.*s: %.*s",
                http_msg->headers[i].key->size,
                http_msg->headers[i].key->data,
                http_msg->headers[i].val->size,
                http_msg->headers[i].val->data);
    }

    return NULL;
}
