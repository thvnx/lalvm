-- RUN: %lalvm --emit=mlir %s | %FileCheck %s --check-prefix=MLIR
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=LLVM

-- Three Boolean types live at different nesting levels:
--   - standard Boolean (False=0, True=1): predefined, emitted at module level.
--   - Inner's local "type Boolean is (True, False)": True=0, False=1 — shadows
--     standard Boolean inside Inner.
--   - Inner.Inner's local "type Boolean is (Not_True, True)": Not_True=0,
--     True=1 — shadows both outer Booleans inside Inner.Inner.
--
-- Constants reflect the rep value of the literal in its resolved type:
--   X := True in Inner       -> outer local True (rep=0) -> false in i1
--   X := True in Inner.Inner -> innermost True (rep=1)   -> true  in i1
--   return True in Inner.Inner -> outer local True (rep=0) -> false in i1
--     (expected type = Inner.Inner's return type = outer local Boolean)
--   return True in Inner      -> standard True (rep=1)   -> true  in i1
--     (expected type = E's Inner return type = standard Boolean)

-- Standard Boolean is lazily emitted at module level when Inner's
-- "return True" resolves to standard Boolean.
-- MLIR: ada.type @standard.boolean : i1 = #ada.enum_info<"false" = 0, "true" = 1>
-- MLIR-LABEL: ada.subp @e
-- MLIR:         ada.subp @inner(%arg0: i1 {ada.type = @standard.boolean}
-- MLIR:           ada.type @boolean : i1 = #ada.enum_info<"true" = 0, "false" = 1>
-- MLIR:           arith.constant{{.*}}false
-- MLIR:           ada.subp @inner(%arg1: i1 {ada.type = @boolean}
-- MLIR:             ada.type @boolean : i1 = #ada.enum_info<"not_true" = 0, "true" = 1>
-- MLIR:             arith.constant{{.*}}true
-- MLIR:             ada.return
-- MLIR:           arith.constant{{.*}}true
-- MLIR:           ada.return
-- MLIR:         arith.constant{{.*}}true
-- MLIR:         ada.call @inner(
-- MLIR:         ada.return

-- Post-order hoisting: Inner.Inner (deeper) is moved to module end first,
-- then Inner.
-- LLVM-LABEL: define i1 @_ada_e(
-- LLVM:          call i1 @e__inner(
-- LLVM-LABEL: define i1 @e__inner__inner(
-- LLVM-LABEL: define i1 @e__inner(
-- Locally-declared Boolean types use the enclosing subprogram as DWARF scope,
-- not the compile unit.
-- LLVM-DAG: DICompositeType(tag: DW_TAG_enumeration_type, name: "boolean", scope: ![[BSCOPE1:[0-9]+]],
-- LLVM-DAG: ![[BSCOPE1]] = distinct !DISubprogram(name: "e__inner",
-- LLVM-DAG: DICompositeType(tag: DW_TAG_enumeration_type, name: "boolean", scope: ![[BSCOPE2:[0-9]+]],
-- LLVM-DAG: ![[BSCOPE2]] = distinct !DISubprogram(name: "e__inner__inner",
-- LLVM: DILocalVariable(name: "B", arg: 1,
-- LLVM: DILocalVariable(name: "B", arg: 1,

function E return Boolean is
   function Inner (B : Boolean) return Boolean is
      type Boolean is (True, False);

      X : Boolean := True;

      function Inner (B : Boolean) return Boolean is
         type Boolean is (Not_True, True);

         X : Boolean := True;
      begin
         return True;
      end Inner;
   begin
      return True;
   end Inner;
begin
   return Inner (True);
end E;
