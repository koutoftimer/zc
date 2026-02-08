#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string-builder/header.h"
#include "utils/header.h"

int indentation_level     = 1;
constexpr int indentation = 4;

bool
output_escaped(char const* start, char const* end, struct StringBuilder* sb)
{
#define output(...)                                    \
        do {                                           \
                bool ok = sb_appendf(sb, __VA_ARGS__); \
                if (!ok) return false;                 \
        } while (0)

        for (auto p = start; p < end; ++p) {
                switch (*p) {
                case '\"':
                        output("%s", "\\\"");
                        break;
                case '\\':
                        output("%s", "\\\\");
                        break;
                case '\n': {
                        // combine newlines on single output line
                        for (; p < end && *p == '\n'; ++p) {
                                output("%s", "\\n");
                        }
                        if (p >= end || *p == '\0') break;
                        --p;

                        // split multi-line string
                        constexpr int quotes_index = sizeof("sb_appendf(sb,");
                        output("\"\n%*s\"",
                               indentation_level * indentation + quotes_index,
                               "");
                } break;
                case '\r':
                        output("%s", "\\r");
                        break;
                default:
                        output("%c", *p);
                }
        }

        return true;

#undef output
}

char*
trim(char* str)
{
        while (isspace(*str)) str++;
        if (*str == 0) return str;
        char* end = str + strlen(str) - 1;
        while (end > str && isspace(*end)) end--;
        end[1] = '\0';
        return str;
}

// Returns string slice to allocated cstr
[[nodiscard]]
struct String
build_header_guard(char const* filename)
{
        auto filename_len       = strlen(filename);
        struct StringBuilder sb = {0};
        if (!sb_appendf(&sb, "%s", "INCLUDE_")) {
                return (struct String){.data = nullptr};
        }
        for (size_t i = 0; i < filename_len; i++) {
                char c = toupper(filename[i]);
                if (strchr(" ./", c)) c = '_';
                if (!sb_appendf(&sb, "%c", c)) {
                        return (struct String){.data = nullptr};
                }
        }
        return sb.ascii;
}

