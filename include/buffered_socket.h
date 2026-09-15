#pragma once

#include <stdint.h>

/** Buffered connection state. The caller retains ownership of its descriptor.
 * Operations are blocking, inherit socket timeouts, and are not thread-safe.
 */
typedef struct BufferedSocket BufferedSocket_t;

/** BR_INCOMPLETE denotes EOF before the requested receive-file length. */
typedef enum { BR_OK, BR_FAILED, BR_INCOMPLETE } BufferedResult;

/** Allocate a socket buffer of size bytes; returns NULL on allocation failure.
 * connfd must be open and size must be positive.
 */
BufferedSocket_t *bs_new(int connfd, uint16_t size);

/** Free initialized state and set the supplied pointer to NULL.
 * Both pointer levels must be non-NULL. Does not close the descriptor.
 */
void bs_delete(BufferedSocket_t **);

/** Return text through and including delimiter str in newly allocated *buffer.
 * str must be nonempty NUL-terminated text; all pointers must be valid. On
 * BR_OK, *len counts bytes excluding the trailing NUL and the caller must free
 * *buffer. Bytes after the delimiter remain buffered. Returns BR_FAILED on EOF
 * without a delimiter, full buffer, allocation failure, or I/O error/timeout.
 */
BufferedResult bs_read_until(BufferedSocket_t *bs, char **buffer, uint16_t *len, char *str);

/** Consume one request body of count bytes into fd, including buffered bytes.
 * Call once after header parsing; the buffered body is not reusable afterward.
 * Returns BR_OK, BR_INCOMPLETE on early EOF, or BR_FAILED on I/O error/timeout.
 * Partial output can remain in fd on failure. count must be at most SSIZE_MAX.
 */
BufferedResult bs_recvfile(BufferedSocket_t *bs, int fd, uint64_t count);

/** Send len bytes from buffer; returns BR_OK or BR_FAILED on partial/error I/O. */
BufferedResult bs_sendbuf(BufferedSocket_t *bs, char *buffer, uint16_t len);

/** Send count bytes from fd's current offset; count must be at most SSIZE_MAX.
 * Returns BR_OK or BR_FAILED on early EOF, partial output, or I/O error/timeout.
 */
BufferedResult bs_sendfile(BufferedSocket_t *bs, int fd, uint64_t count);
