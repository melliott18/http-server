#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "hashtable.h"
#include "request.h"
#include "response.h"
#include "queue.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/file.h>
#include <sys/stat.h>

#define DEFAULT_THREAD_COUNT 4
#define MAX_THREAD_COUNT     1024
#define RID                  "Request-Id"

// Globals
HashTable_t *ht = NULL;
pthread_mutex_t mu;
queue_t *workq = NULL;

void *handle_connection(void *arg);
void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

char *get_rid(conn_t *conn) {
    char *id = conn_get_header(conn, RID);
    if (id == NULL) {
        id = "0";
    }
    return id;
}

void audit_log(const char *name, const char *uri, const char *id, int code) {
    // <Oper>,<URI>,<Status-Code>,<RequestID header value>\n
    fprintf(stderr, "%s,/%s,%d,%s\n", name, uri, code, id);
}

void usage(FILE *stream, char *exec) {
    fprintf(stream, "usage: %s [-t threads] <port>\n", exec);
}

static int positive_number(const char *value, long maximum) {
    char *end = NULL;
    if (value[0] < '0' || value[0] > '9') {
        return -1;
    }
    errno = 0;
    long number = strtol(value, &end, 10);
    if (errno || *end || number < 1 || number > maximum) {
        return -1;
    }
    return (int) number;
}

