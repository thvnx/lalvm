#include "lal/AST.h"

#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

static bool print_exception(bool or_silent) {
  const ada_exception *exc = ada_get_last_exception();
  if (exc != NULL) {
    char *exc_name = ada_exception_name(exc->kind);
    llvm::errs() << "Got an exception (" << exc_name << "):\n  "
                 << exc->information << "\n";
    free(exc_name);
    return true;
  } else if (!or_silent)
    llvm::errs() << "Got no exception\n";
  return false;
}

static void abort_on_exception(void) {
  if (print_exception(true))
    exit(1);
}

static void print_indent(int level) {
  for (int i = 0; i < level; ++i)
    llvm::outs() << "| ";
}

static void fprint_text(llvm::raw_ostream &stream, ada_text text,
                        bool with_quotes) {
  if (with_quotes)
    stream << '"';

  for (unsigned i = 0; i < text.length; i++) {
    uint32_t c = text.chars[i];

    if ((with_quotes && c == '"') || c == '\\')
      stream << '\\' << (char)c;
    else if (0x20 <= c && c <= 0x7f)
      stream << (char)c;
    else if (c <= 0xff)
      stream << llvm::format("\\x%02x", c);
    else if (c <= 0xffff)
      stream << llvm::format("\\u%04x", c);
    else
      stream << llvm::format("\\U%08x", c);
  }

  if (with_quotes)
    stream << '"';
}

static void dump_image(ada_node *node, int level) {
  if (ada_node_is_null(node)) {
    print_indent(level);
    llvm::outs() << "<null node>\n";
    return;
  }

  ada_text img;
  ada_node_image(node, &img);
  print_indent(level);
  fprint_text(llvm::outs(), img, false);
  llvm::outs() << "\n";
  ada_destroy_text(&img);

  unsigned count = ada_node_children_count(node);
  for (unsigned i = 0; i < count; ++i) {
    ada_node child;
    if (ada_node_child(node, i, &child) == 0)
      llvm::errs() << "Error while getting a child";
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

libadalang::AdaAST::AdaAST(const AdaAST &ast)
    : filename(ast.filename), context(ast.context), unit(ast.unit),
      root(ast.root), valid(ast.valid) {
  if (context)
    ada_context_incref(context);
}

libadalang::AdaAST::~AdaAST() {
  if (context)
    ada_context_decref(context);
  // abort_on_exception() intentionally omitted: destructors must not throw or
  // call exit(), and a failure in decref is unrecoverable anyway.
}

void libadalang::dump(ada_node *node) { dump_image(node, 0); }

std::string libadalang::getName(ada_node *node, bool canonical) {
  switch (ada_node_kind(node)) {
  case ada_identifier:
  case ada_defining_name: {
    ada_symbol_type symbol;
    ada_text text;
    if (canonical) {
      ada_name_p_canonical_text(node, &symbol);
      ada_symbol_text(&symbol, &text);
    } else
      ada_node_text(node, &text);
    char *buf;
    size_t length;
    ada_text_to_utf8(&text, &buf, &length);
    ada_destroy_text(&text);
    std::string result(buf, length);
    free(buf);
    return result;
  }
  default: {
    ada_text img;
    ada_node_image(node, &img);
    llvm::errs() << "Can't get name of node: ";
    fprint_text(llvm::errs(), img, false);
    llvm::errs() << "\n";
    ada_destroy_text(&img);
    return {};
  }
  }
}

ada_node libadalang::parent(ada_node *node) {
  ada_node par = {};
  ada_ada_node_parent(node, &par);
  return par;
}
