// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2024-2026 The LALVM Project

#include "frontend/AST.h"

#include "frontend/DiagnosticPrinter.h"

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/Location.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/WithColor.h"
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
  }
  if (!or_silent)
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

/// Canonical fully qualified name of `decl`'s defining name (e.g.
/// `standard.integer`), or "?" when Libadalang cannot give it.
static std::string declFqn(ada_node decl) {
  ada_node defName = {};
  ada_string_type fqn;
  if (!ada_basic_decl_p_defining_name(&decl, &defName) ||
      ada_node_is_null(&defName) ||
      !ada_defining_name_p_canonical_fully_qualified_name(&defName, &fqn))
    return "?";
  char *buf;
  size_t len;
  ada_string_to_utf8(fqn, &buf, &len);
  std::string name(buf, len);
  free(buf);
  ada_string_dec_ref(fqn);
  return name;
}

/// `declFqn (decl)` followed by `@line:col` of the declaration's start, with
/// the file name before it when the declaration is in another unit than `from`
/// (e.g. `standard.integer@__standard:4:3`).
static std::string declRef(ada_node decl, ada_node *from) {
  std::string ref = declFqn(decl) + "@";
  ada_analysis_unit unit = ada_node_unit(&decl);
  if (unit != ada_node_unit(from)) {
    char *filename = ada_unit_filename(unit);
    ref += llvm::sys::path::filename(filename).str() + ":";
    free(filename);
  }
  ada_source_location_range r;
  ada_node_sloc_range(&decl, &r);
  return ref + std::to_string(r.start.line) + ":" +
         std::to_string(r.start.column);
}

static void dump_image(llvm::raw_ostream &os, ada_node *node, int level) {
  // Absent optional children and empty lists carry no information in a dump
  // without field names: skip them.
  if (ada_node_is_null(node))
    return;
#if LALVM_LIBADALANG_VERSION_MAJOR >= 27
  // The property only succeeds on list nodes.
  ada_bool isEmptyList = 0;
  if (ada_ada_list_is_empty_list(node, &isEmptyList) && isEmptyList)
    return;
#else
  // List kinds range from `ada_ada_node_list` to `ada_variant_list`.
  ada_node_kind_enum nodeKind = ada_node_kind(node);
  if (nodeKind >= ada_ada_node_list && nodeKind <= ada_variant_list &&
      ada_node_children_count(node) == 0)
    return;
#endif

  // Colors follow clang's -ast-dump. WithColor only emits escape codes when
  // `os` is a terminal (or under --color).
  using llvm::raw_ostream;
  using llvm::WithColor;
  print_indent(os, level);
  // Kind, then the source text for a token node, then the range.
  ada_text kind;
  ada_kind_name(ada_node_kind(node), &kind);
  fprint_text(WithColor(os, raw_ostream::GREEN, /*Bold=*/true).get(), kind,
              /*with_quotes=*/false);
  ada_destroy_text(&kind);
  if (ada_node_is_token_node(node)) {
    ada_text text;
    ada_node_text(node, &text);
    os << ' ';
    fprint_text(WithColor(os, raw_ostream::CYAN).get(), text,
                /*with_quotes=*/true);
    ada_destroy_text(&text);
  }
  ada_source_location_range r;
  ada_node_sloc_range(node, &r);
  os << ' ';
  WithColor(os, raw_ostream::YELLOW).get()
      << '[' << r.start.line << ':' << r.start.column << '-' << r.end.line
      << ':' << r.end.column << ']';

  // Name resolution information (print nothing on failure).
  ada_bool isDefining = 1;
  ada_node decl = {};
  if (ada_name_p_is_defining(node, &isDefining) && !isDefining &&
      ada_name_p_referenced_decl(node, /*imprecise_fallback=*/0, &decl) &&
      !ada_node_is_null(&decl))
    WithColor(os, raw_ostream::BLUE).get() << " ref=" << declRef(decl, node);
  ada_node type = {};
  if (ada_expr_p_expression_type(node, &type) && !ada_node_is_null(&type))
    WithColor(os, raw_ostream::BLUE).get() << " type=" << declRef(type, node);
  os << '\n';

  unsigned count = ada_node_children_count(node);
  for (unsigned i = 0; i < count; ++i) {
    ada_node child;
    if (ada_node_child(node, i, &child) == 0)
      llvm::errs() << "Error while getting a child\n";
    dump_image(os, &child, level + 1);
  }
}

