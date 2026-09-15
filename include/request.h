#pragma once

#include <stdint.h>

/** Immutable request classification; unsupported methods retain their token in
 * the connection state while sharing REQUEST_UNSUPPORTED here.
 */
typedef struct Request Request_t;

#define NUM_REQUESTS 3
extern const Request_t REQUEST_GET;
extern const Request_t REQUEST_PUT;
extern const Request_t REQUEST_UNSUPPORTED;
extern const Request_t *requests[NUM_REQUESTS];

/** Return a borrowed static method name for a non-NULL request type. */
const char *request_get_str(const Request_t *);
