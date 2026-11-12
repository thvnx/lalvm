-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- A library package with state, for ledger.adb.

package Accounts is
   subtype Id is Positive range 1 .. 4;

   procedure Deposit (Account : Id; Amount : Natural);
   procedure Transfer (From, To : Id; Amount : Natural);
   function Balance (Account : Id) return Integer;
   function Operations return Natural;
end Accounts;
