-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- RUN: %not %lalvm --emit=mlir %s 2>&1 | %FileCheck %s

-- LALVM doesn't handle library-level objects (globals) yet.

-- CHECK: package_object.ads:11:4: error: library-level objects are not supported

package Package_Object is
   Count : Integer := 0;
end Package_Object;
