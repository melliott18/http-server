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

int listener_init(Listener_Socket *sock, int port) {
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if ((sock->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    int reuse = 1;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        goto failed;
    }

    if (bind(sock->fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        goto failed;
    }

    if (listen(sock->fd, 128) < 0) {
        goto failed;
    }

    return 0;

failed: {
    int saved_errno = errno;
    close(sock->fd);
    sock->fd = -1;
    errno = saved_errno;
    return -1;
}
}

int listener_accept(Listener_Socket *sock) {
    int connfd = accept(sock->fd, NULL, NULL);
    return connfd;
}
