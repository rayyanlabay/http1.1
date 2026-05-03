#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "http_server.h"
#include "utils.h"

#define UNUSED(x) (void)x
#define KB 1024
#define BUFSIZE 4 * KB
#define SERVER_RUNNING 1

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
    size_t bytes_read = 0, crlf_start = 0;
    size_t window_start = 0, i = 0;
    size_t header_i = 0;
    size_t stream_length = 0;
    size_t bytes_left_not_read = 0;

    char *content_length = NULL;
    size_t chunked = -1;

    FILE *file_fd = 0;
    file_fd = fopen("tmp", "rw+");

    int first_line_read = 0;
    int state = PARSE_FIRSTLINE_HEADERS;
    while (SERVER_RUNNING)
    {
        switch (state)
        {
        case PARSE_FIRSTLINE_HEADERS:
        {
            while (bytes_left_not_read--)
            {
                if (buffer[i] == '\r' && buffer[i + 1] == '\n')
                {
                    crlf_start = i;

                    if (i < stream_length - 3 && (buffer[i + 2] == '\r' && buffer[i + 3] == '\n'))
                    {
                        i += 4;
                        state = PARSE_BODY;
                    }
                    else
                    {
                        i += 2;
                        state = first_line_read ? CONSUME_HEADER : CONSUME_FIRST_LINE;
                    }
                    break;
                }
                ++i;
            }

            while (state == PARSE_FIRSTLINE_HEADERS &&
                   (bytes_read += read(connection_fd, buffer + stream_length, BUFSIZE - stream_length)) > 0)
            {
                stream_length += bytes_read;
                bytes_read = 0;

                for (; i < stream_length - 1; ++i)
                {
                    if (buffer[i] == '\r' && buffer[i + 1] == '\n')
                    {
                        crlf_start = i;

                        bytes_left_not_read = stream_length - i - 2;

                        if (i < stream_length - 3 && (buffer[i + 2] == '\r' && buffer[i + 3] == '\n'))
                        {
                            i += 4;
                            state = PARSE_BODY;
                        }
                        else
                        {
                            i += 2;
                            state = first_line_read ? CONSUME_HEADER : CONSUME_FIRST_LINE;
                        }

                        break;
                    }
                }
            }
            break;
        }
        case CONSUME_FIRST_LINE:
        {
            char *str = buffer + window_start;
            char *s1 = NULL, *s2 = NULL, *s_end = str + crlf_start;
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

            first_line_read = 1;
            state = PARSE_FIRSTLINE_HEADERS;
            window_start = crlf_start + 2;
            break;
        }

        case CONSUME_HEADER:
        {
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

            if (chunked)
            {
                // read_chunks_until_0();
            }
            else if (content_length)
            {
                size_t sz = (size_t)(unsigned char)*(content_length - 1);
                size_t body_total = parse_u64_n(content_length, sz);

                // flush remaining bytes in stream, which are part of the body into file
                // "processing the data"
                if (fwrite(buffer + i, 1, bytes_left_not_read, file_fd) != 1)
                    return -1; // error

                i += bytes_left_not_read;
                size_t processed_body = bytes_left_not_read;

                size_t last_read = 0;
                // read the rest
                while ((processed_body + last_read < body_total) &&
                       (last_read += read(connection_fd, buffer + stream_length, BUFSIZE - stream_length)))
                {
                    fwrite(buffer + i, 1, last_read, file_fd);
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

    // bind server file descriptor to socket
    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // SOMAXCONN is max connections allowed
    listen(server_fd, SOMAXCONN);

    // this will hold the connection
    int connection_fd = 0;
    struct sockaddr_in client_addr = {0};
    socklen_t client_len = sizeof(client_addr);

    char buffer[4 * KB] = {0};

    http_request_t http_msg = {0};

    while (SERVER_RUNNING)
    {

        connection_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        memset(&http_msg, 0, sizeof(http_msg));

        // misses body, only GET version
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
