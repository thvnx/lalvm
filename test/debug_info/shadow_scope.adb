-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- Locally-declared enum types use the enclosing subprogram as their DWARF
-- scope, not the compile unit. Both nested subprograms share the source name
-- "inner" (DW_AT_name), so their scopes are disambiguated by linkageName.
-- Each shadowing "boolean" DICompositeType must attach to the right
-- DISubprogram scope, and each "inner" parameter "b" yields a DILocalVariable.

-- CHECK-DAG: ![[BSCOPE1:[0-9]+]] = distinct !DISubprogram(name: "inner", linkageName: "shadow_scope__inner",
-- CHECK-DAG: DICompositeType(tag: DW_TAG_enumeration_type, name: "boolean", scope: ![[BSCOPE1]],
-- CHECK-DAG: ![[BSCOPE2:[0-9]+]] = distinct !DISubprogram(name: "inner", linkageName: "shadow_scope__inner__inner",
-- CHECK-DAG: DICompositeType(tag: DW_TAG_enumeration_type, name: "boolean", scope: ![[BSCOPE2]],
-- CHECK: DILocalVariable(name: "b", arg: 1,
-- CHECK: DILocalVariable(name: "b", arg: 1,

function Shadow_Scope return Boolean is
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
end Shadow_Scope;
