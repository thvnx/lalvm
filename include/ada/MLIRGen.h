#ifndef ADA_MLIRGEN_H
#define ADA_MLIRGEN_H

#include "frontend/AST.h"

namespace mlir {
class MLIRContext;
template <typename OpTy> class OwningOpRef;
class ModuleOp;
} // namespace mlir

namespace lalvm {

/// Emit IR for the given Ada source, returns a newly created MLIR module
/// on success, or an empty OwningOpRef on failure.
mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &compilationUnit);

} // namespace lalvm

#endif // ADA_MLIRGEN_H
