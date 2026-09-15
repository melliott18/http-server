#define _GNU_SOURCE
#include "asgn2_helper_funcs.h"

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#define BLOCK 4096

ssize_t read_until(int fd, char buf[], size_t n, char *str) {
    size_t total = 0;
    ssize_t bytes = 0;
    char *found = NULL;

    char *tmpbuf = (char *) malloc(n + 1);
    if (!tmpbuf) {
        return -1;
    }
    tmpbuf[n] = '\0';

    do {
        do {
            bytes = read(fd, tmpbuf + total, n - total);
        } while (bytes < 0 && errno == EINTR);
        if (bytes < 0) {
            free(tmpbuf);
            return bytes;
        }
        total += bytes;
        if (str != NULL) {
            tmpbuf[total] = '\0';
            found = strstr(tmpbuf, str);
        }
    } while (bytes > 0 && total < n && found == NULL);

    memcpy(buf, tmpbuf, total);
    free(tmpbuf);
    return total;
}

ssize_t read_n_bytes(int fd, char buf[], size_t n) {
    size_t total = 0;
    ssize_t bytes = 0;

    do {
        do {
            bytes = read(fd, buf + total, n - total);
        } while (bytes < 0 && errno == EINTR);
        if (bytes < 0) {
            return bytes;
        }
        total += bytes;
    } while (bytes > 0 && total < n);

    return total;
}

ssize_t write_n_bytes(int fd, char buf[], size_t n) {
    size_t total = 0;
    ssize_t bytes = 0;

    do {
        do {
            bytes = write(fd, buf + total, n - total);
        } while (bytes < 0 && errno == EINTR);
        if (bytes < 0) {
            return bytes;
        }
        total += bytes;
    } while (bytes > 0 && total < n);

    return total;
}

ssize_t pass_n_bytes(int src, int dst, size_t n) {
    size_t total = 0;
    ssize_t rbytes = 0;
    ssize_t wbytes = 0;
    char buf[BLOCK];

    do {
        size_t to_read = n < BLOCK ? n : BLOCK;
        rbytes = read_n_bytes(src, buf, to_read);
        if (rbytes < 0) {
            return rbytes;
        }
        wbytes = write_n_bytes(dst, buf, rbytes);
        if (wbytes < 0) {
            return wbytes;
        }
        n -= wbytes;
        total += wbytes;
    } while (n > 0 && rbytes > 0 && wbytes > 0);

    return total;
}
