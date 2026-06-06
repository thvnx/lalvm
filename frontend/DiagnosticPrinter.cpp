#include "frontend/DiagnosticPrinter.h"
#include "frontend/AST.h"

#include "mlir/IR/BuiltinAttributes.h"

#include "llvm/Support/Path.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

namespace libadalang = frontend::libadalang;

static void printSeverity(mlir::DiagnosticSeverity sev) {
  switch (sev) {
  case mlir::DiagnosticSeverity::Error:
    llvm::WithColor::error();
    break;
  case mlir::DiagnosticSeverity::Warning:
    llvm::WithColor::warning();
    break;
  case mlir::DiagnosticSeverity::Note:
    llvm::WithColor::note();
    break;
  case mlir::DiagnosticSeverity::Remark:
    llvm::WithColor::remark();
    break;
  }
}

/// Print the "filename:line:col: " location prefix to stderr.
static void printPrefix(llvm::StringRef filename, unsigned line, unsigned col) {
  llvm::errs() << filename << ":" << line << ":" << col << ": ";
}

void frontend::DiagnosticPrinter::emitDiag(llvm::StringRef filename,
                                           ada_diagnostic &diag) const {
  printPrefix(llvm::sys::path::filename(filename), diag.sloc_range.start.line,
              diag.sloc_range.start.column);

  printSeverity(mlir::DiagnosticSeverity::Error);

  char *msgBuf;
  size_t msgLen;
  ada_text_to_utf8(&diag.message, &msgBuf, &msgLen);
  ada_destroy_text(&diag.message);
  llvm::errs() << llvm::StringRef(msgBuf, msgLen) << "\n";
  free(msgBuf);
}

// Render a solver-diagnostic argument. These are usually entities (type or
// object declarations), whose name lives on the defining name rather than the
// node itself; `getName` only handles Name/DefiningName nodes. Resolve a
// declaration to its defining name first, then fall back to `getName`.
static std::string solverArgImage(ada_node &node) {
  ada_node defName;
  if (ada_basic_decl_p_defining_name(&node, &defName) &&
      !ada_node_is_null(&defName))
    return libadalang::getName(&defName, /*canonical=*/false);
  return libadalang::getName(&node, /*canonical=*/false);
}

// Format a solver diagnostic message by substituting {} holes sequentially
// with the source-text image of each node argument.
static std::string
formatSolverDiag(const ada_internal_solver_diagnostic &diag) {
  // message_template is a UTF-32 ada_string_type; convert it to UTF-8 first.
  char *buf;
  size_t len;
  ada_string_to_utf8(diag.message_template, &buf, &len);
  std::string tmpl(buf, len);
  free(buf);

  // Remap known LAL solver messages to GNAT-style wording.
  // Template comes from @predicate_error annotations in nodes.lkt.
  if (tmpl == "cannot find name {}" && diag.args && diag.args->n >= 1)
    return '"' + solverArgImage(diag.args->items[0]) + "\" is undefined";

  // General case: substitute {} holes sequentially with the source-text image
  // of each argument (mirrors Python's str.format(*args) used by Langkit).
  // Any '{' not followed immediately by '}' is emitted verbatim.
  std::string msg;
  int nextArg = 0;
  for (size_t i = 0; i < tmpl.size();) {
    if (tmpl[i] == '{' && i + 1 < tmpl.size() && tmpl[i + 1] == '}' &&
        diag.args && nextArg < diag.args->n) {
      msg += solverArgImage(diag.args->items[nextArg++]);
      i += 2;
    } else {
      msg += tmpl[i++];
    }
  }
  return msg;
}

// Print "filename:line:col: error: <msg>" for a node's source location.
static void emitNodeError(ada_node *node, llvm::StringRef msg) {
  ada_source_location_range sloc = {{0, 0}, {0, 0}};
  ada_node_sloc_range(node, &sloc);
  ada_analysis_unit unit = ada_node_unit(node);
  char *rawFilename = ada_unit_filename(unit);
  printPrefix(llvm::sys::path::filename(rawFilename), sloc.start.line,
              sloc.start.column);
  free(rawFilename);
  printSeverity(mlir::DiagnosticSeverity::Error);
  llvm::errs() << msg << "\n";
}

void frontend::DiagnosticPrinter::emitDiag(
    const ada_internal_solver_diagnostic &diag) const {
  if (!diag.location)
    return;
  // diag.location is an ada_base_node (opaque pointer); wrap it into an
  // ada_node so we can call the standard node API on it.
  ada_node locNode;
  ada_create_bare_entity(diag.location, &locNode);

  // When a constraint's reference resolved to no declaration, lead with a
  // headline at the reference naming the unresolved candidate (e.g. the
  // operator), then report the type detail at its own location below.
  for (int i = 0; diag.contexts && i < diag.contexts->n; ++i) {
    ada_node refNode = diag.contexts->items[i].ref_node;
    ada_node declNode = diag.contexts->items[i].decl_node;
    if (!ada_node_is_null(&refNode) && ada_node_is_null(&declNode)) {
      ada_text text;
      ada_node_text(&refNode, &text);
      emitNodeError(&refNode, "no matching candidate for \"" +
                                  libadalang::textToString(text) + "\"");
      break;
    }
  }

  emitNodeError(&locNode, formatSolverDiag(diag));
}

void frontend::DiagnosticPrinter::emitDiag(mlir::Diagnostic &diag) const {
  if (auto flc = mlir::dyn_cast<mlir::FileLineColRange>(diag.getLocation()))
    printPrefix(llvm::sys::path::filename(flc.getFilename().getValue()),
                flc.getStartLine(), flc.getStartColumn());
  else
    llvm::errs() << diag.getLocation() << ": ";

  printSeverity(diag.getSeverity());
  diag.print(llvm::errs());
  llvm::errs() << '\n';
}
