#include "request.h"

struct Request {
    const char *str;
};

const Request_t REQUEST_GET = { "GET" };
const Request_t REQUEST_PUT = { "PUT" };
const Request_t REQUEST_UNSUPPORTED = { "UNSUPPORTED" };

const Request_t *requests[NUM_REQUESTS] = { &REQUEST_GET, &REQUEST_PUT, &REQUEST_UNSUPPORTED };

const char *request_get_str(const Request_t *req) {
    return req->str;
}
