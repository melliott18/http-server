/**
 * @file socket_io.h
 *
 * Blocking TCP listener and file-descriptor transfer helpers.
 *
 * @author Andrew Quinn
 */

#pragma once

#include <stdint.h>
#include <sys/types.h>

/** IPv4 listener bound to all host interfaces. */
typedef struct {
    /** Owned descriptor; close it after accepting connections has stopped. */
    int fd;
} Listener_Socket;

/** Initialize a new blocking listener with SO_REUSEADDR and a backlog of 128.
 * sock must be non-NULL and must not already own an open descriptor. port must
 * be in 0..65535; zero requests an ephemeral port. Returns 0 on success or -1
 * with errno set and sock->fd set to -1 on failure.
 */
int listener_init(Listener_Socket *sock, int port);

/** Wait for a connection on an initialized listener.
 * Returns a blocking client descriptor owned by the caller, or -1 with errno
 * set. Interrupted accepts are reported as EINTR; callers decide whether to
 * retry. Accepted sockets have 10-second receive and send inactivity timeouts;
 * each blocking I/O call has its own timeout, rather than a request deadline.
 * Timeout setup failure closes the client descriptor and preserves errno.
 */
int listener_accept(Listener_Socket *sock);

/** Read at most n bytes, stopping after EOF or a read containing str.
 * buf must hold n bytes; n must be less than SIZE_MAX and at most SSIZE_MAX.
 * str is a NUL-terminated text delimiter, or NULL to read until n bytes or EOF.
 * Delimiter matching stops at embedded NUL bytes. A read can include bytes
 * beyond the delimiter; all bytes read are returned without NUL termination.
 * Returns the byte count, or -1 with errno set on allocation or I/O failure.
 * On failure, bytes already read are consumed but are not copied to buf.
 */
ssize_t read_until(int fd, char buf[], size_t n, char *str);

/** Read n bytes into buf, or fewer if EOF occurs.
 * buf must hold n bytes and n must be at most SSIZE_MAX. Returns the byte count
 * or -1 with errno set on I/O failure, including socket timeout. A failure can
 * occur after part of buf has been written and input has been consumed.
 */
ssize_t read_n_bytes(int fd, char buf[], size_t n);

/** Write up to n bytes from buf, stopping on error or a zero-byte write.
 * buf must hold n bytes and n must be at most SSIZE_MAX. Returns the byte count
 * or -1 with errno set on I/O failure, including socket timeout. A failure can
 * occur after partial output. Callers must handle SIGPIPE for closed sockets.
 */
ssize_t write_n_bytes(int fd, char buf[], size_t n);

/** Copy at most n bytes from src to dst, stopping at EOF or I/O failure.
 * n must be at most SSIZE_MAX. Returns bytes written or -1 with errno set on
 * error; a short count indicates an incomplete transfer. Descriptors remain
 * open, and a failure can occur after partial input consumption and output.
 *
 * All transfer helpers retry EINTR, use the descriptors' current offsets, and
 * rely on their blocking/timeout configuration. They do not impose deadlines.
 */
ssize_t pass_n_bytes(int src, int dst, size_t n);
