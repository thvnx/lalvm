// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#ifndef LALVM_SUPPORT_H
#define LALVM_SUPPORT_H

#include "mlir/IR/OwningOpRef.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/StringRef.h"
#include <string>

namespace llvm {
class Module;
class TargetMachine;
class raw_ostream;
} // namespace llvm

namespace mlir {
class MLIRContext;
class ModuleOp;
} // namespace mlir

namespace lalvm {

/// The compiler identity, with the LLVM and Libadalang versions, as printed by
/// `--version` and recorded in `llvm.ident` and DW_AT_producer.
std::string version();

/// Load an MLIR module from a file, or standard input when the filename is "-".
int loadMLIRFile(llvm::StringRef inputFilename, mlir::MLIRContext &context,
                 mlir::OwningOpRef<mlir::ModuleOp> &module);

/// Write textual output to a file, or standard output when the filename is "-".
int writeTextOutput(llvm::StringRef outputFilename,
                    llvm::function_ref<void(llvm::raw_ostream &)> print);

/// Emit an object file or target assembly to the given path.
int emitMachineCode(llvm::Module &module, llvm::TargetMachine &targetMachine,
                    llvm::StringRef path, bool emitObject);

} // namespace lalvm

#endif // LALVM_SUPPORT_H
