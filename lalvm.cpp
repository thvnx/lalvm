#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"

#include "ada/Dialect.h"
#include "ada/EnumDITypes.h"
#include "ada/MLIRGen.h"
#include "ada/Passes.h"

#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"

#include "mlir/IR/AsmState.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"

#include "ada/ToLLVMIRTranslation.h"
#include "mlir/Dialect/LLVMIR/Transforms/Passes.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/ExecutionEngine/ExecutionEngine.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Target/LLVMIR/Dialect/Builtin/BuiltinToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Dialect/LLVMIR/LLVMToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Export.h"
#include "mlir/Transforms/Passes.h"

#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include "frontend/AST.h"

// Command line

namespace cl = llvm::cl;
namespace libadalang = frontend::libadalang;

// lalvm's own options live in this category; HideUnrelatedOptions (in main)
// hides everything LLVM/MLIR back ends register from --help.
static cl::OptionCategory lalvmCategory("lalvm options");

static cl::opt<std::string>
    inputFilename(cl::Positional, cl::desc("<input Ada file>"), cl::init("-"),
                  cl::value_desc("filename"), cl::cat(lalvmCategory));

namespace {
enum InputType { Ada, MLIR };
} // namespace

static cl::opt<enum InputType> inputType(
    "x", cl::init(Ada), cl::desc("Decides the kind of input to load"),
    cl::values(clEnumValN(Ada, "Ada", "load the input file as a Ada source.")),
    cl::values(clEnumValN(MLIR, "mlir", "load the input file as an MLIR file")),
    cl::cat(lalvmCategory));

namespace {
enum Action { None, EmitAST, EmitMLIR, EmitLLVMIR, EmitObject, EmitAssembly };
} // namespace

static cl::opt<enum Action> emitAction(
    "emit", cl::desc("Select the kind of output desired"),
    cl::values(clEnumValN(EmitAST, "ast", "output the AST dump")),
    cl::values(clEnumValN(EmitMLIR, "mlir", "output the MLIR dump")),
    cl::values(clEnumValN(EmitLLVMIR, "llvm", "output the LLVM IR dump")),
    cl::values(clEnumValN(EmitObject, "obj", "output an object file")),
    cl::values(clEnumValN(EmitAssembly, "asm", "output target assembly")),
    cl::cat(lalvmCategory));

static cl::opt<std::string> outputFilename("o",
                                           cl::desc("Output filename "
                                                    "(default: stdout)"),
                                           cl::value_desc("filename"),
                                           cl::init("-"),
                                           cl::cat(lalvmCategory));

/// Register the standard codegen flags (-mcpu, -mattr, --relocation-model,
/// --code-model, ...) shared with llc; consumed when building the target
/// machine for --emit=obj/asm.
///
/// @todo Also register a -mtriple option (and initialize all target backends)
///       to emit for a non-host target. Deferred until cross-compilation is
///       needed; it also requires applying the target datalayout to the module
///       before lowering.
static llvm::codegen::RegisterCodeGenFlags codeGenFlags;

// Load a .mlir file directly, bypassing Libadalang entirely.
static int loadMLIRFile(mlir::MLIRContext &context,
                        mlir::OwningOpRef<mlir::ModuleOp> &module) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
      llvm::MemoryBuffer::getFileOrSTDIN(inputFilename);
  if (std::error_code ec = fileOrErr.getError()) {
    llvm::errs() << "Could not open input file: " << ec.message() << "\n";
    return 1;
  }
  llvm::SourceMgr sourceMgr;
  sourceMgr.AddNewSourceBuffer(std::move(*fileOrErr), llvm::SMLoc());
  module = mlir::parseSourceFile<mlir::ModuleOp>(sourceMgr, &context);
  if (!module) {
    llvm::errs() << "Error can't load file " << inputFilename << "\n";
    return 1;
  }
  return 0;
}

// Generate MLIR from Ada source via Libadalang.
static int loadMLIR(libadalang::AdaAST &ast, mlir::MLIRContext &context,
                    mlir::OwningOpRef<mlir::ModuleOp> &module) {
  if (!ast.isValid())
    return 1;
  module = lalvm::mlirGen(context, ast.getUnitRootNode());
  return !module ? 1 : 0;
}

static int applyMLIRPasses(mlir::OwningOpRef<mlir::ModuleOp> &module) {
  // Separate PM so --mlir-print-ir-before=mem2reg captures pre-promotion IR.
  mlir::PassManager pm(module.get()->getName());
  pm.addPass(mlir::createMem2Reg());
  if (mlir::failed(mlir::applyPassManagerCLOptions(pm)))
    return 1;
  if (mlir::failed(pm.run(*module)))
    return 1;
  return 0;
}

