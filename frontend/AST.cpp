#include "frontend/AST.h"

#include "frontend/DiagnosticPrinter.h"

#include "mlir/IR/Diagnostics.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdlib>

namespace libadalang = frontend::libadalang;

#define DEBUG_TYPE "ada-nameres"

static bool print_exception(bool or_silent) {
  const ada_exception *exc = ada_get_last_exception();
  if (exc != nullptr) {
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

static void print_indent(llvm::raw_ostream &os, int level) {
  for (int i = 0; i < level; ++i)
    os << "| ";
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

static void dump_image(llvm::raw_ostream &os, ada_node *node, int level) {
  if (ada_node_is_null(node)) {
    print_indent(os, level);
    os << "<null node>\n";
    return;
  }

  ada_text img;
  ada_node_image(node, &img);
  print_indent(os, level);
  fprint_text(os, img, false);
  os << "\n";
  ada_destroy_text(&img);

  unsigned count = ada_node_children_count(node);
  for (unsigned i = 0; i < count; ++i) {
    ada_node child;
    if (ada_node_child(node, i, &child) == 0)
      llvm::errs() << "Error while getting a child\n";
    dump_image(os, &child, level + 1);
  }
}

libadalang::AdaAST::AdaAST(llvm::StringRef inputFilename)
    : filename(inputFilename) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
      llvm::MemoryBuffer::getFileOrSTDIN(filename);
  if (std::error_code ec = fileOrErr.getError()) {
    valid = false;
    llvm::errs() << "Could not open input file: " << ec.message() << "\n";
  } else {
    auto buffer = fileOrErr.get()->getBuffer();

    context = ada_allocate_analysis_context();
    abort_on_exception();

    ada_initialize_analysis_context(context, nullptr, nullptr, nullptr, nullptr,
                                    1, 8);
    abort_on_exception();

    unit = ada_get_analysis_unit_from_buffer(context, filename.data(), nullptr,
                                             buffer.data(), buffer.size(),
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

void libadalang::dump(ada_node *node, llvm::raw_ostream &os) {
  dump_image(os, node, 0);
}

std::string libadalang::getName(ada_node *node, bool canonical) {
  switch (ada_node_kind(node)) {
  case ada_identifier:
  case ada_defining_name: {
    ada_text text;
    if (canonical) {
      ada_symbol_type symbol;
      ada_name_p_canonical_text(node, &symbol);
      ada_symbol_text(&symbol, &text);
    } else
      ada_node_text(node, &text);
    std::string name = textToString(text);
    // Operator names use Ada double-quote syntax (e.g. `"*"`, `"and"`). Strip
    // the surrounding quotes so MLIR displays them as @"*" / @and instead of
    // @"\22*\22" / @"\22and\22".
    ada_bool isOp = false;
    if (ada_name_p_is_operator_name(node, &isOp) && isOp)
      name = name.substr(1, name.size() - 2);
    return name;
  }
  default:
    llvm::errs() << "Can't get name of node: " << libadalang::image(node)
                 << "\n";
    return {};
  }
}

std::string libadalang::textToString(ada_text &text) {
  char *buf;
  size_t length;
  ada_text_to_utf8(&text, &buf, &length);
  ada_destroy_text(&text);
  std::string result(buf, length);
  free(buf);
  return result;
}

std::string libadalang::bigIntToString(ada_big_integer bigint) {
  ada_text text;
  ada_big_integer_text(bigint, &text);
  std::string s = textToString(text);
  ada_big_integer_decref(bigint);
  return s;
}

std::optional<llvm::APInt> libadalang::bigIntToAPInt(ada_big_integer bigint) {
  std::string s = bigIntToString(bigint);
  // Validate before handing to APInt, whose string constructor asserts on
  // malformed input instead of failing.
  llvm::StringRef str(s);
  llvm::StringRef digits = str;
  digits.consume_front("-");
  if (digits.empty() ||
      digits.find_first_not_of("0123456789") != llvm::StringRef::npos)
    return std::nullopt;
  // getBitsNeeded is the unsigned magnitude width for non-negative values;
  // one more bit keeps the sign clear under signed interpretation.
  unsigned bits = llvm::APInt::getBitsNeeded(str, /*radix=*/10) + 1;
  return llvm::APInt(bits, str, /*radix=*/10);
}

ada_node libadalang::parent(ada_node *node) {
  ada_node par = {};
  ada_ada_node_parent(node, &par);
  return par;
}

llvm::raw_ostream &libadalang::operator<<(llvm::raw_ostream &os,
                                          libadalang::NodePrinter np) {
  ada_text img;
  ada_node_image(np.node, &img);
  fprint_text(os, img, false);
  ada_destroy_text(&img);
  return os;
}

mlir::Diagnostic &libadalang::operator<<(mlir::Diagnostic &diag,
                                         libadalang::NodePrinter np) {
  std::string buf;
  llvm::raw_string_ostream os(buf);
  os << np;
  return diag << buf;
}

bool libadalang::AdaAST::emitParserDiagnostics() const {
  if (!unit)
    return false;
  unsigned count = ada_unit_diagnostic_count(unit);
  if (count == 0)
    return false;

  // Only the first diagnostic is reported: parse errors are often cascading,
  // and showing just the root cause is less noisy.
  char *rawFilename = ada_unit_filename(unit);
  ada_diagnostic diag;
  if (ada_unit_diagnostic(unit, 0, &diag))
    frontend::DiagnosticPrinter().emitDiag(rawFilename, diag);
  free(rawFilename);
  return true;
}

bool libadalang::isBaseTypeDecl(ada_node &node) {
  ada_node_kind_enum kind = ada_node_kind(&node);
  return kind >= ada_discrete_base_subtype_decl && kind <= ada_formal_type_decl;
}

bool libadalang::isEnumTypeDecl(ada_node &typeDecl) {
  ada_bool result = false;
  return ada_base_type_decl_p_is_enum_type(&typeDecl, &kNullOrigin, &result) &&
         result;
}

bool libadalang::isUniversalTypeDecl(ada_node &typeDecl) {
  // Identify the universal integer/real types by entity identity rather than by
  // name: p_universal_int_type / p_universal_real_type return the Standard
  // universal types (using typeDecl's context), compared via is_equivalent.
  for (auto getUniversal : {ada_ada_node_p_universal_int_type,
                            ada_ada_node_p_universal_real_type}) {
    ada_node universal;
    if (getUniversal(&typeDecl, &universal) && !ada_node_is_null(&universal) &&
        ada_node_is_equivalent(&typeDecl, &universal))
      return true;
  }
  return false;
}

bool libadalang::isNumericTypeDecl(ada_node &typeDecl) {
  ada_bool result = false;
  return ada_base_type_decl_p_is_numeric_type(&typeDecl, &kNullOrigin,
                                              &result) &&
         result;
}

bool libadalang::emitSolverDiagnostics(ada_node *node) {
  ada_bool resolved;
  if (!ada_ada_node_p_resolve_names(node, &resolved) || resolved)
    return false;

  // Resolution failed; emit diagnostics if available.
  ada_internal_solver_diagnostic_array diags = nullptr;
  if (ada_ada_node_p_nameres_diagnostics(node, &diags) && diags &&
      diags->n > 0) {
    // The solver tags each diagnostic with the round that produced it. Earlier
    // rounds are intermediate exploration the solver later refines, so report
    // only the last round's diagnostics to cut the noise.
    int lastRound = 0;
    for (int i = 0; i < diags->n; ++i)
      if (diags->items[i].round > lastRound)
        lastRound = diags->items[i].round;

    LLVM_DEBUG({
      for (int i = 0; i < diags->n; ++i) {
        ada_internal_logic_context_array ctxs = diags->items[i].contexts;
        llvm::dbgs() << "diag " << i << " round=" << diags->items[i].round
                     << " nctx=" << (ctxs ? ctxs->n : 0) << "\n";
        for (int j = 0; ctxs && j < ctxs->n; ++j)
          llvm::dbgs() << "  ref_node="
                       << libadalang::image(&ctxs->items[j].ref_node)
                       << " decl_node="
                       << libadalang::image(&ctxs->items[j].decl_node) << "\n";
      }
    });

    frontend::DiagnosticPrinter printer;
    for (int i = 0; i < diags->n; ++i)
      if (diags->items[i].round == lastRound)
        printer.emitDiag(diags->items[i]);
  } else {
    llvm::errs()
        << "error: name resolution failed but no diagnostics to report\n";
  }
  if (diags)
    ada_internal_solver_diagnostic_array_dec_ref(diags);
  return true;
}
