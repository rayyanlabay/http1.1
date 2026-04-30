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

// memory pool will look like this: |size|data...|size|data.. etc
pool_t *mem_pool_ptr = NULL;

static char *TrimLeft(char *ptr)
{
    while (isspace(*ptr))
    {
        --ptr;
    }
}

static char *TrimRight(char *ptr)
{
    while (isspace(*ptr))
    {
        ++ptr;
    }
}

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

int ParseHeaders(char *str, http_request_t *http_msg, size_t crlf_index)
{
    char *s1 = strchr(str, ':');
    if (!s1)
    {
        return HTTP_BAD_REQUEST;
    }
    char *s2 = NULL;
    if (s1 + 1 == str + crlf_index)
    {
        return HTTP_BAD_REQUEST;
    }

    ++s1;
    while (s1 + 1 < str + crlf_index && !isspace(*(s1++)))
        ;
    if (isspace(*s1))
    {
        return HTTP_BAD_REQUEST;
    }
}

http_status_t
ParseHttp(int connection_fd, char *buffer, http_request_t *http_msg)
{
    char *parse_portion = buffer;
    char *read_point = parse_portion;
    size_t buf_size = sizeof(parse_portion);

    size_t total_read = 0;

    size_t crlf_index = 0;
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

    size_t prev_crlf_index = crlf_index;
    crlf_index += 2;

    parse_portion = buffer + crlf_index;

    int state = PARSE_HEADERS;

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

        if (crlf_index == prev_crlf_index + 2)
        {
            return HTTP_BAD_REQUEST;
        }

        // crlf_index points at cr-lf

        ParseHeaders(parse_portion, http_msg, crlf_index);

        char *s1 = NULL, *s2 = NULL;
        char *s_end = FindCRLF(n_bytes, current_parse_ptr);

        if (s_end == NULL)
        {
            // need more data
            // read more

            buffer += n_bytes;
            break;
        }

        // crlf read, then we can proceed

        if (http_msg->state == PARSE_FIRST_LINE)
        {
            s1 = strchr(current_parse_ptr, ' ');
            if (s1 == NULL)
            {
                // need more data
                // read more

                buffer += n_bytes;
                break;
            }
            s2 = s1 ? strchr(s1 + 1, ' ') : NULL;
            if (!s1 || !s2)
            {
                return HTTP_BAD_REQUEST;
            }

            // parsed ok

            http_msg->s[METHOD].data = current_parse_ptr;
            http_msg->s[METHOD].size = s1 - current_parse_ptr;

            http_msg->s[PATH].data = s1;
            http_msg->s[PATH].size = s2 - s1;

            http_msg->s[PROTOCOL].data = s2;
            http_msg->s[PROTOCOL].size = s_end - s2;
        }

        // check if there is data we can advance to

        if (s_end + 2 - buffer < n_bytes)
        {
            current_parse_ptr = s_end + 2;
        }
        else
        {
            // if tcp data is over, fix buffer, read again.
            // assume buffer is big

            buffer += n_bytes;
            current_parse_ptr = buffer;

            break;
        }

        // there is more data

        size_t i = 0;
        while (http_msg->state == PARSE_HEADERS)
        {
            s1 = strchr(current_parse_ptr, ':');
            s_end = strstr(current_parse_ptr, "\r\n");

            char *s_tmp = s_end;

            http_msg->headers[i].key->data = current_parse_ptr;
            http_msg->headers[i].key->size = s1 - current_parse_ptr;

            s1 = TrimRight(++s1);
            s_end = TrimLeft(--s_end);

            http_msg->headers[i].val->data = s1;
            http_msg->headers[i].val->size = s_end - s1 + 1;

            ++i;

            if (s_tmp + 2 - buffer < n_bytes)
            {
                current_parse_ptr = s_tmp + 2;
            }

            processed += current_parse_ptr - buffer;

            if (0 == memcmp(current_parse_ptr, "\r\n", 2))
            {
                current_parse_ptr += 2;
                http_msg->state = PARSE_BODY;
                break;
            }
        }
    }
    memset(buffer, 0, sizeof(buffer));
    processed = 0;
}

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

    mem_pool_ptr = PoolCreate();
    char buffer[4 * KB] = {0};
    ssize_t bytes = 0;
    while (SERVER_RUNNING)
    {

        connection_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        http_request_t http_msg = {0};
        http_header_entry_t hent = {0};

        memset(&http_msg, 0, sizeof(http_msg));
        memset(&hent, 0, sizeof(hent));

        http_msg.headers_start = &hent;

        ParseHttp(connection_fd, buffer, &http_msg);

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

// crlf_index holds the position of \r\n

/*


    while read tcp stream
    {
        is there crlf ?
        no
            continue
        yes
            break
    }

    update method, path, version

    while not body
    {
        while read tcp stream
        {
            is there crlf ?
            no
                continue
            yes
                break
        }

        update header
    }

    if(chunked-enocoding)
    {
        while read tcp stream
            is there size ?
            no
                break

            size = 0?
                stop
            no
                read_len(size)
    }
    else
        read_len(content-length)


func read_len(len) {
    while read tcp stream
    {
        process
        is len bytes read ?
        no
            continue
        yes
            break
    }
}


*/