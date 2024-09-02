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

    if (ada_node_is_null(node)) {
        print_indent(level);
        printf("<null node>\n");
        return;
    }

    kind = ada_node_kind(node);
    ada_kind_name(kind, &kind_name);
    print_indent(level);
    putchar('<');
    fprint_text(stdout, kind_name, false);
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

llvm::StringRef getNameUtf8(ada_node *node) {
    switch (ada_node_kind(node)) {
        case ada_identifier:
        case ada_defining_name: {
            ada_symbol_type symbol;
            ada_text text;
            ada_name_p_canonical_text (node, &symbol);
            ada_symbol_text (&symbol, &text);
            char *subp_name;
            size_t length;
            ada_text_to_utf8(&text, &subp_name, &length);
            // TODO: why this is necessary? subp_name should end with a NUL byte
            subp_name[length] = '\0';
            return llvm::StringRef(subp_name, length);
        }
        default: {
            std::cerr << "Can't get StringRef of node: ";
            ada_text img;
            ada_node_image(node, &img);
            fprint_text(stderr, img, false);
            std::cerr << std::endl;
            return llvm::StringRef();
        }
    }
}
