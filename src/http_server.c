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

http_status_t ParseHttp(char *buffer, http_request_t *http_msg)
{
    // current_parse_ptr is where the parser at
    char *current_parse_ptr = buffer;
    // next_ptr will point at the next part
    char *next_ptr = NULL;

    // fix later to size_t
    char size = 0;

    // memory pool will look like this: |size|data...|size|data.. etc
    pool_t *mem_pool_ptr = PoolCreate();

    // i represents the start line portions (GET / HTTP/1.1)
    size_t i = 0;
    while (0 != memcmp(current_parse_ptr - 1, "\r\n", 2))
    {
        // find delimiter ' ' - space
        // next_ptr points to the delimeter address
        if (i < 2)
        {
            next_ptr = strchr(current_parse_ptr, ' ');
        }
        else
        {
            next_ptr = strstr(current_parse_ptr, "\r\n");
        }
        size = next_ptr - current_parse_ptr;

        // assume for simplicity, size can be represented using one char
        PoolWrite(mem_pool_ptr, &size, 1);

        // point the string after the size
        http_msg->s[i] = PoolContentEnd(mem_pool_ptr);
        PoolWrite(mem_pool_ptr, current_parse_ptr, (size_t)size);

        // prepare for next string
        ++i;
        current_parse_ptr = next_ptr + 1;
    }

    if (i != 3)
    {
        // missing information in the start line
        return HTTP_BAD_REQUEST;
    }

    unsigned int now_content_length_header = 0;
    char *content_length = NULL;
    if (0 == memcmp(http_msg->s[0], "GET", 3))
    {
        content_length = "0";
    }

    /* parse headers */
    next_ptr = NULL;
    ++current_parse_ptr;
    // +2 to skip <cr-lf>
    char *headers_start = current_parse_ptr;

    while (0 != memcmp(current_parse_ptr, "\r\n", 2) && (next_ptr = strchr(current_parse_ptr, ':')) != NULL)
    {
        // upcoming key string size, e.g. "content-lenght" is 14
        // fix later from char to size_t
        char key_size = (next_ptr - current_parse_ptr);

        PoolWrite(mem_pool_ptr, &key_size, 1);
        http_msg->headers_start->key = PoolContentEnd(mem_pool_ptr);

        if (0 == memcmp(current_parse_ptr, "content-length", 14))
        {
            // flag that there was content length header
            now_content_length_header = 1;
        }

        // now write the key string
        PoolWrite(mem_pool_ptr, current_parse_ptr, (size_t)key_size);

        ++next_ptr;
        // skip all whitespaces
        while (0 != isspace(*next_ptr))
            ++next_ptr;

        current_parse_ptr = next_ptr;
        // while is for checking if it is not '\r' and it is "\r\n"
        if (NULL == (next_ptr = strstr(current_parse_ptr, "\r\n")))
        {
            return HTTP_BAD_REQUEST;
        }

        char *tmp = next_ptr;
        --next_ptr;
        // trim if there are trailing spaces
        while (0 != isspace(*next_ptr))
            --next_ptr;
        ++next_ptr;
        // copy contents of buffer into area of val in the memory_pool
        // fix later size issues from char to size_t
        char val_size = (next_ptr - current_parse_ptr);
        PoolWrite(mem_pool_ptr, &val_size, 1);
        http_msg->headers_start->val = PoolContentEnd(mem_pool_ptr);

        if (now_content_length_header)
        {
            content_length = PoolContentEnd(mem_pool_ptr);
            now_content_length_header = 0;
        }

        PoolWrite(mem_pool_ptr, current_parse_ptr, (size_t)val_size);
        // tmp is pointing to crlf, + 2 is next line's first character
        current_parse_ptr = tmp + 2;
    }

    // if there were no headers at all not valid in HTTP/1.1
    if (current_parse_ptr + 2 == headers_start)
    {
        return HTTP_BAD_REQUEST;
    }

    if (!strcmp(content_length, "0"))
    {
        return HTTP_OK;
    }
    // parse the body
    // Transfer-Encoding: chunked do later
    // currently support content-length header only
    size_t len = StringToSizeT(content_length);

    PoolWrite(mem_pool_ptr, &content_length, sizeof(size_t));
    PoolWrite(mem_pool_ptr, current_parse_ptr, len);
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
        http_header_entry_t hent = {0};
        while ((bytes = read(connection_fd, buffer, sizeof(buffer))) > 0)
        {
            memset(&http_msg, 0, sizeof(http_msg));
            memset(&hent, 0, sizeof(hent));

            http_msg.headers_start = &hent;
            // http_msg.headers_end = calloc(0, sizeof(http_header_entry_t));
            ParseHttp(buffer, &http_msg);
            char *body = "Hello from server\n";

            snprintf(
                buffer,
                sizeof(buffer),
                "%s %s %s\r\n",
                http_msg.s[0],
                http_msg.s[1],
                http_msg.s[2]);

            // |sizeof s1|s1..|sizeof s2|s2..|sizeof s3|s3..|
            size_t len = (size_t)(http_msg.s[0] - 1) + (size_t)(http_msg.s[1] - 1) + (size_t)(http_msg.s[2] - 1);
            size_t n_bytes = HttpRequestToString(&http_msg, buffer + len, http_msg.headers_start, http_msg.headers_end);

            int len = snprintf(
                buffer + n_bytes + len,
                sizeof(buffer) - n_bytes - len,
                "Content-Length: %zu\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                strlen(body),
                body);

            if (-1 == write(connection_fd, buffer, len))
            {
                return 0;
            }

            memset(&buffer, 0, sizeof(buffer));

            if (-1 == write(connection_fd, buffer, len))
            {
                return 0;
            }
            // actions on http_msg
        }

        memset(buffer, 0, sizeof(buffer));

        // printf http_msg
        // printf("Received: %.*s\n", bytes, buffer);

                close(connection_fd);
    }

    return 0;
}

static char *HttpRequestToString(http_request_t *http_msg, char *buffer, http_header_entry_t *headers_start, http_header_entry_t *headers_end)
{

    http_header_entry_t *p = headers_start;
    size_t i = 0;
    size_t n_bytes = 0;
    // http_header_entry_t is like an iterator
    // move until we reach the last key-val
    // we increase pointer by the size of the string
    // our memory pool stores like this:
    // |sizeof s1|s1..|sizeof s2|s2..|sizeof s3|s3..|
    while (p->val + *(p->val - 1) != headers_end->val + *(headers_end->val - 1))
    {
        n_bytes = snprintf(
            buffer + i,
            sizeof(buffer) - i,
            "%s: %s\r\n", p->key, p->val);
        i += n_bytes;

        p->key = p->val + *(p->val - 1) + 1;
        p->val = p->key + *(p->key - 1) + 1;
    }
}
