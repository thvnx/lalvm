-- SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-- Copyright (c) 2026 The LALVM Project

package body Accounts is
   -- MISSING: accounts.adb:[[#@LINE+1]]:4: error: array index constraint must be a .. binary op
   type Balances is array (Id) of Integer;
   Book  : Balances;
   Count : Natural := 0;

   procedure Deposit (Account : Id; Amount : Natural) is
   begin
      Book (Account) := Book (Account) + Amount;
      Count := Count + 1;
   end Deposit;

   procedure Transfer (From, To : Id; Amount : Natural) is
   begin
      Book (From) := Book (From) - Amount;
      Book (To) := Book (To) + Amount;
      Count := Count + 1;
   end Transfer;

   function Balance (Account : Id) return Integer is
   begin
      return Book (Account);
   end Balance;

   function Operations return Natural is
   begin
      return Count;
   end Operations;
begin
   --  Every account opens with 10 when the package is elaborated.
   for A in Id loop
      Book (A) := 10;
   end loop;
end Accounts;
