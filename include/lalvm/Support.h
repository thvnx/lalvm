// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#ifndef LALVM_SUPPORT_H
#define LALVM_SUPPORT_H

#include <string>

namespace lalvm {

/// The compiler identity, with the LLVM and Libadalang versions, as printed by
/// `--version` and recorded in `llvm.ident` and DW_AT_producer.
std::string version();

} // namespace lalvm

#endif // LALVM_SUPPORT_H