// Lower Ada dialect ops to LLVM dialect and attach debug info.
// Pre-condition: context must have AdaDialect and ArithDialect loaded.
static int applyLoweringPasses(mlir::MLIRContext &context,
                               mlir::OwningOpRef<mlir::ModuleOp> &module) {
  // DI attribute types (DIFileAttr, DICompileUnitAttr, ...) belong to the LLVM
  // dialect; load it before creating them.
  context.getOrLoadDialect<mlir::LLVM::LLVMDialect>();
  context.getOrLoadDialect<mlir::memref::MemRefDialect>();

  // Lambda-lift up-level references and hoist nested subprograms to module
  // level before mem2reg, so a captured local stays an alloca able to carry
  // up-level writes. Run only on the lowering path, leaving --emit=mlir as the
  // nested dialect view.
  mlir::PassManager ccPm(module.get()->getName());
  ccPm.addPass(mlir::ada::createClosureConversionPass());
  if (mlir::failed(mlir::applyPassManagerCLOptions(ccPm)))
    return 1;
  if (mlir::failed(ccPm.run(*module)))
    return 1;

  if (int error = applyMLIRPasses(module))
    return error;

  mlir::PassManager pm(module.get()->getName());
  // Attach Ada DICompileUnitAttr so DIScopeForLLVMFuncOpPass uses Ada metadata.
  pm.addPass(mlir::ada::createDICompileUnitAdaPass());
  // Hoist nested symbol ops (subprograms and types) to module level and apply
  // GNAT ABI name mangling to subprograms.
  pm.addPass(mlir::ada::createHoistNestedSymbolOperationsPass());
  // Lower Ada dialect ops to the LLVM dialect.
  pm.addPass(mlir::ada::createLowerToLLVMPass());
  // Attach DI scope metadata so debuggers can map LLVM IR back to source lines.
  pm.addPass(mlir::LLVM::createDIScopeForLLVMFuncOpPass());
  // Emit debug intrinsics for Ada objects and parameters.
  pm.addPass(mlir::ada::createAdaDebugInfoPass());

  if (mlir::failed(mlir::applyPassManagerCLOptions(pm)))
    return 1;
  if (mlir::failed(pm.run(*module)))
    return 1;
  return 0;
}

// Write textual output produced by `print` to `outputFilename` ("-" = stdout).
static int
writeTextOutput(llvm::function_ref<void(llvm::raw_ostream &)> print) {
  std::error_code ec;
  llvm::raw_fd_ostream out(outputFilename, ec, llvm::sys::fs::OF_Text);
  if (ec) {
    llvm::errs() << "Could not open output file '" << outputFilename
                 << "': " << ec.message() << "\n";
    return 1;
  }
  print(out);
  return 0;
}

// Run the LLVM backend to emit an object file or target assembly for
// `llvmModule` (using host target machine `tm`). Output goes to -o when given,
// otherwise to the input basename with a .o/.s extension (in the current
// directory), mirroring a compiler's default object/assembly output.
static int emitMachineCode(llvm::Module &llvmModule, llvm::TargetMachine &tm,
                           bool emitObject) {
  std::string path = outputFilename;
  if (outputFilename.getNumOccurrences() == 0) {
    llvm::StringRef stem =
        inputFilename == "-" ? "a" : llvm::sys::path::stem(inputFilename);
    path = stem.str() + (emitObject ? ".o" : ".s");
  }

  // Writing a binary object to a terminal produces garbage; require -o (llc
  // does the same). Assembly is text and prints fine.
  if (emitObject && path == "-" &&
      llvm::sys::Process::StandardOutIsDisplayed()) {
    llvm::errs() << "Refusing to write a binary object file to the terminal; "
                    "use -o <file>\n";
    return 1;
  }

  std::error_code ec;
  llvm::raw_fd_ostream out(
      path, ec, emitObject ? llvm::sys::fs::OF_None : llvm::sys::fs::OF_Text);
  if (ec) {
    llvm::errs() << "Could not open output file '" << path
                 << "': " << ec.message() << "\n";
    return 1;
  }

  // addPassesToEmitFile writes via pwrite; a non-seekable stream (e.g. a pipe)
  // must be buffered first.
  llvm::raw_pwrite_stream *os = &out;
  std::unique_ptr<llvm::buffer_ostream> buffered;
  if (!out.supportsSeeking()) {
    buffered = std::make_unique<llvm::buffer_ostream>(out);
    os = buffered.get();
  }

  llvm::legacy::PassManager pm;
  if (tm.addPassesToEmitFile(pm, *os, /*DwoOut=*/nullptr,
                             emitObject
                                 ? llvm::CodeGenFileType::ObjectFile
                                 : llvm::CodeGenFileType::AssemblyFile)) {
    llvm::errs() << "Target cannot emit a "
                 << (emitObject ? "object" : "assembly") << " file\n";
    return 1;
  }
  pm.run(llvmModule);
  return 0;
}

