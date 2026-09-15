#pragma once

#include "debug.h"

// Accepted request-line syntax. The handler validates filesystem safety.
#define TYPE_REGEX   "([a-zA-Z]{1,8})"
#define FNAME_REGEX  "/([a-zA-Z0-9.-]{1,63})"
#define HTTP_REGEX   "(HTTP/[0-9]\\.[0-9])"
#define HTTP_VERSION "HTTP/1.1"

// Header names and printable ASCII values are limited to 128 characters each.
#define HEADER_FIELD_REGEX "([a-zA-Z0-9.-]{1,128})"
#define HEADER_VALUE_REGEX "([ -~]{1,128})"

// Maximum aggregate header bytes, including the terminating empty line.
#define MAX_HEADER_LEN 2048

// Saved-header declarations: diagnostic label, public lookup name, member name.
#define SAVE_HEADERS                                                                               \
    X("cl", "Content-Length", cl)                                                                  \
    X("rid", "Request-Id", rid)
