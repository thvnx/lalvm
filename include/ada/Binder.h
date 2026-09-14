// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#ifndef ADA_BINDER_H
#define ADA_BINDER_H

#include "frontend/AST.h"

namespace mlir {
class MLIRContext;
template <typename OpTy> class OwningOpRef;
class ModuleOp;
} // namespace mlir

namespace lalvm {

/// Bind `compilationUnit` to a main program. This function emits the bind
/// module as plain LLVM dialect, defining `main`, the entry point a C runtime
/// startup calls. `main` calls the main subprogram from `compilationUnit` and
/// returns an exit status: an Integer for a function returning one, or 0 for a
/// procedure.
///
/// The main unit must be the body of a library-level subprogram, parameterless,
/// and return Integer or one of its subtypes. @rm{10-2} only requires
/// parameterless library procedures, everything else is implementation defined.
///
/// @todo The binder should also handle the following aspects: elaboration,
/// runtime initialization and finalization, storage of command line arguments,
/// consistency of timestamps/verions, and so on.
mlir::OwningOpRef<mlir::ModuleOp> bind(mlir::MLIRContext &context,
                                       ada_node &compilationUnit);

} // namespace lalvm

#endif // ADA_BINDER_H
