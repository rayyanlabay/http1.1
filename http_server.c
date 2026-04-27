#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define SERVER_RUNNING 1

#define KB 1024
#define HEADER_POOL_SIZE 8 * KB

#define METHOD_PTR 0
#define PATH_PTR 1
#define PROTOCOL_PTR 2

void ParseHttp(char *buffer, http_request_t *http_msg)
{
    /* parse the start line of the http message */
    char *current_parse_point = buffer;
    char *start_line[3] = {0};

    size_t i = 0;
    while (!strcmp(current_parse_point, "\r\n"))
    {
        char *next_point = strchr(current_parse_point, '/');
        start_line[i++] = current_parse_point;
        current_parse_point = next_point + 1;
    }

    strncpy(http_msg->method, buffer, start_line[PATH_PTR] - buffer);
    strncpy(http_msg->path, start_line[PATH_PTR] + 1, start_line[PROTOCOL_PTR] - start_line[PATH_PTR]);
    strncpy(http_msg->protocol, start_line[PROTOCOL_PTR] + 1, current_parse_point - start_line[PATH_PTR]);

    /* parse headers */
    char *next_point = NULL;
    char *headers_start = current_parse_point + 2;

    // helper struct, for readability
    http_header_entry_t h_ent = {0};

    while (!strcmp(current_parse_point, "\r\n") && (next_point = strchr(current_parse_point, ':')) != NULL)
    {
        size_t key_size = (next_point - current_parse_point);
        size_t key_index = (current_parse_point - headers_start);

        h_ent.key = mem_pool + key_index;
        strncpy(h_ent.key, current_parse_point, key_size);

        // advance in 2 chars, because first one is null terminator of the key
        size_t val_index = key_size + 2;
        h_ent.val = h_ent.key + val_index;

        // now current parse point is at the start of the val after :
        current_parse_point = next_point + 1;

        // continue untill cr-lf
        next_point = strchr(current_parse_point, '\r');

        // copy contents of buffer into area of val in the memory_pool
        size_t val_size = (next_point - current_parse_point);
        strncpy(h_ent.val, current_parse_point, val_size);

        // update http_msg
        http_msg->headers.entries_start = mem_pool;
    }

    http_msg->headers.entries_end = mem_pool + (current_parse_point - headers_start);

    // parse the body
    if (!strcmp(current_parse_point, "\r\n"))
    {
        return;
    }

    char *end_of_body = strstr(current_parse_point, "\r\n");
    size_t body_size = end_of_body - current_parse_point;

    http_msg->body = calloc(0, body_size + 1);
    strcnpy(http_msg->body, current_parse_point, body_size);
}

// GET /api/user/124 HTTP/1.1
// Host: example.com
// User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)
// Accept: application/json
// Authorization: Bearer my-secret-token-123
// Connection: keep-alive

char mem_pool[HEADER_POOL_SIZE] = {0};

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

#define STARTLINE_SIZE 20
typedef struct
{
    char method[STARTLINE_SIZE];
    char path[STARTLINE_SIZE];
    char protocol[STARTLINE_SIZE];

    header_list_t headers;

    // later change to grab from memory pool
    char *body;
} http_request_t;

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // bind server file descriptor to socket
    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // SOMAXCONN is max connections allowed
    listen(server_fd, SOMAXCONN);

    // this will hold the connection
    int connection_fd = 0;
    struct sockaddr_in client_addr = {0};
    socklen_t client_len = sizeof(client_addr);

    // passed message
    char buffer[1024] = {0};

    while (SERVER_RUNNING)
    {

        connection_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        int bytes = read(connection_fd, buffer, sizeof(buffer));

        http_request_t http_msg = {0};
        ParseHttp(&buffer, &http_msg);

        // printf http_msg
        printf("Received: %.*s\n", bytes, buffer);

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
        write(connection_fd, buffer, len);
        memset(&buffer, 1024, 0);
        close(connection_fd);
    }

    return 0;
}
