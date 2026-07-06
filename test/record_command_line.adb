-- RUN: %lalvm --emit=llvm --record-command-line %s | %FileCheck %s --check-prefix=REC
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=OFF --implicit-check-not=llvm.commandline

-- With --record-command-line the invocation is stamped into `llvm.commandline`;
-- without it (the default) no such metadata is emitted.

-- REC: !llvm.commandline = !{![[N:[0-9]+]]}
-- REC: ![[N]] = !{!"{{.*}}lalvm --emit=llvm --record-command-line {{.*}}record_command_line.adb"}

-- OFF: define void @_ada_record_command_line(

procedure Record_Command_Line is
begin
   null;
end Record_Command_Line;
