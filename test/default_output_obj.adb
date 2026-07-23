-- RUN: rm -rf %t.dir && mkdir -p %t.dir && cd %t.dir && %lalvm --emit=obj %s && test -f default_output_obj.o

-- `--emit=obj` without `-o` derives the output name from the input basename
-- (`<stem>.o` in the current directory), like a compiler's default output.

function Default_Output_Obj return Integer is
begin
   return 0;
end Default_Output_Obj;
