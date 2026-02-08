#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "header.h"

[[nodiscard]]
struct String
read_entire_file(char const* filename)
{
        auto file = fopen(filename, "rb");
        if (!file) {
                return (struct String){0};
        }
        struct stat st;
        if (stat(filename, &st) == -1 || st.st_size == 0) {
                fclose(file);
                return (struct String){0};
        }
        char* buf = malloc(st.st_size + 1);
        if (!buf) {
                return (struct String){0};
        }
        size_t n = fread(buf, 1, st.st_size, file);
        if (n <= 0) {
                free(buf);
                return (struct String){0};
        }
        buf[n] = '\0';
        return (struct String){.data = buf, .size = n};
}
