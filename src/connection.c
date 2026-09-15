
#include "buffered_socket.h"
#include "connection.h"
#include "protocol.h"
#include "response.h"
#include "request.h"

#include <errno.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <string.h>
#include <strings.h>
#include <sys/types.h>

struct Conn {
    const Request_t *type;
    char method[9];
    BufferedSocket_t *bs;
    char *URI;

#define X(str, longstr, name) char *name;
    SAVE_HEADERS
#undef X
};

// Constructor
conn_t *conn_new(int connfd) {
    conn_t *conn = (conn_t *) calloc(1, sizeof(conn_t));
    if (!conn) {
        return NULL;
    }

    conn->type = &REQUEST_UNSUPPORTED;
    conn->URI = NULL;
    conn->bs = bs_new(connfd, MAX_HEADER_LEN + 1);
    if (!conn->bs) {
        free(conn);
        return NULL;
    }

#define X(str, longstr, name) conn->name = NULL;
    SAVE_HEADERS
#undef X

    return conn;
}

// Destructor
void conn_delete(conn_t **ppconn) {
    conn_t *pconn = *ppconn;
    bs_delete(&(pconn->bs));

    if (pconn->URI) {
        free(pconn->URI);
        pconn->URI = NULL;
    }

#define X(str, longstr, name)                                                                      \
    if (pconn->name != NULL)                                                                       \
        free(pconn->name);
    SAVE_HEADERS
#undef X

    free(pconn);
    *ppconn = NULL;
}

//////////////////////////////////////////////////////////////////////
// Parsing code.
//
// Helper functions:

const Response_t *parse_request_line(conn_t *conn) {
    char *buffer = NULL;
    uint16_t buff_len = 0;
    if (bs_read_until(conn->bs, &buffer, &buff_len, "\r\n") != BR_OK) {
        return &RESPONSE_BAD_REQUEST;
    }

    regex_t re;
    regmatch_t matches[4];
    const Response_t *res = NULL;
    if (regcomp(&re, "^" TYPE_REGEX " " FNAME_REGEX " " HTTP_REGEX "\r\n$", REG_EXTENDED)) {
        free(buffer);
        return &RESPONSE_INTERNAL_SERVER_ERROR;
    }
    int rc = regexec(&re, buffer, 4, matches, 0);
    if (rc != 0 || matches[0].rm_eo != buff_len) {
        res = &RESPONSE_BAD_REQUEST;
    } else {
        char *type = buffer;
        char *fname = buffer + matches[2].rm_so;
        char *ver = buffer + matches[3].rm_so;
        buffer[matches[1].rm_eo] = 0;
        buffer[matches[2].rm_eo] = 0;
        buffer[matches[3].rm_eo] = 0;
        memcpy(conn->method, type, strlen(type) + 1);
        for (int i = 0; i < NUM_REQUESTS; ++i) {
            if (strcmp(type, request_get_str(requests[i])) == 0) {
                conn->type = requests[i];
                break;
            }
        }
        conn->URI = strdup(fname);
        if (!conn->URI) {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
        } else if (strcmp(ver, HTTP_VERSION)) {
            res = &RESPONSE_VERSION_NOT_SUPPORTED;
        }
    }
    regfree(&re);
    free(buffer);
    return res;
}

const Response_t *parse_headers(conn_t *conn) {
    regex_t re;
    if (regcomp(&re, "^" HEADER_FIELD_REGEX ": " HEADER_VALUE_REGEX "\r\n$", REG_EXTENDED)) {
        return &RESPONSE_INTERNAL_SERVER_ERROR;
    }
    const Response_t *res = NULL;
    size_t total = 0;
    for (;;) {
        char *buffer = NULL;
        uint16_t buff_len = 0;
        if (bs_read_until(conn->bs, &buffer, &buff_len, "\r\n") != BR_OK) {
            res = &RESPONSE_BAD_REQUEST;
            break;
        }
        total += buff_len;
        if (total > MAX_HEADER_LEN) {
            res = &RESPONSE_BAD_REQUEST;
        } else if (buff_len == 2) {
            free(buffer);
            break;
        } else {
            regmatch_t matches[3];
            int rc = regexec(&re, buffer, 3, matches, 0);
            if (rc != 0 || matches[0].rm_eo != buff_len) {
                res = &RESPONSE_BAD_REQUEST;
            } else {
                char *key = buffer;
                char *value = buffer + matches[2].rm_so;
                buffer[matches[1].rm_eo] = 0;
                buffer[matches[2].rm_eo] = 0;
                char **saved = NULL;
                if (strcasecmp(key, "Content-Length") == 0) {
                    saved = &conn->cl;
                } else if (strcasecmp(key, "Request-Id") == 0) {
                    saved = &conn->rid;
                } else if (strcasecmp(key, "Transfer-Encoding") == 0) {
                    // This server only accepts bodies with a fixed Content-Length.
                    res = &RESPONSE_BAD_REQUEST;
                }
                if (saved) {
                    if (*saved) {
                        res = &RESPONSE_BAD_REQUEST;
                    } else {
                        *saved = strdup(value);
                        if (!*saved) {
                            res = &RESPONSE_INTERNAL_SERVER_ERROR;
                        }
                    }
                }
            }
        }
        free(buffer);
        if (res) {
            break;
        }
    }
    regfree(&re);
    return res;
}

