#ifndef LAL_AST_H
#define LAL_AST_H

#include <string>

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include "libadalang.h"

namespace mlir {
class Diagnostic;
} // namespace mlir

namespace libadalang {

/// Dump an ada_node tree.
void dump(ada_node *node);

/// Get the name (in utf8 format) of the given ada_node (return an empty string
/// if no name). If canonical is true (the default), the name is returned in its
/// canonical lowercase form; otherwise it is returned as written in the source.
std::string getName(ada_node *node, bool canonical = true);

ada_node parent(ada_node *node);

/// Convert an ada_text to a UTF-8 std::string, freeing the text afterwards.
std::string textToString(ada_text &text);

/// Wrapper that enables printing an ada_node via operator<<.
/// Usage: llvm::errs() << libadalang::print(&node);
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
  bool valid = true;

public:
  AdaAST(llvm::StringRef inputFilename);
  AdaAST(const AdaAST &);
  ~AdaAST();

  ada_analysis_unit &getAnalysisUnit() { return unit; }
  ada_node &getUnitRootNode() { return root; }

  /// Return whether everything went well during object construction.
  bool isValid() { return valid; }
  void dump() { libadalang::dump(&root); }
};

} // namespace libadalang

#endif // LAL_AST_H
