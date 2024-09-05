#include "lal/AST.h"

#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

// TODO concert fprintf to stderr to llvm::errs()

static bool print_exception(bool or_silent) {
  const ada_exception *exc = ada_get_last_exception();
  if (exc != NULL) {
    char *exc_name = ada_exception_name(exc->kind);
    printf("Got an exception (%s):\n  %s\n", exc_name, exc->information);
    free(exc_name);
    return true;
  } else if (!or_silent)
    puts("Got no exception\n");
  return false;
}

static void abort_on_exception(void) {
  if (print_exception(true))
    exit(1);
}

static void print_indent(int level) {
  int i;
  for (i = 0; i < level; ++i)
    printf("| ");
}

static void fprint_text(FILE *stream, ada_text text, bool with_quotes) {
  unsigned i;

  if (with_quotes)
    fputc('"', stream);

  for (i = 0; i < text.length; i++) {
    uint32_t c = text.chars[i];

    if ((with_quotes && c == '"') || c == '\\')
      fprintf(stream, "\\%c", (char)c);
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

static void dump_image(ada_node *node, int level) {
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
  for (i = 0; i < count; ++i) {
    ada_node child;

    if (ada_node_child(node, i, &child) == 0)
      fprintf(stderr, "Error while getting a child");
    dump_image(&child, level + 1);
  }
}

libadalang::AdaAST::AdaAST(llvm::StringRef inputFilename) {
  filename = inputFilename;

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
      llvm::MemoryBuffer::getFileOrSTDIN(filename);
  if (std::error_code ec = fileOrErr.getError()) {
    valid = false;
    llvm::errs() << "Could not open input file: " << ec.message() << "\n";
  } else {
    auto buffer = fileOrErr.get()->getBuffer();

    context = ada_allocate_analysis_context();
    abort_on_exception();

    ada_initialize_analysis_context(context, NULL, NULL, NULL, NULL, 1, 8);
    abort_on_exception();

    unit = ada_get_analysis_unit_from_buffer(
        context, filename.data(), NULL, buffer.data(), strlen(buffer.data()),
        ada_default_grammar_rule);
    abort_on_exception();

    ada_unit_root(unit, &root);
  }
}

libadalang::AdaAST::AdaAST(const AdaAST &ast) {
  filename = ast.filename;
  context = ast.context;
  ada_context_incref(context);
  unit = ast.unit;
  root = ast.root;
  valid = ast.valid;
}

libadalang::AdaAST::~AdaAST() {
  ada_context_decref(context);
  abort_on_exception();
}

void libadalang::dump(ada_node *node) { dump_image(node, 0); }

llvm::StringRef libadalang::getName(ada_node *node) {
  switch (ada_node_kind(node)) {
  case ada_identifier:
  case ada_defining_name: {
    ada_symbol_type symbol;
    ada_text text;
    ada_name_p_canonical_text(node, &symbol);
    ada_symbol_text(&symbol, &text);
    char *subp_name;
    size_t length;
    ada_text_to_utf8(&text, &subp_name, &length);
    // TODO why this is necessary? subp_name should end with a NUL byte
    subp_name[length] = '\0';
    return llvm::StringRef(subp_name, length);
  }
  default: {
    // TODO use llvm::errs to print the error
    std::cerr << "Can't get StringRef of node: ";
    ada_text img;
    ada_node_image(node, &img);
    fprint_text(stderr, img, false);
    std::cerr << std::endl;
    return llvm::StringRef();
  }
  }
}
