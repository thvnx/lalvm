#include <iostream>
#include <cstring>

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"


#include "include/MLIRGen.h"
#include "libadalang.h"

using namespace std;

namespace cl = llvm::cl;

static cl::opt<std::string> inputFilename(cl::Positional,
                                          cl::desc("<input toy file>"),
                                          cl::init("-"),
                                          cl::value_desc("filename"));

namespace {
enum InputType { Toy, MLIR };
} // namespace
static cl::opt<enum InputType> inputType(
    "x", cl::init(Toy), cl::desc("Decided the kind of output desired"),
    cl::values(clEnumValN(Toy, "toy", "load the input file as a Toy source.")),
    cl::values(clEnumValN(MLIR, "mlir",
                          "load the input file as an MLIR file")));

namespace {
enum Action { None, DumpAST, DumpMLIR };
} // namespace
static cl::opt<enum Action> emitAction(
    "emit", cl::desc("Select the kind of output desired"),
    cl::values(clEnumValN(DumpAST, "ast", "output the AST dump")),
    cl::values(clEnumValN(DumpMLIR, "mlir", "output the MLIR dump")));



/// Returns a Toy AST resulting from parsing the file or a nullptr on error.
ada_analysis_context ctx;
ada_analysis_unit unit;
ada_node root;
ada_node * parseInputFile(llvm::StringRef filename) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
      llvm::MemoryBuffer::getFileOrSTDIN(filename);
  if (std::error_code ec = fileOrErr.getError()) {
    llvm::errs() << "Could not open input file: " << ec.message() << "\n";
    return nullptr;
  }
  auto buffer = fileOrErr.get()->getBuffer();
  // LexerBuffer lexer(buffer.begin(), buffer.end(), std::string(filename));
  // Parser parser(lexer);
  // return parser.parseModule();



    ctx = ada_allocate_analysis_context ();
    //abort_on_exception ();

    ada_initialize_analysis_context (ctx, NULL, NULL, NULL, NULL, 1, 8);
    //abort_on_exception ();

    unit = ada_get_analysis_unit_from_buffer(ctx, "foo.adb", NULL, buffer.data(),
				     strlen(buffer.data()),
				     ada_default_grammar_rule);
    //abort_on_exception ();

    ada_unit_root(unit, &root);
    ada_context_decref(ctx);
    //abort_on_exception ();

    return &root;
}



//extern "C" void* my_func (int a);



const char *src_buffer = (
  //"limited with Ada.Text_IO;\n"
  //"\n"
  //"procedure Foo is\n"
  //"   function \"+\" (S : String) return String is (S);\n"
  //"begin\n"
  //"   Ada.Text_IO.Put_Line (+\"Hello, world!\");\n"
  //"end Foo;\n"
  "function Foo (I : Integer) return Boolean is\n"
  "begin\n"
  "   return False;\n"
  "end Foo;\n"
);


static bool                     //
print_exception (bool or_silent)
{
  const ada_exception *exc = ada_get_last_exception ();
  if (exc != NULL)
    {
      char *exc_name = ada_exception_name (exc->kind);
      printf ("Got an exception (%s):\n  %s\n",
	     exc_name,
	     exc->information);
      free (exc_name);
      return true;
    }
  else if (! or_silent)
    puts ("Got no exception\n");
  return false;
}

static void
abort_on_exception (void)
{
  if (print_exception (true))
    exit (1);
}

static void
print_indent(int level)
{
    int i;
    for (i = 0; i < level; ++i)
        printf("| ");
}

static void
fprint_text(FILE *stream, ada_text text, bool with_quotes) {
    unsigned i;

    if (with_quotes)
        fputc('"', stream);

    for (i = 0; i < text.length; i++) {
        uint32_t c = text.chars[i];

        if ((with_quotes && c == '"')
            || c == '\\')
            fprintf(stream, "\\%c", (char) c);
        else if (0x20 <= c && c <= 0x7f) {
            fputc(c, stream);
        } else if (c <= 0xff) {
            fprintf(stream, "\\x");
            fprintf(stream, "%02x", c);
        } else if (c <= 0xffff) {
            fprintf(stream, "\\u");
            fprintf(stream, "%02x", c >> 8);
            fprintf(stream, "%02x", c & 0xff);
        } else {
            fprintf(stream, "\\U");
            fprintf(stream, "%02x", c >> 24);
            fprintf(stream, "%02x", (c >> 16) & 0xff);
            fprintf(stream, "%02x", (c >> 8) & 0xff);
            fprintf(stream, "%02x", c & 0xff);
        }
    }

    if (with_quotes)
        fputc('"', stream);
}

static void
dump(ada_node *node, int level)
{
    ada_node_kind_enum kind;
    ada_text kind_name;
    unsigned i, count;

    if (ada_node_is_null(node)) {
        print_indent(level);
        printf("<null node>\n");
        return;
    }

    kind = ada_node_kind(node);
    ada_kind_name(kind, &kind_name);
    print_indent(level);
    putchar('<');
    fprint_text(stdout, kind_name, false);
    puts(">");

    count = ada_node_children_count(node);
    for (i = 0; i < count; ++i)
    {
        ada_node child;

        if (ada_node_child(node, i, &child) == 0)
            std::cerr << "Error while getting a child";
        dump(&child, level + 1);
    }
}


int dumpAST() {
  if (inputType == InputType::MLIR) {
    llvm::errs() << "Can't dump a Toy AST when the input is MLIR\n";
    return 5;
  }

  auto moduleAST = parseInputFile(inputFilename);
  if (!moduleAST)
    return 1;

  dump(moduleAST, 0);
  return 0;
}



int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "toy compiler\n");

  //auto moduleAST = parseInputFile(inputFilename);
  //if (!moduleAST)
   // return 1;


 //  std::cout << toy::fn (4) << std::endl;
 // // std::cout << my_func (4) << std::endl;


 //    ada_analysis_context ctx;
 //    ada_analysis_unit unit;
 //    ada_node root;

 //    ctx = ada_allocate_analysis_context ();
 //    abort_on_exception ();

 //    ada_initialize_analysis_context (ctx, NULL, NULL, NULL, NULL, 1, 8);
 //    abort_on_exception ();

 //    unit = ada_get_analysis_unit_from_buffer(ctx, "foo.adb", NULL, src_buffer,
 //    			     strlen(src_buffer),
 //    			     ada_default_grammar_rule);
 //    abort_on_exception ();

 //    ada_unit_root(unit, &root);
    //dump(&root, 0);

    //ada_context_decref(ctx);
    //abort_on_exception ();


    switch (emitAction) {
    case Action::DumpAST:
      // //dump(*moduleAST);
      // dump(&root, 0);
      // //return 0;
      // break;
      return dumpAST();
    default:
      llvm::errs() << "No action specified (parsing only?), use --emit=<action>\n";
    }

    // ada_context_decref(ctx);
    // abort_on_exception ();


  return 0;
}