int main(int argc, char **argv) {
    int opt = 0;
    int threads = DEFAULT_THREAD_COUNT;
    pthread_t *threadids;

    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    while ((opt = getopt(argc, argv, "t:h")) != -1) {
        switch (opt) {
        case 't':
            threads = positive_number(optarg, MAX_THREAD_COUNT);
            if (threads < 0) {
                errx(EXIT_FAILURE, "threads must be between 1 and %d", MAX_THREAD_COUNT);
            }
            break;
        case 'h': usage(stdout, argv[0]); return EXIT_SUCCESS;
        default: usage(stderr, argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind != argc - 1) {
        warnx("wrong arguments: %s port_num", argv[0]);
        usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    int port = positive_number(argv[optind], 65535);
    if (port < 0) {
        warnx("port must be between 1 and 65535: %s", argv[optind]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    if (listener_init(&sock, port) < 0) {
        warnx("Cannot open listener sock: %s", argv[0]);
        return EXIT_FAILURE;
    }

    threadids = malloc(sizeof(pthread_t) * threads);
    workq = queue_new(threads);
    ht = ht_create(threads);
    if (!threadids || !workq || !ht) {
        errx(EXIT_FAILURE, "cannot allocate server workers");
    }

    int rc = pthread_mutex_init(&mu, NULL);
    if (rc) {
        errx(EXIT_FAILURE, "pthread_mutex_init failed: %d", rc);
    }

    for (int i = 0; i < threads; ++i) {
        int rc = pthread_create(threadids + i, NULL, handle_connection, NULL);
        if (rc != 0) {
            warnx("Cannot create %d pthreads", threads);
            return EXIT_FAILURE;
        }
    }

    while (1) {
        int connfd = listener_accept(&sock);
        if (connfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            err(EXIT_FAILURE, "cannot accept connection");
        }
        debug("accepted %d\n", connfd);
        if (!queue_push(workq, (void *) (intptr_t) connfd)) {
            close(connfd);
            err(EXIT_FAILURE, "cannot queue connection");
        }
    }

    queue_delete(&workq);

    return EXIT_SUCCESS;
}

void *handle_connection(void *arg) {
    (void) arg;
    while (true) {
        void *item = NULL;
        conn_t *conn = NULL;

        if (!queue_pop(workq, &item)) {
            return NULL;
        }
        int connfd = (int) (intptr_t) item;

        debug("popped off %d", connfd);
        conn = conn_new(connfd);
        if (!conn) {
            close(connfd);
            continue;
        }

        const Response_t *res = conn_parse(conn);

        if (res != NULL) {
            conn_send_response(conn, res);
        } else {
            const Request_t *req = conn_get_request(conn);
            if (req == &REQUEST_GET) {
                handle_get(conn);
            } else if (req == &REQUEST_PUT) {
                handle_put(conn);
            } else {
                handle_unsupported(conn);
            }
        }

        conn_delete(&conn);
        close(connfd);
    }
    return NULL;
}

void handle_get(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    debug("handling GET request for %s", uri);
    const Response_t *res = NULL;
    uint64_t content_length = 0;
    int16_t code = 200;

    pthread_mutex_lock(&mu);

    rwlock_t *rwlock = ht_lookup(ht, uri);

    if (!rwlock) {
        rwlock = ht_insert(ht, uri);
    }

    pthread_mutex_unlock(&mu);

    if (!rwlock) {
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        audit_log("GET", uri, get_rid(conn), 500);
        return;
    }
    reader_lock(rwlock);

    // Open the file.
    int fd = open(uri, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) {
        debug("%s: %s", uri, strerror(errno));
        if (errno == EACCES || errno == ELOOP) {
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }

    // Get the size of the file.
    struct stat statbuf;
    if (fstat(fd, &statbuf) < 0) {
        debug("%s: %s", uri, strerror(errno));
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        goto out;
    }

    content_length = statbuf.st_size;

    // Check if "file" is a directory.
    if (!S_ISREG(statbuf.st_mode)) {
        debug("%s: %s", uri, strerror(EISDIR));
        res = &RESPONSE_FORBIDDEN;
        goto out;
    }

    // Send file to the client.
    conn_send_file(conn, fd, content_length);

out:
    if (res != NULL) {
        conn_send_response(conn, res);
        code = response_get_code(res);
    }

    audit_log("GET", uri, get_rid(conn), code);
    debug("finished handling GET request for %s", uri);

    reader_unlock(rwlock);
    if (fd >= 0) {
        close(fd);
    }
}

void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    const Response_t *res = &RESPONSE_INTERNAL_SERVER_ERROR;
    debug("handling put request for %s", uri);

    // The underscore keeps private staging names outside the allowed URI syntax.
    char temp[] = ".httpserver_upload-XXXXXX";
    int tempfd = mkstemp(temp);
    bool locked = false;
    rwlock_t *rwlock = NULL;
    if (tempfd < 0) {
        goto out;
    }

    pthread_mutex_lock(&mu);

    rwlock = ht_lookup(ht, uri);

    if (!rwlock) {
        rwlock = ht_insert(ht, uri);
    }

    pthread_mutex_unlock(&mu);
    if (!rwlock) {
        goto out;
    }

    // Receive the file from the client.
    res = conn_recv_file(conn, tempfd);

    if (res != NULL) {
        goto out;
    }
    if (close(tempfd) < 0) {
        tempfd = -1;
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        goto out;
    }
    tempfd = -1;

    writer_lock(rwlock);
    locked = true;

    struct stat statbuf;
    bool existed = lstat(uri, &statbuf) == 0;
    if ((existed && (!S_ISREG(statbuf.st_mode) || access(uri, W_OK) != 0))
        || (!existed && errno != ENOENT)) {
        res = &RESPONSE_FORBIDDEN;
        goto out;
    }
    if (rename(temp, uri) < 0) {
        res = (errno == EACCES || errno == EPERM || errno == EISDIR || errno == ENOENT)
                  ? &RESPONSE_FORBIDDEN
                  : &RESPONSE_INTERNAL_SERVER_ERROR;
        goto out;
    }
    res = existed ? &RESPONSE_OK : &RESPONSE_CREATED;

out:
    if (tempfd >= 0) {
        close(tempfd);
    }
    unlink(temp);
    conn_send_response(conn, res);
    audit_log("PUT", uri, get_rid(conn), response_get_code(res));
    debug("finished handling PUT request for %s", uri);

    if (locked) {
        writer_unlock(rwlock);
    }
}

void handle_unsupported(conn_t *conn) {
    debug("handling unsupported request");

    // Send responses.
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
    audit_log(conn_get_method(conn), conn_get_uri(conn), get_rid(conn), 501);
}
