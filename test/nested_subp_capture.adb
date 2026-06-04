-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Exercises closure conversion on two harder cases at once:
--   * a *mutable* capture: `Add` reads and writes the enclosing `Total`, so
--     it is captured by reference (memref/ptr) and the write propagates back;
--     `Total` must stay an alloca (closure conversion runs before mem2reg, and
--     its address then escapes via the call), so the writes are not lost;
--   * a *transitive* capture: `Add_Twice` never names `Total`, but it calls
--     `Add`, which captures it; once `Add` is lifted, the forwarded argument
--     makes `Add_Twice` capture `Total` too and thread it through.
-- After lifting, all three subprograms are flat at module level with `Total`
-- passed by pointer, and the function returns 10.

-- LLVM-LABEL: define i32 @_ada_counter(
-- LLVM:         %[[T:.*]] = alloca i32
-- LLVM:         store i32 0, ptr %[[T]]
-- LLVM:         call void @counter__add_twice(i32 5, ptr %[[T]])
-- LLVM:         %[[R:.*]] = load i32, ptr %[[T]]
-- LLVM:         ret i32 %[[R]]

-- LLVM-DAG: define internal void @counter__add(i32 {{.*}}, ptr
-- LLVM-DAG: define internal void @counter__add_twice(i32 {{.*}}, ptr

function Counter return Integer is
   Total : Integer := 0;

   procedure Add (N : Integer) is
   begin
      Total := Total + N;
   end Add;

   procedure Add_Twice (N : Integer) is
   begin
      Add (N);
      Add (N);
   end Add_Twice;
begin
   Add_Twice (5);
   return Total;
end Counter;
