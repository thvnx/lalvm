#ifndef LAL_AST_H
#define LAL_AST_H

#include <cstring>
#include <iostream>

#include "llvm/ADT/StringRef.h"

#include "libadalang.h"

namespace libadalang {

/// Dump an ada_node tree.
void dump(ada_node *node);

/// Get the name (in utf8 format) of the given ada_node (return an empty string
/// if no name).
llvm::StringRef getName(ada_node *node);

/// The AdaAST class handles an AST produced by Libadalang.
class AdaAST {
  llvm::StringRef filename;
  ada_analysis_context context;
  ada_analysis_unit unit;
  ada_node root;
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
