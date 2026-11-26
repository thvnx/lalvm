// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project
//
// Temporary LALVM check failure handlers until the Ada runtime provides them.

#include <stdio.h>
#include <stdlib.h>

static _Noreturn void raiseConstraintError(const char *file, int line,
                                           const char *kind) {
  fprintf(stderr, "\nraised CONSTRAINT_ERROR : %s:%d %s\n", file, line, kind);
  exit(1);
}

_Noreturn void __lalvm_rcheck_CE_Overflow_Check(const char *file, int line) {
  raiseConstraintError(file, line, "overflow check failed");
}

_Noreturn void __lalvm_rcheck_CE_Range_Check(const char *file, int line) {
  raiseConstraintError(file, line, "range check failed");
}

_Noreturn void __lalvm_rcheck_CE_Index_Check(const char *file, int line) {
  raiseConstraintError(file, line, "index check failed");
}

_Noreturn void __lalvm_rcheck_CE_Divide_By_Zero(const char *file, int line) {
  raiseConstraintError(file, line, "divide by zero");
}

_Noreturn void __lalvm_rcheck_CE_Invalid_Data(const char *file, int line) {
  raiseConstraintError(file, line, "invalid data");
}
