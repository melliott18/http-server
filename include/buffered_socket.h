#pragma once

#include <stdint.h>

typedef struct BufferedSocket BufferedSocket_t;

typedef enum { BR_OK, BR_FAILED, BR_INCOMPLETE } BufferedResult;

// Constructor
BufferedSocket_t *bs_new(int connfd, uint16_t size);

// Destructor
void bs_delete(BufferedSocket_t **);

// read until something happens:
BufferedResult bs_read_until(BufferedSocket_t *bs, char **buffer, uint16_t *len, char *str);

BufferedResult bs_recvfile(BufferedSocket_t *bs, int fd, uint64_t count);

BufferedResult bs_sendbuf(BufferedSocket_t *bs, char *buffer, uint16_t len);
BufferedResult bs_sendfile(BufferedSocket_t *bs, int fd, uint64_t count);
