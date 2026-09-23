-- RUN: %lalvm --emit=llvm --record-command-line %s | %FileCheck %s --check-prefix=REC
-- RUN: %lalvm --emit=llvm %s | %FileCheck %s --check-prefix=OFF --implicit-check-not=llvm.commandline

-- With --record-command-line the invocation line is stored into `llvm.commandline`.

-- The IR printer escapes the quotes as `\22` (each argument is quoted, see `llvm::sys::printArg`).
-- REC: !llvm.commandline = !{![[N:[0-9]+]]}
-- REC: ![[N]] = !{!"\22{{.*}}lalvm\22 \22--emit=llvm\22 \22--record-command-line\22 \22{{.*}}record_command_line.adb\22"}

-- OFF: define void @_ada_record_command_line(

procedure Record_Command_Line is
begin
   null;
end Record_Command_Line;