libadalang::AdaAST::AdaAST(llvm::StringRef inputFilename,
                           llvm::StringRef projectFile)
    : filename(inputFilename) {
  if (!projectFile.empty()) {
    // Resolve units through the GPR project's provider (`with`ed units,
    // separate specs).
    // @todo Scenario variables (-X) are not wired yet.
    std::string projPath(projectFile);
    ada_string_array_ptr errors = nullptr;
#if LALVM_LIBADALANG_VERSION_MAJOR >= 27
    // Libadalang 27 loads a project from a GPR options object.
    ada_gpr_options opts = ada_gpr_options_create();
    ada_gpr_options_add_switch(opts, ADA_GPR_OPTION_P, projPath.c_str(),
                               nullptr,
                               /*override=*/0);
    ada_gpr_project_load(opts, /*ada_only=*/1, &project, &errors);
    // Read the exception now: the next libadalang call (options_free) clears
    // it.
    bool loadFailed = print_exception(/*or_silent=*/true);
    ada_gpr_options_free(opts);
#else
    ada_gpr_project_load(projPath.c_str(), /*scenario_vars=*/nullptr,
                         /*target=*/nullptr, /*runtime=*/nullptr,
                         /*config_file=*/nullptr, /*ada_only=*/1, &project,
                         &errors);
    bool loadFailed = print_exception(/*or_silent=*/true);
#endif
    if (errors) {
      for (int i = 0; i < errors->length; ++i)
        llvm::errs() << "project error: " << errors->c_ptr[i] << "\n";
      ada_free_string_array(errors);
    }
    if (loadFailed || !project) {
      valid = false;
      return;
    }

    context = ada_allocate_analysis_context();
    abort_on_exception();
#if LALVM_LIBADALANG_VERSION_MAJOR >= 27
    // Libadalang 27 added the `charset` argument.
    ada_gpr_project_initialize_context(project, context, /*project=*/nullptr,
                                       /*charset=*/nullptr,
                                       /*event_handler=*/nullptr,
                                       /*with_trivia=*/1, /*tab_stop=*/8);
#else
    ada_gpr_project_initialize_context(project, context, /*project=*/nullptr,
                                       /*event_handler=*/nullptr,
                                       /*with_trivia=*/1, /*tab_stop=*/8);
#endif
    abort_on_exception();

    std::string file(filename);
    unit = ada_get_analysis_unit_from_file(context, file.c_str(),
                                           /*charset=*/nullptr, /*reparse=*/0,
                                           ada_default_grammar_rule);
    abort_on_exception();
    ada_unit_root(unit, &root);
    return;
  }

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
      llvm::MemoryBuffer::getFileOrSTDIN(filename);
  if (std::error_code ec = fileOrErr.getError()) {
    valid = false;
    llvm::errs() << "Could not open input file: " << ec.message() << "\n";
    return;
  }
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

libadalang::AdaAST::~AdaAST() {
  // Decref the context first: its unit provider references the project.
  if (context)
    ada_context_decref(context);
  if (project)
    ada_gpr_project_free(project);
  // abort_on_exception() intentionally omitted: destructors must not throw or
  // call exit(), and a failure here is unrecoverable anyway.
}

void libadalang::dump(ada_node *node, llvm::raw_ostream &os) {
  if (!ada_node_is_null(node)) {
    char *filename = ada_unit_filename(ada_node_unit(node));
    llvm::WithColor(os, llvm::raw_ostream::SAVEDCOLOR, /*Bold=*/true).get()
        << "file " << llvm::sys::path::filename(filename);
    os << '\n';
    free(filename);
  }
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

bool libadalang::isStaticExpr(ada_node &expr) {
  ada_bool isStatic = false;
  return ada_expr_p_is_static_expr(&expr, /*imprecise_fallback=*/false,
                                   &isStatic) &&
         isStatic;
}

std::optional<llvm::APInt> libadalang::evalExprAsInt(ada_node &expr) {
  ada_big_integer bigint;
  if (!ada_expr_p_eval_as_int(&expr, &bigint))
    return std::nullopt;
  return bigIntToAPInt(bigint);
}

ada_node libadalang::parent(ada_node *node) {
  ada_node par = {};
  ada_ada_node_parent(node, &par);
  return par;
}

ada_node libadalang::enclosingSubpBody(ada_node *node) {
  ada_node cur = parent(node);
  while (!ada_node_is_null(&cur) && ada_node_kind(&cur) != ada_subp_body)
    cur = parent(&cur);
  return cur;
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
  return libadalang::emitParserDiagnostics(root);
}

bool libadalang::emitParserDiagnostics(const ada_node &node) {
  ada_analysis_unit unit = ada_node_unit(const_cast<ada_node *>(&node));
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
#if LALVM_LIBADALANG_VERSION_MAJOR >= 27
  return ada_base_type_decl_p_is_numeric_type(&typeDecl, &kNullOrigin,
                                              &result) &&
         result;
#else
  // `p_is_numeric_type` appeared in Libadalang 27 as "integer or real type".
  if (ada_base_type_decl_p_is_int_type(&typeDecl, &kNullOrigin, &result) &&
      result)
    return true;
  return ada_base_type_decl_p_is_real_type(&typeDecl, &kNullOrigin, &result) &&
         result;
#endif
}

bool libadalang::isArrayTypeDecl(ada_node &typeDecl) {
  ada_bool result = false;
  return ada_base_type_decl_p_is_array_type(&typeDecl, &kNullOrigin, &result) &&
         result;
}

unsigned libadalang::arrayNdims(ada_node &arrayType) {
  unsigned dim = 0;

  for (;; ++dim) {
    ada_node indexType = {};
    ada_base_type_decl_p_index_type(&arrayType, static_cast<int>(dim),
                                    &kNullOrigin, &indexType);

    if (ada_node_is_null(&indexType))
      break;
  }

  return dim;
}

std::optional<ada_node> libadalang::typeDefOfKind(ada_node &typeDecl,
                                                  ada_node_kind_enum kind) {
  ada_node typeDef = {};
  if (ada_type_decl_f_type_def(&typeDecl, &typeDef) &&
      !ada_node_is_null(&typeDef) && ada_node_kind(&typeDef) == kind)
    return typeDef;
  return std::nullopt;
}

std::optional<ada_node> libadalang::canonicalType(ada_node &typeDecl) {
  ada_node canon = {};
  if (ada_base_type_decl_p_canonical_type(&typeDecl, &kNullOrigin, &canon) &&
      !ada_node_is_null(&canon))
    return canon;
  return std::nullopt;
}

std::optional<ada_node> libadalang::referencedDecl(ada_node &name) {
  ada_node refDecl = {};
  if (ada_name_p_referenced_decl(&name, /*imprecise_fallback=*/0, &refDecl) &&
      !ada_node_is_null(&refDecl))
    return refDecl;
  return std::nullopt;
}

std::optional<ada_node> libadalang::designatedTypeDecl(ada_node &typeExpr) {
  ada_node typeDecl = {};
  if (ada_type_expr_p_designated_type_decl(&typeExpr, &typeDecl) &&
      !ada_node_is_null(&typeDecl))
    return typeDecl;
  return std::nullopt;
}

std::optional<llvm::StringRef> libadalang::specKind(ada_node &root) {
  if (ada_node_is_null(&root) || ada_node_kind(&root) != ada_compilation_unit)
    return std::nullopt;
  ada_analysis_unit_kind kind;
  if (!ada_compilation_unit_p_unit_kind(&root, &kind) ||
      kind != ADA_ANALYSIS_UNIT_KIND_UNIT_SPECIFICATION)
    return std::nullopt;
  ada_node body = {}, item = {};
  if (ada_compilation_unit_f_body(&root, &body) &&
      ada_node_kind(&body) == ada_library_item &&
      ada_library_item_f_item(&body, &item))
    switch (ada_node_kind(&item)) {
    case ada_package_decl:
    case ada_generic_package_decl:
      return "package spec";
    case ada_subp_decl:
    case ada_generic_subp_decl:
      return "subprogram spec";
    default:
      break;
    }
  return "spec";
}

bool libadalang::isFunction(ada_node &subpSpec) {
  ada_node returns = {};
  return ada_subp_spec_f_subp_returns(&subpSpec, &returns) &&
         !ada_node_is_null(&returns);
}

mlir::Location libadalang::sourceLocation(mlir::MLIRContext &context,
                                          const ada_node &node) {
  // const_cast: the Libadalang C API has no const-qualified overloads: the
  // underlying objects are never actually const.
  ada_node *n = const_cast<ada_node *>(&node);
  ada_source_location_range range;
  ada_node_sloc_range(n, &range);
  char *filename = ada_unit_filename(ada_node_unit(n));
  // StringAttr::get copies the string into the context, so filename can be
  // freed immediately.
  auto result = mlir::FileLineColRange::get(
      mlir::StringAttr::get(&context, filename), range.start.line,
      range.start.column, range.end.line, range.end.column);
  free(filename);
  return result;
}

std::optional<ada_node> libadalang::declPart(ada_node &body) {
  ada_node decl = {};
  if (ada_body_node_p_decl_part(&body, /*imprecise_fallback=*/0, &decl) &&
      !ada_node_is_null(&decl))
    return decl;
  return std::nullopt;
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
