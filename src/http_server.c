#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "http_server.h"
#include "utils.h"

#define UNUSED(x) (void)x

size_t parse_u64_n(const char *s, size_t n)
{
    size_t val = 0;

    for (size_t i = 0; i < n; i++)
    {
        if (s[i] < '0' || s[i] > '9')
            break;
        val = val * 10 + (s[i] - '0');
    }
    return val;
}

int strncmp_lower(char *s1, char *s2, size_t n)
{
    while (n > 0 && *s1 && (tolower(*s1) == tolower(*s2)))
    {
        ++s1;
        ++s2;
        --n;
    }

    return (n == 0) ? 0 : (tolower(*s1) - tolower(*s2));
}

http_status_t parse_http_request(int connection_fd, char *buffer, http_request_t *http_msg)
{

    parser_state_t p_state = {0};
    p_state.connection_fd = connection_fd;
    p_state.buffer = buffer;
    p_state.chunked = -1;
    p_state.state = PARSE_FIRSTLINE_HEADERS;

    FILE *file_fd = 0;
    file_fd = fopen("tmp", "rw+");

    while (SERVER_RUNNING)
    {

        switch (p_state.state)
        {
        case PARSE_FIRSTLINE_HEADERS:
        {
            parse_loop(&p_state);
            break;
        }
        case CONSUME_FIRST_LINE:
        {
            parser_consume_firstline(&p_state);
            break;
        }

        case CONSUME_HEADER:
        {
            parser_consume_header(&p_state);
            break;
        }

        case PARSE_BODY:
        {
            char *method = http_msg->s[METHOD].data;
            size_t method_size = http_msg->s[METHOD].size;

            if (0 == strncmp_lower(method, "GET", method_size))
            {
                return HTTP_OK;
            }

            if (p_state.chunked)
            {
                // read_chunks_until_0();
            }
            else if (p_state.content_length)
            {
                size_t sz = (size_t)(unsigned char)*(p_state.content_length - 1);
                size_t body_total = parse_u64_n(p_state.content_length, sz);

                // flush remaining bytes in stream, which are part of the body into file
                // "processing the data"
                if (fwrite(buffer + p_state.i, 1, p_state.bytes_left_not_read, file_fd) != 1)
                    return -1; // error

                p_state.i += p_state.bytes_left_not_read;
                size_t processed_body = p_state.bytes_left_not_read;

                size_t last_read = 0;
                // read the rest
                while ((processed_body + last_read < body_total) &&
                       (last_read += read(connection_fd, buffer + p_state.stream_length, BUFSIZE - p_state.stream_length)))
                {
                    fwrite(buffer + p_state.i, 1, last_read, file_fd);
                    processed_body += last_read;
                    last_read = 0;
                }
            }
            // else if (no_body_expected)
            //     body_length = 0;
            // else
            //     read_until_connection_close();
            break;
        }
        }
    }
}

int main(int argc, char **argv)
{
    UNUSED(argc);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int port = atoi(argv[1]);
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_fd, SOMAXCONN);

    int connection_fd = 0;
    struct sockaddr_in client_addr = {0};
    socklen_t client_len = sizeof(client_addr);

    char buffer[4 * KB] = {0};
    http_request_t http_msg = {0};

    while (SERVER_RUNNING)
    {
        connection_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        memset(&http_msg, 0, sizeof(http_msg));
        int r = 0;
        if (HTTP_OK != (r = parse_http_request(connection_fd, buffer, &http_msg)))
        {
            return r;
        }
        memset(buffer, 0, sizeof(buffer));
        close(connection_fd);
    }

    return 0;
}