int
translate(const char* const filename)
{
        auto content = read_entire_file(filename);
        if (!content.data) {
                perror("Could not read input file/stream");
                return EXIT_FAILURE;
        }
        char const* const src = content.data;
        struct String guard   = build_header_guard(filename);
        if (!guard.data) {
                perror("Could not build header file guard");
                return EXIT_FAILURE;
        }

        struct StringBuilder sb = {0};

#define output(...)                                     \
        do {                                            \
                bool ok = sb_appendf(&sb, __VA_ARGS__); \
                if (!ok) {                              \
                        free(guard.data);               \
                        free(sb.ascii.data);            \
                        free(content.data);             \
                        return EXIT_FAILURE;            \
                }                                       \
        } while (0)

#define output_escaped(...)                            \
        do {                                           \
                bool ok = output_escaped(__VA_ARGS__); \
                if (!ok) {                             \
                        free(guard.data);              \
                        free(sb.ascii.data);           \
                        free(content.data);            \
                        return EXIT_FAILURE;           \
                }                                      \
        } while (0)

        // Header Guard
        output("#ifndef %s_H\n#define %s_H\n\n", guard.data, guard.data);
        output("%s",
               "bool render_template("
               "struct Context* ctx, struct StringBuilder* sb);\n\n");
        output("%s", "#endif\n\n#ifdef IMPLEMENTATION\n\n");
        output("%s", "#define $ (*ctx)\n");
        output("%s",
               "#define sb_appendf(...) do { "
               "bool res = sb_appendf(__VA_ARGS__); if (!res) return false; "
               "} while(0)\n\n");
        output("%s",
               "bool\nrender_template("
               "struct Context* ctx, struct StringBuilder* sb)\n{\n");

        char const* cursor = src;
        while (*cursor) {
                char* tag_open = strstr(cursor, "{{");

                if (!tag_open) {
                        // Remainder of file
                        output("%*ssb_appendf(sb, \"",
                               indentation_level * indentation, "");
                        output_escaped(cursor, cursor + strlen(cursor), &sb);
                        output("%s", "\");\n");
                        break;
                }

                // 1. Literal text before tag
                struct String text_before_tag = {0};
                if (tag_open > cursor) {
                        text_before_tag.data = (char*)cursor;
                        text_before_tag.size = tag_open - cursor;
                }

                // 2. Parse Tag
                char* tag_contents = tag_open + 2;
                char* tag_close    = strstr(tag_contents, "}}");

                if (!tag_close) {
                        int starting_line = 1;
                        for (auto it = src; it < tag_open; ++it) {
                                starting_line += *it == '\n';
                        }
                        fprintf(stderr,
                                "ERROR: Template statement has unmatched "
                                "parentecies\n%s:%d:\n",
                                filename, starting_line);
                        auto tail_len = strlen(tag_open);
                        if (tail_len > 40) tail_len = 40;
                        tag_open[tail_len] = '\0';
                        fprintf(stderr, "%s", tag_open);
                        free(sb.ascii.data);
                        free(content.data);
                        return EXIT_FAILURE;
                }

                bool line_elimination =
                    (tag_close > tag_contents && *(tag_close - 1) == '-');

                // Extract inner content
                size_t content_len = tag_close - tag_contents;
                if (line_elimination) content_len--;

                char* raw_content = strndup(tag_contents, content_len);
                char* code        = trim(raw_content);

                cursor            = tag_close + 2;

                // 3. Handle Line Elimination
                if (line_elimination) {
                        // Skip trailing white-spaces and exactly one newline
                        while (isspace(*cursor) && *cursor != '\n') {
                                cursor++;
                        }
                        if (*cursor == '\n') ++cursor;

                        // Skip prepended white-spaces
                        while (text_before_tag.size) {
                                char last = text_before_tag
                                                .data[text_before_tag.size - 1];
                                if (isspace(last) && last != '\n') {
                                        text_before_tag.size--;
                                } else {
                                        break;
                                }
                        }
                }

                // Output text before tag
                if (text_before_tag.size) {
                        output("%*ssb_appendf(sb, \"",
                               indentation_level * indentation, "");
                        output_escaped(
                            text_before_tag.data,
                            text_before_tag.data + text_before_tag.size, &sb);
                        output("%s", "\");\n");
                }

                // Output tag body
                size_t const code_len = strlen(code);
                if (strncmp(code, "$.", 2) == 0) {
                        // String Mode shortcut
                        output("%*ssb_appendf(sb, \"%%s\", %s);\n",
                               indentation_level * indentation, "", code);
                } else if (code[0] == '\"') {
                        // Format Mode shortcut
                        output("%*ssb_appendf(sb, %s);\n",
                               indentation_level * indentation, "", code);
                } else if (code_len) {
                        // Raw Mode (C code)
                        // count brackets balance for indentation
                        int brackets = 0;
                        for (size_t i = 0; i < code_len; ++i) {
                                brackets += code[i] == '{';
                                brackets -= code[i] == '}';
                        }
                        // decrease indentation before tag's body
                        if (brackets < 0) indentation_level += brackets;
                        output("%*s%s\n", indentation_level * indentation, "",
                               code);
                        // increase indentation after tag's body
                        if (brackets > 0) indentation_level += brackets;
                }
                free(raw_content);
        }

        output("%s", "    return true;\n");
        output("%s", "}\n\n#undef sb_appendf\n#undef $\n#endif\n");

        printf("%s", sb.ascii.data);

        free(guard.data);
        free(sb.ascii.data);
        free(content.data);
        return EXIT_SUCCESS;

#undef output
#undef output_escaped
}

int
main(int argc, char* argv[])
{
        if (argc != 2 || strcmp(argv[1], "-h") == 0 ||
            strcmp(argv[1], "--help") == 0) {
                printf("argc: %d\n", argc);
                fprintf(stderr, "Usage: %s <file.th> > <file.h>\n", argv[0]);
                return EXIT_FAILURE;
        }

        return translate(argv[1]);
}
