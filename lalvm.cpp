#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include "include/MLIRGen.h"
#include "include/lal.h"

using namespace std;


// Command line

namespace cl = llvm::cl;

static cl::opt<std::string>
inputFilename(cl::Positional,
              cl::desc("<input Ada file>"),
              cl::init("-"),
              cl::value_desc("filename"));

namespace {
  enum InputType { Ada, MLIR };
} // namespace

static cl::opt<enum InputType>
inputType("x",
          cl::init(Ada),
          cl::desc("Decided the kind of output desired"),
          cl::values(clEnumValN(Ada, "Ada",
                                "load the input file as a Ada source.")),
          cl::values(clEnumValN(MLIR, "mlir",
                                "load the input file as an MLIR file")));

namespace {
  enum Action { None, DumpAST, DumpMLIR };
} // namespace

static cl::opt<enum Action>
emitAction("emit",
           cl::desc("Select the kind of output desired"),
           cl::values(clEnumValN(DumpAST, "ast", "output the AST dump")),
           cl::values(clEnumValN(DumpMLIR, "mlir", "output the MLIR dump")));




/// Returns a Ada AST resulting from parsing the file or a nullptr on error.
// TODO remove the use of global variable below, free ctx cleanly, see main.
ada_analysis_context ctx;
ada_analysis_unit unit;
ada_node root;

ada_node* parseInputFile(llvm::StringRef filename) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
    llvm::MemoryBuffer::getFileOrSTDIN(filename);
  if (std::error_code ec = fileOrErr.getError()) {
    llvm::errs() << "Could not open input file: " << ec.message() << "\n";
    return nullptr;
  }
  auto buffer = fileOrErr.get()->getBuffer();

  ctx = ada_allocate_analysis_context ();
  abort_on_exception ();

  ada_initialize_analysis_context (ctx, NULL, NULL, NULL, NULL, 1, 8);
  abort_on_exception ();

  unit = ada_get_analysis_unit_from_buffer(ctx, filename.data(),
                                           NULL, buffer.data(),
                                           strlen(buffer.data()),
                                           ada_default_grammar_rule);
  abort_on_exception ();

  ada_unit_root(unit, &root);
  ada_context_decref(ctx);
  abort_on_exception ();

  return &root;
}


int dumpAST() {
  if (inputType == InputType::MLIR) {
    llvm::errs() << "Can't dump a Libadalang AST when the input is MLIR\n";
    return 5;
  }

  auto lalAST = parseInputFile(inputFilename);
  if (!lalAST)
    return 1;

  dump(lalAST, 0);
  return 0;
}

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "ada compiler\n");

  switch (emitAction) {
  case Action::DumpAST:
    return dumpAST();
  default:
    llvm::errs() << "No action specified (parsing only?), use --emit=<action>\n";
  }

  // TODO: free ctx
  // ada_context_decref(ctx);
  // abort_on_exception ();

  return 0;
}
