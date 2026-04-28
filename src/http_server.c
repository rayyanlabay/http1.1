#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

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
    while (0 != memcmp(current_parse_ptr, "\r\n", 2))
    {
        // find delimiter ' ' - space
        // next_ptr points to the delimeter address
        next_ptr = strchr(current_parse_ptr, ' ');
        size = next_ptr - current_parse_ptr;

        // assume for simplicity, size can be represented using one char
        PoolWrite(mem_pool_ptr, &size, 1);

        // point the string after the size
        http_msg->s[i] = PoolContentEnd(mem_pool_ptr) + 1;
        PoolWrite(mem_pool_ptr, http_msg->s[i], (size_t)size);

        // prepare for next string
        ++i;
        current_parse_ptr = next_ptr + 1;
    }

    if (i != 3)
    {
        // missing information in the start line
        return HTTP_BAD_REQUEST;
    }

    /* parse headers */
    next_ptr = NULL;
    // +2 to skip <cr-lf>
    char *headers_start = current_parse_ptr + 2;

    unsigned int now_content_length_header = 0;
    char *content_length = NULL;
    while (!memcmp(current_parse_ptr, "\r\n\r\n", 4) && (next_ptr = strchr(current_parse_ptr, ':')) != NULL)
    {
        // upcoming key string size, e.g. "content-lenght" is 14
        // fix later from char to size_t
        char key_size = (next_ptr - current_parse_ptr);
        PoolWrite(mem_pool_ptr, &key_size, 1);

        if (0 == memcmp(current_parse_ptr, "content-length", 14))
        {
            // flag that there was content length header
            now_content_length_header = 1;
        }

        // now write the key string
        PoolWrite(mem_pool_ptr, current_parse_ptr, (size_t)key_size);

        current_parse_ptr = next_ptr + 1;
        // while is for checking if it is not '\r' and it is "\r\n"
        if (NULL == (next_ptr = strstr(current_parse_ptr, "\r\n")))
        {
            return HTTP_BAD_REQUEST;
        }

        // copy contents of buffer into area of val in the memory_pool
        // fix later size issues from char to size_t
        char val_size = (next_ptr - current_parse_ptr);
        PoolWrite(mem_pool_ptr, &val_size, 1);

        if (now_content_length_header)
        {
            content_length = PoolContentEnd(mem_pool_ptr);
        }

        PoolWrite(mem_pool_ptr, current_parse_ptr, (size_t)val_size);
    }

    // if there were no headers at all not valid in HTTP/1.1
    if (current_parse_ptr + 2 == headers_start)
    {
        return HTTP_BAD_REQUEST;
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

        while ((bytes = read(connection_fd, buffer, sizeof(buffer))) > 0)
        {
            http_request_t http_msg = {0};
            ParseHttp(buffer, &http_msg);

            // actions on http_msg
        }

        memset(buffer, 0, sizeof(buffer));

        // printf http_msg
        // printf("Received: %.*s\n", bytes, buffer);

        char *body = "Hello from server\n";

        int len = snprintf(
            buffer,
            sizeof(buffer),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
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
        close(connection_fd);
    }

    return 0;
}
