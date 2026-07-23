-- RUN: %not %lalvm -P %S/no_such_project.gpr --emit=mlir %s 2>&1 | %FileCheck %s

-- `-P` with a nonexistent project file fails at project load
-- (`project_with_clause` covers only the success path).

-- CHECK: {{Got an exception|project error}}

procedure Invalid_Project is
begin
   null;
end Invalid_Project;
