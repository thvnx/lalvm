#ifndef ADA_MLIRGEN_H
#define ADA_MLIRGEN_H

#include "lal/AST.h"

#define MLIRGEN_DEBUG "mlirgen"

namespace mlir {
class MLIRContext;
template <typename OpTy> class OwningOpRef;
class ModuleOp;
} // namespace mlir

namespace ada {

/// Emit IR for the given Ada source, returns a newly created MLIR module
/// or nullptr on failure.
mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &moduleAST);

} // namespace ada

#endif // ADA_MLIRGEN_H
