-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

package Pkg is

   type Public_T is (Vrai, Faux);

   procedure Public_Proc;

private

   type Private_T is (Vert, Rouge);

end Pkg;
