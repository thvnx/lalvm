with Ada.Text_IO; use Ada.Text_IO;

package body AST is

   function My_Func (a : int) return CC is
      package LAL renames Libadalang.Analysis;
      package LCO renames Libadalang.Common;

      Context : constant LAL.Analysis_Context := LAL.Create_Context;
      Unit    : constant LAL.Analysis_Unit :=
        Context.Get_From_Buffer (Filename => "toto.adb",
                                 Buffer => "type T is new Integer;",
                                 Rule => LCO.Type_Decl_Rule);

      Ret : CC;
   begin

         if Unit.Has_Diagnostics then
            for D of Unit.Diagnostics loop
               Put_Line (Unit.Format_GNU_Diagnostic (D));
            end loop;

         --  Otherwise, look for object declarations
         --  else
         --   Unit.Root.Traverse (Process_Node'Access);
         end if;

         Unit.Root.Print;



      return CC'(Node => Unit.Root);
   end My_Func;

end AST;
