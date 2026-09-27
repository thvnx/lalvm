// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "lalvm/Support.h"
#include "frontend/AST.h"

#include "llvm/Config/llvm-config.h"

std::string lalvm::version() {
  return "lalvm (LLVM " LLVM_VERSION_STRING ", Libadalang " +
         frontend::libadalang::version() + ")";
}