// Parse the data from connection. Checks static correctness (i.e.,
// that each field fits within our required bounds), but does not
// check for semantic correctness (e.g., does not check that a URI is
// not a directory).
const Response_t *conn_parse(conn_t *conn) {

    const Response_t *res = NULL;

    res = parse_request_line(conn);
    if (res == NULL) {
        res = parse_headers(conn);

        if (res == NULL) {
            const char *length = conn_get_header(conn, "Content-Length");
            if (!length && conn->type == &REQUEST_PUT) {
                res = &RESPONSE_BAD_REQUEST;
            } else if (length) {
                char *end = NULL;
                errno = 0;
                unsigned long long count = strtoull(length, &end, 10);
                if (length[0] < '0' || length[0] > '9' || *end || errno == ERANGE
                    || count > INT64_MAX) {
                    res = &RESPONSE_BAD_REQUEST;
                }
            }
        }
    }

    return res;
}

//////////////////////////////////////////////////////////////////////
// Functions that get stuff we might need elsewhere from a connection

// Return the RequestType from parsing.
const Request_t *conn_get_request(conn_t *conn) {
    return conn->type;
}

const char *conn_get_method(conn_t *conn) {
    return conn->method;
}

// Return URI from parsing.
char *conn_get_uri(conn_t *conn) {
    return conn->URI;
}

char *conn_get_header(conn_t *conn, char *header) {

#define X(str, longstr, name)                                                                      \
    if (!strncmp(header, longstr, sizeof(longstr))) {                                              \
        return conn->name;                                                                         \
    } else
    SAVE_HEADERS {
        return NULL;
    }
#undef X

    return NULL;
}

//////////////////////////////////////////////////////////////////////
// Functions that help get data from a connection

// write the data from the connection into the file (fd).
const Response_t *conn_recv_file(conn_t *conn, int fd) {

    const Response_t *res = NULL;
    uint64_t cl = strtoull(conn_get_header(conn, "Content-Length"), NULL, 10);

    debug("content length: %lu (%s)", cl, conn_get_header(conn, "Content-Length"));
    BufferedResult br = bs_recvfile(conn->bs, fd, cl);

    if (br == BR_INCOMPLETE)
        res = &RESPONSE_BAD_REQUEST;
    else if (br != BR_OK)
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
    return res;
}

//////////////////////////////////////////////////////////////////////
// Functions that help write responses to the client:

// send a message body from the file (fd)
const Response_t *conn_send_file(conn_t *conn, int fd, uint64_t count) {
    char buf[MAX_HEADER_LEN + 1];

    BufferedResult res = BR_OK;
    sprintf(buf, "%s %d %s\r\nContent-Length: %lu\r\n\r\n", HTTP_VERSION,
        response_get_code(&RESPONSE_OK), response_get_message(&RESPONSE_OK), count);

    res = bs_sendbuf(conn->bs, buf, strlen(buf));
    if (res == BR_OK)
        res = bs_sendfile(conn->bs, fd, count);

    return NULL;
}

// send canonical message for a response type
const Response_t *conn_send_response(conn_t *conn, const Response_t *res) {

    char buf[MAX_HEADER_LEN + 1];

    sprintf(buf, "%s %d %s\r\nContent-Length: %lu\r\n\r\n%s\n", HTTP_VERSION,
        response_get_code(res), response_get_message(res), strlen(response_get_message(res)) + 1,
        response_get_message(res));

    bs_sendbuf(conn->bs, buf, strlen(buf));
    return NULL;
}

//Functions for debugging:

#ifdef DEBUG
char *conn_str(conn_t *conn) {
    char buf[8192] = { 0 };
    sprintf(buf + strlen(buf), "Conn {\n");
    sprintf(buf + strlen(buf), "   type: %s,\n", request_get_str(conn->type));
    sprintf(buf + strlen(buf), "    uri: %s,\n", conn->URI);
    sprintf(buf + strlen(buf), "   heads: [\n");

#define X(str, longstr, name) sprintf(buf + strlen(buf), "       " str ": %s\n", conn->name);
    SAVE_HEADERS
#undef X
    sprintf(buf + strlen(buf), "          ]\n");
    sprintf(buf + strlen(buf), "}");
    return strdup(buf);
}
#endif
