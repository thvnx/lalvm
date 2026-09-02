#ifndef FRONTEND_AST_H
#define FRONTEND_AST_H

#include <cstdint>
#include <optional>
#include <string>

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include "libadalang.h"

namespace mlir {
class Diagnostic;
} // namespace mlir

namespace frontend {
namespace libadalang {

/// Dump an ada_node tree to `os`.
void dump(ada_node *node, llvm::raw_ostream &os);

/// Get the name (in utf8 format) of the given ada_node (return an empty string
/// if no name). If canonical is true (the default), the name is returned in its
/// canonical lowercase form; otherwise it is returned as written in the source.
std::string getName(ada_node *node, bool canonical = true);

ada_node parent(ada_node *node);

/// Innermost subprogram body enclosing `node`, or null when there is none.
ada_node enclosingSubpBody(ada_node *node);

/// Convert an ada_text to a UTF-8 std::string, freeing the text afterwards.
std::string textToString(ada_text &text);

/// Convert `bigint` to its decimal string representation, consuming it.
std::string bigIntToString(ada_big_integer bigint);

/// Convert `bigint` to a signed arbitrary-precision APInt, consuming it.
/// The result is wide enough to hold the value under signed interpretation
/// (`getSignificantBits()` gives the exact need). Returns nullopt if its
/// decimal text does not parse.
std::optional<llvm::APInt> bigIntToAPInt(ada_big_integer bigint);

/// Whether `expr` is a static expression (@rm{4-9}).
bool isStaticExpr(ada_node &expr);

/// Evaluate `expr` as an integer, or nullopt when it cannot be evaluated
/// (`eval_as_int` fails, or the result does not parse as an APInt).
std::optional<llvm::APInt> evalExprAsInt(ada_node &expr);

/// Wrapper that enables printing an ada_node via operator<<.
/// Usage: llvm::errs() << libadalang::image(&node);
struct NodePrinter {
  ada_node *node;
};
inline NodePrinter image(ada_node *node) { return NodePrinter{node}; }
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, NodePrinter np);
mlir::Diagnostic &operator<<(mlir::Diagnostic &diag, NodePrinter np);

/// The AdaAST class handles an AST produced by Libadalang.
class AdaAST {
  llvm::StringRef filename;
  ada_analysis_context context = nullptr;
  ada_analysis_unit unit = nullptr;
  ada_node root = {};
  // Freed after the context, whose unit provider references it.
  ada_gpr_project project = nullptr;
  bool valid = true;

public:
  /// With a `projectFile`, resolve units via its GPR provider; otherwise parse
  /// the single file from its buffer.
  AdaAST(llvm::StringRef inputFilename, llvm::StringRef projectFile = {});
  AdaAST(const AdaAST &) = delete; // raw handles; a copy would double-free
  ~AdaAST();

  ada_analysis_unit &getAnalysisUnit() { return unit; }
  ada_node &getUnitRootNode() { return root; }

  /// Return whether everything went well during object construction.
  bool isValid() { return valid; }
  void dump(llvm::raw_ostream &os) { libadalang::dump(&root, os); }

  /// Print any Libadalang parse/lex diagnostics for this unit to stderr,
  /// matching the standard "file:line:col: error: msg" diagnostic format.
  /// Returns true if any diagnostics were emitted (i.e. the unit has errors).
  bool emitParserDiagnostics() const;
};

/// Call p_resolve_names on node, emit any solver diagnostics to stderr, and
/// return true if name resolution failed.
bool emitSolverDiagnostics(ada_node *node);

/// "No origin" value for Libadalang property calls that take an optional
/// `const ada_node *origin` parameter.
inline const ada_node kNullOrigin{};

constexpr llvm::StringLiteral kUniversalIntTypeName = "universal_int_type_";
constexpr llvm::StringLiteral kUniversalRealTypeName = "universal_real_type_";

bool isBaseTypeDecl(ada_node &node);
bool isEnumTypeDecl(ada_node &typeDecl);
bool isUniversalTypeDecl(ada_node &typeDecl);
bool isNumericTypeDecl(ada_node &typeDecl);
bool isArrayTypeDecl(ada_node &typeDecl);
unsigned arrayNdims(ada_node &arrayType);

/// Return the type definition of `typeDecl` if it exists and has kind `kind`,
/// nullopt otherwise.
std::optional<ada_node> typeDefOfKind(ada_node &typeDecl,
                                      ada_node_kind_enum kind);

} // namespace libadalang
} // namespace frontend

#endif // FRONTEND_AST_H
