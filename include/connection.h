#pragma once

#include "response.h"
#include "request.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/** State for one HTTP request. The caller owns the socket descriptor.
 * Operations are blocking, inherit socket timeouts, and are not thread-safe.
 */
typedef struct Conn conn_t;

/** Allocate request state for an open socket; returns NULL on allocation failure. */
conn_t *conn_new(int connfd);

/** Free initialized state and set *conn to NULL; does not close the socket.
 * conn and *conn must be non-NULL. All borrowed field pointers become invalid.
 */
void conn_delete(conn_t **conn);

/** Parse one request line and headers using the limits in protocol.h.
 * Returns NULL on success or the response describing an error. Does not access
 * the filesystem or consume the entire body. Call only once per connection.
 */
const Response_t *conn_parse(conn_t *conn);

/** Return the parsed request type, or REQUEST_UNSUPPORTED for unknown methods. */
const Request_t *conn_get_request(conn_t *conn);

/** Borrow the original method token, including unsupported methods. */
const char *conn_get_method(conn_t *conn);

/** Borrow the parsed filename without the leading slash; NULL before parsing. */
char *conn_get_uri(conn_t *conn);

/** Borrow a saved header value, or NULL if absent or unsupported.
 * header must be exactly "Content-Length" or "Request-Id"; saved values preserve
 * the request's text. Borrowed strings remain owned by conn and must not change.
 */
char *conn_get_header(conn_t *conn, char *header);

/** Receive the body into fd after successful parsing with a Content-Length.
 * Returns NULL on success, BAD_REQUEST on early EOF, or INTERNAL_SERVER_ERROR
 * on I/O failure. Partial output can remain in fd; the descriptor stays open.
 */
const Response_t *conn_recv_file(conn_t *conn, int fd);

/** Send a 200 response with count bytes from fd's current offset.
 * count must be at most SSIZE_MAX. Does not close fd. Returns NULL on success or
 * INTERNAL_SERVER_ERROR on I/O failure. Partial output may already have reached
 * the client; callers must close the connection without sending another response.
 */
const Response_t *conn_send_file(conn_t *conn, int fd, uint64_t count);

/** Send res with its canonical reason phrase as a newline-terminated body.
 * res must be non-NULL. Returns NULL on success or INTERNAL_SERVER_ERROR on send
 * failure. Partial output may already have reached the client.
 */
const Response_t *conn_send_response(conn_t *conn, const Response_t *res);

/** Allocate a diagnostic description; the caller frees it. DEBUG builds only. */
#ifdef DEBUG
char *conn_str(conn_t *conn);
#endif