static int emitLLVMIR(mlir::MLIRContext &context,
                      mlir::OwningOpRef<mlir::ModuleOp> &module) {
  if (int error = applyLoweringPasses(context, module))
    return error;

  // Register the translation to LLVM IR with the MLIR context.
  mlir::registerBuiltinDialectTranslation(context);
  mlir::registerLLVMDialectTranslation(context);
  mlir::ada::registerAdaDialectTranslation(context);

  // Convert the module to LLVM IR in a new LLVM IR context.
  llvm::LLVMContext llvmContext;
  auto llvmModule = mlir::translateModuleToLLVMIR(*module, llvmContext);
  if (!llvmModule) {
    llvm::errs() << "Failed to emit LLVM IR\n";
    return 1;
  }

  llvmModule->setModuleIdentifier(llvm::sys::path::filename(inputFilename));
  llvmModule->setSourceFileName(inputFilename);

  // Request DWARF 5 so the backend emits the modern `.debug_names`
  // accelerator table instead of the deprecated GNU `.debug_pubnames`
  // (the name-table kind stays at its default; the DWARF version is the
  // selector, see `DwarfCompileUnit::hasDwarfPubSections`).
  llvmModule->addModuleFlag(llvm::Module::Max, "Dwarf Version", 5);

  mlir::ada::buildEnumDITypes(*llvmModule, *module);

  // Initialize the host target backend.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  // Build the host target machine, honoring the codegen flags (-mcpu, -mattr,
  // --relocation-model, ...) registered above, and stamp its triple/datalayout
  // onto the LLVM module.
  auto tmOrError = llvm::codegen::createTargetMachineForTriple(
      llvm::sys::getDefaultTargetTriple());
  if (!tmOrError) {
    llvm::errs() << "Could not create target machine: "
                 << llvm::toString(tmOrError.takeError()) << "\n";
    return 1;
  }
  llvm::TargetMachine &tm = *tmOrError.get();
  mlir::ExecutionEngine::setupTargetTripleAndDataLayout(llvmModule.get(), &tm);

  // TODO: add an optional optimization pipeline via
  // mlir::makeOptimizingTransformer.
  if (emitAction == Action::EmitLLVMIR)
    return writeTextOutput(
        [&](llvm::raw_ostream &os) { os << *llvmModule << "\n"; });

  return emitMachineCode(*llvmModule, tm, emitAction == Action::EmitObject);
}

int main(int argc, char **argv) {
  // InitLLVM sets up signal handlers, pretty stack traces, and registers
  // LLVM's command line options (including -debug and -debug-only).
  llvm::InitLLVM x(argc, argv);
  mlir::registerAsmPrinterCLOptions();
  mlir::registerMLIRContextCLOptions();
  mlir::registerTransformsPasses();
  mlir::registerPassManagerCLOptions();
  // Hide the options LLVM/MLIR back ends register so --help lists only lalvm's
  // own options.
  cl::HideUnrelatedOptions(lalvmCategory);
  cl::ParseCommandLineOptions(argc, argv, "Ada to LLVM Compiler\n");

  if (emitAction == Action::None) {
    llvm::errs()
        << "No action specified (parsing only?), use --emit=<action>\n";
    return 1;
  }

  bool isMLIRInput = inputType == InputType::MLIR ||
                     llvm::StringRef(inputFilename).ends_with(".mlir");

  // EmitAST is Ada-only and needs no MLIR context.
  if (emitAction == Action::EmitAST) {
    if (isMLIRInput) {
      llvm::errs() << "Can't dump a Libadalang AST when the input is MLIR\n";
      return 1;
    }
    libadalang::AdaAST ast(inputFilename);
    if (ast.emitParserDiagnostics())
      return 1;
    if (!ast.isValid())
      return 1;
    return writeTextOutput([&](llvm::raw_ostream &os) { ast.dump(os); });
  }

  // EmitMLIR and EmitLLVMIR: load a module then emit.
  mlir::MLIRContext context;
  context.getOrLoadDialect<mlir::ada::AdaDialect>();
  context.getOrLoadDialect<mlir::arith::ArithDialect>();
  context.getOrLoadDialect<mlir::memref::MemRefDialect>();
  mlir::OwningOpRef<mlir::ModuleOp> module;

  if (isMLIRInput) {
    if (int error = loadMLIRFile(context, module))
      return error;
  } else {
    libadalang::AdaAST ast(inputFilename);
    if (ast.emitParserDiagnostics())
      return 1;
    if (int error = loadMLIR(ast, context, module))
      return error;
  }

  switch (emitAction) {
  case Action::EmitMLIR: {
    if (int error = applyMLIRPasses(module))
      return error;
    return writeTextOutput([&](llvm::raw_ostream &os) {
      module->print(os);
      os << "\n";
    });
  }
  case Action::EmitLLVMIR:
  case Action::EmitObject:
  case Action::EmitAssembly:
    return emitLLVMIR(context, module);
  default:
    llvm_unreachable("unhandled emit action");
  }
}
