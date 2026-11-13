// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "lalvm/Support.h"
#include "frontend/AST.h"
#include "frontend/DiagnosticPrinter.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Support/FileUtilities.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

#include "llvm/Config/llvm-config.h"

#include <memory>
#include <utility>

std::string lalvm::version() {
  return "lalvm (LLVM " LLVM_VERSION_STRING ", Libadalang " +
         frontend::libadalang::version() + ")";
}

int lalvm::loadMLIRFile(llvm::StringRef inputFilename,
                        mlir::MLIRContext &context,
                        mlir::OwningOpRef<mlir::ModuleOp> &module) {
  std::string errorMessage;
  std::unique_ptr<llvm::MemoryBuffer> file =
      mlir::openInputFile(inputFilename, &errorMessage);
  if (!file) {
    frontend::emitErrorDiag(errorMessage);
    return 1;
  }
  llvm::SourceMgr sourceMgr;
  sourceMgr.AddNewSourceBuffer(std::move(file), llvm::SMLoc());
  // Route MLIR parse diagnostics through the source manager so errors print
  // with file:line:col and a source snippet instead of only the terse "can't
  // load file" fallback below.
  mlir::SourceMgrDiagnosticHandler sourceMgrHandler(sourceMgr, &context);
  module = mlir::parseSourceFile<mlir::ModuleOp>(sourceMgr, &context);
  if (!module) {
    frontend::emitErrorDiag(llvm::Twine("can't load file ") + inputFilename);
    return 1;
  }
  return 0;
}

int lalvm::writeTextOutput(
    llvm::StringRef outputFilename,
    llvm::function_ref<void(llvm::raw_ostream &)> print) {
  std::string errorMessage;
  std::unique_ptr<llvm::ToolOutputFile> out =
      mlir::openOutputFile(outputFilename, &errorMessage);
  if (!out) {
    frontend::emitErrorDiag(errorMessage);
    return 1;
  }
  print(out->os());
  out->keep();
  return 0;
}

int lalvm::emitMachineCode(llvm::Module &llvmModule, llvm::TargetMachine &tm,
                           llvm::StringRef path, bool emitObject) {
  // Writing a binary object to a terminal produces garbage; require -o (llc
  // does the same). Assembly is text and prints fine.
  if (emitObject && path == "-" &&
      llvm::sys::Process::StandardOutIsDisplayed()) {
    frontend::emitErrorDiag(
        "refusing to write a binary object file to the terminal; "
        "use -o <file>");
    return 1;
  }

  std::string errorMessage;
  std::unique_ptr<llvm::ToolOutputFile> out =
      mlir::openOutputFile(path, &errorMessage);
  if (!out) {
    frontend::emitErrorDiag(errorMessage);
    return 1;
  }

  // addPassesToEmitFile writes via pwrite; a non-seekable stream (e.g. a pipe)
  // must be buffered first.
  llvm::raw_pwrite_stream *os = &out->os();
  std::unique_ptr<llvm::buffer_ostream> buffered;
  if (!out->os().supportsSeeking()) {
    buffered = std::make_unique<llvm::buffer_ostream>(out->os());
    os = buffered.get();
  }

  llvm::legacy::PassManager pm;
  if (tm.addPassesToEmitFile(pm, *os, /*DwoOut=*/nullptr,
                             emitObject
                                 ? llvm::CodeGenFileType::ObjectFile
                                 : llvm::CodeGenFileType::AssemblyFile)) {
    frontend::emitErrorDiag(llvm::Twine("target cannot emit an ") +
                            (emitObject ? "object" : "assembly") + " file");
    return 1;
  }
  pm.run(llvmModule);
  out->keep();
  return 0;
}
