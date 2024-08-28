#ifndef MLIRGEN_H_
#define MLIRGEN_H_

#include "lal.h"
//#include <memory>

namespace mlir {
class MLIRContext;
template <typename OpTy>
class OwningOpRef;
class ModuleOp;
} // namespace mlir

namespace ada {
class ModuleAST;

/// Emit IR for the given Toy moduleAST, returns a newly created MLIR module
/// or nullptr on failure.
mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &moduleAST);

int fn (int a);
} // namespace toy

#endif // MLIRGEN_H_
