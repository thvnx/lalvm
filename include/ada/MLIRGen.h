#ifndef ADA_MLIRGEN_H
#define ADA_MLIRGEN_H

#include "lal/AST.h"
#include "mlir/IR/Value.h"
//#include <memory>

#define MLIRGEN_DEBUG "mlirgen"

namespace mlir {
class MLIRContext;
template <typename OpTy>
class OwningOpRef;
class ModuleOp;
} // namespace mlir

namespace ada {
class ModuleAST;

mlir::Value visit_expr(ada_node &expr);

/// Emit IR for the given Toy moduleAST, returns a newly created MLIR module
/// or nullptr on failure.
mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &moduleAST);

} // namespace toy

#endif // ADA_MLIRGEN_H
