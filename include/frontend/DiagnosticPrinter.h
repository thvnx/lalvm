#ifndef FRONTEND_DIAGNOSTIC_PRINTER_H
#define FRONTEND_DIAGNOSTIC_PRINTER_H

#include "llvm/ADT/StringRef.h"

#include "mlir/IR/Diagnostics.h"

#include "libadalang.h"

namespace frontend {

/// Formats and prints diagnostics to stderr in the standard
/// "basename:line:col: severity: message" format.
class DiagnosticPrinter {
public:
  DiagnosticPrinter() = default;

  /// Print a Libadalang parse/lex diagnostic.
  /// @param filename Source file path; the basename is extracted internally.
  /// @param diag     Diagnostic to print. Consumes diag.message (frees its
  /// buffer).
  void emitDiag(llvm::StringRef filename, ada_diagnostic &diag) const;

  /// Print a Libadalang name-resolution solver diagnostic.
  /// Extracts location and message from the diagnostic itself.
  /// No-op if the diagnostic has no location.
  /// @param diag Solver diagnostic to print.
  void emitDiag(const ada_internal_solver_diagnostic &diag) const;

  /// Print an MLIR diagnostic, extracting location, severity, and message
  /// from the diagnostic object itself.
  /// @param diag Diagnostic to print.
  void emitDiag(mlir::Diagnostic &diag) const;
};

} // namespace frontend

#endif // FRONTEND_DIAGNOSTIC_PRINTER_H
