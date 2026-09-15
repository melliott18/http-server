#pragma once

#include <stdint.h>

/** Immutable HTTP status and canonical reason phrase. */
typedef struct Response Response_t;

extern const Response_t RESPONSE_OK;
extern const Response_t RESPONSE_CREATED;
extern const Response_t RESPONSE_BAD_REQUEST;
extern const Response_t RESPONSE_FORBIDDEN;
extern const Response_t RESPONSE_NOT_FOUND;
extern const Response_t RESPONSE_INTERNAL_SERVER_ERROR;
extern const Response_t RESPONSE_NOT_IMPLEMENTED;
extern const Response_t RESPONSE_VERSION_NOT_SUPPORTED;

/** Return the numeric status for a non-NULL response. */
uint16_t response_get_code(const Response_t *);

/** Borrow the static reason phrase for a non-NULL response. */
const char *response_get_message(const Response_t *);
