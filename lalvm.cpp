#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"

#include "ada/Dialect.h"
#include "ada/EnumDITypes.h"
#include "ada/MLIRGen.h"
#include "ada/Passes.h"

#include "llvm/IR/Module.h"

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

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include "frontend/AST.h"

// Command line

namespace cl = llvm::cl;
namespace libadalang = frontend::libadalang;

static cl::opt<std::string> inputFilename(cl::Positional,
                                          cl::desc("<input Ada file>"),
                                          cl::init("-"),
                                          cl::value_desc("filename"));

namespace {
enum InputType { Ada, MLIR };
} // namespace

static cl::opt<enum InputType> inputType(
    "x", cl::init(Ada), cl::desc("Decides the kind of input to load"),
    cl::values(clEnumValN(Ada, "Ada", "load the input file as a Ada source.")),
    cl::values(clEnumValN(MLIR, "mlir",
                          "load the input file as an MLIR file")));

namespace {
enum Action { None, DumpAST, DumpMLIR, DumpLLVMIR };
} // namespace

static cl::opt<enum Action> emitAction(
    "emit", cl::desc("Select the kind of output desired"),
    cl::values(clEnumValN(DumpAST, "ast", "output the AST dump")),
    cl::values(clEnumValN(DumpMLIR, "mlir", "output the MLIR dump")),
    cl::values(clEnumValN(DumpLLVMIR, "llvm", "output the LLVM IR dump")));

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

static int dumpLLVMIR(mlir::MLIRContext &context,
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

  // Initialize LLVM targets.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  // Create target machine and configure the LLVM Module
  auto tmBuilderOrError = llvm::orc::JITTargetMachineBuilder::detectHost();
  if (!tmBuilderOrError) {
    llvm::errs() << "Could not create JITTargetMachineBuilder\n";
    return 1;
  }

  auto tmOrError = tmBuilderOrError->createTargetMachine();
  if (!tmOrError) {
    llvm::errs() << "Could not create TargetMachine\n";
    return 1;
  }
  mlir::ExecutionEngine::setupTargetTripleAndDataLayout(llvmModule.get(),
                                                        tmOrError.get().get());

  // TODO: add an optional optimization pipeline via
  // mlir::makeOptimizingTransformer.
  llvm::outs() << *llvmModule << "\n";
  return 0;
}

int main(int argc, char **argv) {
  // InitLLVM sets up signal handlers, pretty stack traces, and registers
  // LLVM's command line options (including -debug and -debug-only).
  llvm::InitLLVM x(argc, argv);
  mlir::registerAsmPrinterCLOptions();
  mlir::registerMLIRContextCLOptions();
  mlir::registerTransformsPasses();
  mlir::registerPassManagerCLOptions();
  cl::ParseCommandLineOptions(argc, argv, "ada compiler\n");

  if (emitAction == Action::None) {
    llvm::errs()
        << "No action specified (parsing only?), use --emit=<action>\n";
    return 1;
  }

  bool isMLIRInput = inputType == InputType::MLIR ||
                     llvm::StringRef(inputFilename).ends_with(".mlir");

  // DumpAST is Ada-only and needs no MLIR context.
  if (emitAction == Action::DumpAST) {
    if (isMLIRInput) {
      llvm::errs() << "Can't dump a Libadalang AST when the input is MLIR\n";
      return 1;
    }
    libadalang::AdaAST ast(inputFilename);
    if (ast.emitParserDiagnostics())
      return 1;
    if (!ast.isValid())
      return 1;
    ast.dump();
    return 0;
  }

  // DumpMLIR and DumpLLVMIR: load a module then emit.
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
  case Action::DumpMLIR: {
    if (int error = applyMLIRPasses(module))
      return error;
    module->print(llvm::outs());
    llvm::outs() << "\n";
    return 0;
  }
  case Action::DumpLLVMIR:
    return dumpLLVMIR(context, module);
  default:
    llvm_unreachable("unhandled emit action");
  }
}
