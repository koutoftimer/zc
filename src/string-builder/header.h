#ifndef INCLUDE_STRING_BUILDER_HEADER_H_
#define INCLUDE_STRING_BUILDER_HEADER_H_

#include "string/header.h"

// Main purpose is to construct arbitrary sequences of printable ASCII
// characters or binary data.
//
// Supports zero initialization, e.g.:
//
//      struct StringBuilder sb = {0};  // correct
//
struct StringBuilder {
        // Using union to prevent type punning everywhere
        union {
                struct BinaryString binary;
                struct String ascii;
        };
        size_t capacity;
        bool contains_binary_data;
};

// Append printable ASCII data
[[gnu::format(printf, 2, 3)]]
bool sb_appendf(struct StringBuilder* sb, char const* fmt, ...);

// Append binary data
bool sb_append_binary(struct StringBuilder* sb, unsigned char const* data,
                      size_t size);

#endif  // INCLUDE_STRING-BUILDER_HEADER_H_
