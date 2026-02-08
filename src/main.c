#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string-builder/header.h"
#include "utils/header.h"

int indentation_level     = 1;
constexpr int indentation = 4;
int line_number           = 1;

bool
output_escaped(char const* start, char const* end, struct StringBuilder* sb)
{
#define OUTPUT(...)                                    \
        do {                                           \
                bool ok = sb_appendf(sb, __VA_ARGS__); \
                if (!ok) return false;                 \
        } while (0)

        for (auto p = start; p < end; ++p) {
                switch (*p) {
                case '\"':
                        OUTPUT("%s", "\\\"");
                        break;
                case '\\':
                        OUTPUT("%s", "\\\\");
                        break;
                case '\n': {
                        // combine newlines on single output line
                        for (; p < end && *p == '\n'; ++p) {
                                OUTPUT("%s", "\\n");
                        }
                        if (p >= end || *p == '\0') break;
                        --p;

                        // split multi-line string
                        constexpr int quotes_index = sizeof("sb_appendf(sb,");
                        OUTPUT("\"\n%*s\"",
                               indentation_level * indentation + quotes_index,
                               "");
                } break;
                case '\r':
                        OUTPUT("%s", "\\r");
                        break;
                default:
                        OUTPUT("%c", *p);
                }
        }

        return true;

#undef OUTPUT
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
count_char(struct String s, char target)
{
        int count = 0;
        for (size_t i = 0; i < s.size; i++) {
                if (s.data[i] == target) {
                        count++;
                }
        }
        return count;
}

int
translate(const char* const filename)
{
        bool ok;
        struct StringBuilder sb = {0};

        struct String content   = read_entire_file(filename);
        if (!content.data) {
                perror("Could not read input file/stream");
                return EXIT_FAILURE;
        }
        struct String guard = build_header_guard(filename);
        if (!guard.data) {
                perror("Could not build header file guard");
                return EXIT_FAILURE;
        }

#define OUTPUT(...)                                \
        do {                                       \
                ok = sb_appendf(&sb, __VA_ARGS__); \
                if (!ok) goto cleanup;             \
        } while (0)
#define OUTPUT_LINE OUTPUT("#line %d \"%s\"\n", line_number, filename)

#define OUTPUT_ESCAPED(...)                            \
        do {                                           \
                ok = output_escaped(__VA_ARGS__, &sb); \
                if (!ok) goto cleanup;                 \
        } while (0)

        // Header Guard
        OUTPUT("#ifndef %s_H\n#define %s_H\n\n", guard.data, guard.data);
        OUTPUT("%s",
               "bool render_template("
               "struct Context* ctx, struct StringBuilder* sb);\n\n");
        OUTPUT("%s", "#endif\n\n#ifdef IMPLEMENTATION\n\n");
        OUTPUT("%s", "#define $ (*ctx)\n");
        OUTPUT("%s",
               "#define sb_appendf(...) do { "
               "bool res = sb_appendf(__VA_ARGS__); if (!res) return false; "
               "} while(0)\n\n");
        OUTPUT("%s",
               "bool\nrender_template("
               "struct Context* ctx, struct StringBuilder* sb)\n{\n");

        char const* cursor = content.data;
        struct String text = {
            .const_data = content.data,
            .size       = 0,
        };
        while (*cursor) {
                char const* tag_open = strstr(cursor, "{{");

                if (!tag_open) {
                        // Remainder of file
                        OUTPUT_LINE;
                        OUTPUT("%*ssb_appendf(sb, \"",
                               indentation_level * indentation, "");
                        OUTPUT_ESCAPED(cursor, cursor + strlen(cursor));
                        OUTPUT("%s", "\");\n");
                        break;
                }

                // 1. Literal text before tag
                struct String text_before_tag = {0};
                if (tag_open > cursor) {
                        text_before_tag.data = (char*)cursor;
                        text_before_tag.size = tag_open - cursor;
                }

                // 2. Parse Tag
                char const* tag_contents = tag_open + 2;
                char const* tag_close    = strstr(tag_contents, "}}");

                if (!tag_close) {
                        int starting_line = 1;
                        for (auto it = content.data; it < tag_open; ++it) {
                                starting_line += *it == '\n';
                        }
                        fprintf(stderr,
                                "ERROR: Template statement has unmatched "
                                "parentecies\n%s:%d:\n",
                                filename, starting_line);
                        int tail_len = strlen(tag_open);
                        if (tail_len > 40) tail_len = 40;
                        fprintf(stderr, "%.*s\n", tail_len, tag_open);
                        ok = false;
                        goto cleanup;
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
                        OUTPUT_LINE;
                        text.size = tag_open - text.const_data;
                        line_number += count_char(text, '\n');
                        OUTPUT("%*ssb_appendf(sb, \"",
                               indentation_level * indentation, "");
                        OUTPUT_ESCAPED(
                            text_before_tag.data,
                            text_before_tag.data + text_before_tag.size);
                        OUTPUT("%s", "\");\n");
                }

                text.const_data       = tag_close;

                // Output tag body
                size_t const code_len = strlen(code);
                if (strncmp(code, "$.", 2) == 0) {
                        // String Mode shortcut
                        OUTPUT_LINE;
                        OUTPUT("%*ssb_appendf(sb, \"%%s\", %s);\n",
                               indentation_level * indentation, "", code);
                } else if (code[0] == '\"') {
                        // Format Mode shortcut
                        OUTPUT_LINE;
                        OUTPUT("%*ssb_appendf(sb, %s);\n",
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
                        OUTPUT_LINE;
                        OUTPUT("%*s%s\n", indentation_level * indentation, "",
                               code);
                        // increase indentation after tag's body
                        if (brackets > 0) indentation_level += brackets;
                }
                free(raw_content);
        }

        OUTPUT("%s", "    return true;\n");
        OUTPUT("%s", "}\n\n#undef sb_appendf\n#undef $\n#endif\n");

        printf("%s", sb.ascii.data);

cleanup:
        free(guard.data);
        free(sb.ascii.data);
        free(content.data);
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;

#undef OUTPUT
#undef OUTPUT_ESCAPED
}

int
main(int argc, char* argv[])
{
        if (argc != 2 || strcmp(argv[1], "-h") == 0 ||
            strcmp(argv[1], "--help") == 0) {
                fprintf(stderr, "Usage: %s <file.th> > <file.h>\n", argv[0]);
                return EXIT_FAILURE;
        }

        return translate(argv[1]);
}
