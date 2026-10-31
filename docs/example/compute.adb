function Compute (X : Integer) return Integer is
   function Double (N : Integer) return Integer is
   begin
      return N + N;
   end Double;
   Result : Integer := Double (X);
begin
   return Result;
end Compute;
