// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project
//
// Temporary substitutes for the GNAT runtime entry points the check failure
// paths call, so that a lalvm object links without libgnat. They still print
// GNAT's message and exit with GNAT's status. To be used until LALVM's own Ada
// runtime provides the proper support.

#include <stdio.h>
#include <stdlib.h>

static _Noreturn void raiseConstraintError(const char *file, int line,
                                           const char *kind) {
  fprintf(stderr, "\nraised CONSTRAINT_ERROR : %s:%d %s\n", file, line, kind);
  exit(1);
}

_Noreturn void __gnat_rcheck_CE_Overflow_Check(const char *file, int line) {
  raiseConstraintError(file, line, "overflow check failed");
}

_Noreturn void __gnat_rcheck_CE_Range_Check(const char *file, int line) {
  raiseConstraintError(file, line, "range check failed");
}

_Noreturn void __gnat_rcheck_CE_Divide_By_Zero(const char *file, int line) {
  raiseConstraintError(file, line, "divide by zero");
}
