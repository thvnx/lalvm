--  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
--  Copyright (c) 2026 The LALVM Project

package System
  with Pure
is

   Min_Int               : constant := -2**127;
   Max_Int               : constant := 2**127 - 1;
   Max_Binary_Modulus    : constant := 2**128;
   Max_Nonbinary_Modulus : constant := 2**32 - 1;
   Max_Base_Digits       : constant := 18;
   Max_Digits            : constant := 18;

end System;
