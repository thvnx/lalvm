-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

-- REQUIRES: cc
-- RUN: %not %lalvm --emit=obj -P %S/subset.gpr %S/accounts.adb -o %t.accounts.o 2>&1 | %FileCheck %S/accounts.adb --check-prefix=MISSING

-- EXEC: %lalvm --emit=obj -P %S/subset.gpr %S/accounts.adb -o %t.accounts.o
-- EXEC: %lalvm --emit=obj -P %S/subset.gpr %s -o %t.o
-- EXEC: %lalvm --bind -P %S/subset.gpr %s -o %t.b.o
-- EXEC: %cc %t.o %t.accounts.o %t.b.o %gnatstub -o %t
-- EXEC: %t; test $? -eq 72

-- Two units: package Accounts keeps state set by its elaboration
-- (RM 7.2, RM 10.2).
-- Missing today: index subtypes, library-level objects, elaboration, calls
-- across units.

with Accounts;

function Ledger return Integer is
begin
   Accounts.Deposit (Account => 2, Amount => 50);
   Accounts.Transfer (To => 3, From => 2, Amount => 25);
   if Accounts.Balance (2) /= 35 then
      return 0;
   end if;
   return Accounts.Balance (3) * 2 + Accounts.Operations;
end Ledger;
