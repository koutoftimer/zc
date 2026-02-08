#ifndef INCLUDE_STRING_HEADER_H_
#define INCLUDE_STRING_HEADER_H_

#include <stddef.h>

struct BinaryString {
        unsigned char* data;
        size_t size;
};

struct String {
        union {
                char* data;
                char const* const_data;
        };
        size_t size;
};

#endif  // INCLUDE_STRING_HEADER_H_
