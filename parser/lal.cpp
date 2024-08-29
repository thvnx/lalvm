#include "lal.h"

bool
print_exception (bool or_silent)
{
    const ada_exception *exc = ada_get_last_exception ();
    if (exc != NULL)
    {
        char *exc_name = ada_exception_name (exc->kind);
        printf ("Got an exception (%s):\n  %s\n",
                exc_name,
                exc->information);
        free (exc_name);
        return true;
    }
    else if (! or_silent)
        puts ("Got no exception\n");
    return false;
}

void
abort_on_exception (void)
{
    if (print_exception (true))
        exit (1);
}

static void
print_indent(int level)
{
    int i;
    for (i = 0; i < level; ++i)
        printf("| ");
}

static void
fprint_text(FILE *stream, ada_text text, bool with_quotes) {
    unsigned i;

    if (with_quotes)
        fputc('"', stream);

    for (i = 0; i < text.length; i++) {
        uint32_t c = text.chars[i];

        if ((with_quotes && c == '"')
            || c == '\\')
            fprintf(stream, "\\%c", (char) c);
        else if (0x20 <= c && c <= 0x7f) {
            fputc(c, stream);
        } else if (c <= 0xff) {
            fprintf(stream, "\\x");
            fprintf(stream, "%02x", c);
        } else if (c <= 0xffff) {
            fprintf(stream, "\\u");
            fprintf(stream, "%02x", c >> 8);
            fprintf(stream, "%02x", c & 0xff);
        } else {
            fprintf(stream, "\\U");
            fprintf(stream, "%02x", c >> 24);
            fprintf(stream, "%02x", (c >> 16) & 0xff);
            fprintf(stream, "%02x", (c >> 8) & 0xff);
            fprintf(stream, "%02x", c & 0xff);
        }
    }

    if (with_quotes)
        fputc('"', stream);
}

void
dump_image(ada_node *node, int level)
{
    ada_text img;
    unsigned i, count;

    if (ada_node_is_null(node)) {
        print_indent(level);
        printf("<null node>\n");
        return;
    }

    ada_node_image(node, &img);
    print_indent(level);
    fprint_text(stdout, img, false);
    printf("\n");
    ada_destroy_text(&img);

    count = ada_node_children_count(node);
    for (i = 0; i < count; ++i)
    {
        ada_node child;

        if (ada_node_child(node, i, &child) == 0)
            fprintf(stderr, "Error while getting a child");
        dump_image(&child, level + 1);
    }
}

void
dump(ada_node *node, int level)
{
    ada_node_kind_enum kind;
    ada_text kind_name;
    unsigned i, count;

    ada_source_location_range loc_range;

    if (ada_node_is_null(node)) {
        print_indent(level);
        printf("<null node>\n");
        return;
    }

    ada_node_sloc_range(node, &loc_range);

    kind = ada_node_kind(node);
    ada_kind_name(kind, &kind_name);
    print_indent(level);
    putchar('<');
    fprint_text(stdout, kind_name, false);
    // fprintf(stdout, "%" PRIu32 ":%" PRIu16 "" , //":%"PRIu16"-%"PRIu32":%"PRIu16" ",
    //         loc_range.start.line,
    //         loc_range.start.column);
    //         // loc_range.end.line,
    //         // loc_range.end.column);
    puts(">");

    count = ada_node_children_count(node);
    for (i = 0; i < count; ++i)
    {
        ada_node child;

        if (ada_node_child(node, i, &child) == 0)
            std::cerr << "Error while getting a child";
        dump(&child, level + 1);
    }
}

AdaNode*
convert(ada_node node) {
    return new AdaNode(node);
}
