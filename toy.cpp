#include <iostream>

#include "include/MLIRGen.h"

using namespace std;

extern "C" int my_func (int a);


int main(int argc, char **argv) {
//  cl::ParseCommandLineOptions(argc, argv, "toy compiler\n");

  //auto moduleAST = parseInputFile(inputFilename);
  //if (!moduleAST)
   // return 1;

 // switch (emitAction) {
 // case Action::DumpAST:
  //  dump(*moduleAST);
   // return 0;
  //default:
  //  llvm::errs() << "No action specified (parsing only?), use -emit=<action>\n";
 // }
  std::cout << toy::fn (2);
  std::cout << my_func (2);

  return 0;
}
