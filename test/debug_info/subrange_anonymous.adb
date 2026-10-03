-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %lalvm -g --emit=llvm %s | %FileCheck %s

-- Anonymous subtype is described by an unnamed, artificial DW_TAG_subrange_type
-- of its base, scoped to the enclosing subprogram.

-- CHECK-DAG: !DILocalVariable(name: "x",{{.*}}type: ![[T:[0-9]+]])
-- CHECK-DAG: ![[T]] = !DISubrangeType(scope: ![[SP:[0-9]+]],{{.*}}flags: DIFlagArtificial,{{.*}}lowerBound: i32 1, upperBound: i32 10)
-- CHECK-DAG: ![[SP]] = distinct !DISubprogram(name: "subrange_anonymous"

procedure Subrange_Anonymous is
   X : Integer range 1 .. 10 := 5;
begin
   null;
end Subrange_Anonymous;
