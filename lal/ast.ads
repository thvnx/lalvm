with Interfaces.C; use Interfaces.C;

with Libadalang.Analysis;
with Libadalang.Common;

package AST is

   type CC is record
      node : Libadalang.Analysis.Ada_Node;
   end record
      with Convention => C,
           Export => True;

   function My_Func (a : int) return CC
     with
       Export        => True,
       Convention    => C,
       External_Name => "my_func";

end AST;
