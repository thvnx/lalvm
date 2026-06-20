//===- DITypeUtils.h - Shared post-translation DI type helpers --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Small helpers shared by the post-translation DI type builders
// (buildEnumDITypes, buildSubrangeDITypes), which both walk the module's
// DbgVariableRecords and rebuild placeholder types via LLVM's DIBuilder.
//
//===----------------------------------------------------------------------===//

#ifndef ADA_DITYPEUTILS_H
#define ADA_DITYPEUTILS_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugProgramInstruction.h"
#include "llvm/Support/Path.h"

namespace mlir::ada {

/// Return the DIFile for `path`, creating it once and caching by path so
/// repeated lookups reuse the same file node.
inline llvm::DIFile *
getOrCreateDIFile(llvm::DIBuilder &db,
                  llvm::DenseMap<llvm::StringRef, llvm::DIFile *> &cache,
                  llvm::StringRef path) {
  auto *&file = cache[path];
  if (!file)
    file = db.createFile(llvm::sys::path::filename(path),
                         llvm::sys::path::parent_path(path));
  return file;
}

/// Rebuild the local variable `dvr` refers to so its type becomes `newType`,
/// preserving whether it is a parameter or an auto variable (DILocalVariable
/// has no copy-with, so the variable must be recreated).
inline void retypeLocalVariable(llvm::DIBuilder &db,
                                llvm::DbgVariableRecord &dvr,
                                llvm::DIType *newType) {
  auto *var = dvr.getVariable();
  llvm::DILocalVariable *newVar;
  if (var->getArg() > 0)
    newVar = db.createParameterVariable(var->getScope(), var->getName(),
                                        var->getArg(), var->getFile(),
                                        var->getLine(), newType);
  else
    newVar = db.createAutoVariable(var->getScope(), var->getName(),
                                   var->getFile(), var->getLine(), newType);
  dvr.setVariable(newVar);
}

} // namespace mlir::ada

#endif // ADA_DITYPEUTILS_H
