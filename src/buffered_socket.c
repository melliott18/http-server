//Buffered Socket abstraction
// By: Andrew Quinn

#include "asgn2_helper_funcs.h"
#include "buffered_socket.h"
#include "debug.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLOCK 4096

struct BufferedSocket {
    char *buffer;
    uint16_t bufflen;
    uint16_t maxlen;

    int connfd;
};

// Constructor
BufferedSocket_t *bs_new(int connfd, uint16_t size) {

    BufferedSocket_t *bs = (BufferedSocket_t *) malloc(sizeof(BufferedSocket_t));
    if (!bs) {
        return NULL;
    }

    bs->connfd = connfd;
    bs->buffer = calloc((size_t) size + 1, 1);
    if (!bs->buffer) {
        free(bs);
        return NULL;
    }
    bs->bufflen = 0;
    bs->maxlen = size;

    return bs;
}

// Destructor
void bs_delete(BufferedSocket_t **ppbs) {
    BufferedSocket_t *pbs = (*ppbs);
    free(pbs->buffer);
    free(pbs);
    *ppbs = NULL;
}

static int bs_fill_and_shift(BufferedSocket_t *bs, char **buffer, uint16_t len) {

    (*buffer) = malloc(sizeof(char) * (len + 1));
    if (!*buffer) {
        return -1;
    }
    memcpy(*buffer, bs->buffer, len);
    (*buffer)[len] = 0;

    // shift buffer over:
    memmove(bs->buffer, bs->buffer + len, bs->bufflen - len);
    bs->bufflen -= len;
    memset(bs->buffer + bs->bufflen, 0, bs->maxlen - bs->bufflen);
    return 0;
}

// read until something happens:
BufferedResult bs_read_until(BufferedSocket_t *bs, char **buff, uint16_t *len, char *needle) {

    ssize_t rc = 2;
    char *found = NULL;
    size_t needle_len = strlen(needle);

    if (needle_len > bs->maxlen) {
        return BR_FAILED;
    }

    found = strstr(bs->buffer, needle);

    // continue until any of the following are true:
    // (1) timeout
    // (2) buffer full
    // (3) found the string

    while (rc > 0 && bs->bufflen < bs->maxlen && !found) {

        do {
            rc = read(bs->connfd, bs->buffer + bs->bufflen, bs->maxlen - bs->bufflen);
        } while (rc < 0 && errno == EINTR);

        if (rc > 0) {
            bs->bufflen += rc;
            bs->buffer[bs->bufflen] = '\0';
            found = strstr(bs->buffer, needle);
        }
    }

    if (!found) {
        return BR_FAILED;
    }

    // copy over internal buffer to the buffer argument
    (*len) = found - bs->buffer + needle_len;
    return bs_fill_and_shift(bs, buff, *len) == 0 ? BR_OK : BR_FAILED;
}

BufferedResult bs_sendbuf(BufferedSocket_t *bs, char *buffer, uint16_t len) {

    ssize_t written = write_n_bytes(bs->connfd, buffer, len);
    return written == len ? BR_OK : BR_FAILED;
}

BufferedResult bs_sendfile(BufferedSocket_t *bs, int fd, uint64_t count) {

    ssize_t written = pass_n_bytes(fd, bs->connfd, count);
    return written >= 0 && (uint64_t) written == count ? BR_OK : BR_FAILED;
}

BufferedResult bs_recvfile(BufferedSocket_t *bs, int fd, uint64_t count) {

    size_t buffered = count < bs->bufflen ? (size_t) count : bs->bufflen;
    if (buffered > 0) {
        ssize_t written = write_n_bytes(fd, bs->buffer, buffered);
        if (written < 0 || (size_t) written != buffered) {
            return BR_FAILED;
        }
        count -= buffered;
    }
    if (count == 0) {
        return BR_OK;
    }
    ssize_t written = pass_n_bytes(bs->connfd, fd, count);
    if (written < 0) {
        return BR_FAILED;
    }
    return (uint64_t) written == count ? BR_OK : BR_INCOMPLETE;
}
