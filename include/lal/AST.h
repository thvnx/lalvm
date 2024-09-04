#ifndef LAL_AST_H
#define LAL_AST_H

#include <cstring>
#include <iostream>

#include "llvm/ADT/StringRef.h"

#include "libadalang.h"

// TODO: to include in AdaAST
void dump_image(ada_node *node, int level);

namespace libadalang {

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
  void dump() { dump_image(&root, 0); }
};

} // namespace libadalang

// bool print_exception(bool or_silent);

void abort_on_exception(void);

llvm::StringRef getNameUtf8(ada_node *node);

#endif // LAL_AST_H
