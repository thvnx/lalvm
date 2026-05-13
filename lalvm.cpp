#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"

#include "ada/Dialect.h"
#include "ada/MLIRGen.h"
#include "ada/Passes.h"

#include "mlir/IR/AsmState.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"

#include "mlir/Dialect/LLVMIR/Transforms/Passes.h"
#include "mlir/ExecutionEngine/ExecutionEngine.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Target/LLVMIR/Dialect/Builtin/BuiltinToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Dialect/LLVMIR/LLVMToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Export.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include "lal/AST.h"

// Command line

namespace cl = llvm::cl;

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

int dumpAST(libadalang::AdaAST ast) {
  if (inputType == InputType::MLIR) {
    llvm::errs() << "Can't dump a Libadalang AST when the input is MLIR\n";
    return 1;
  }

  if (!ast.isValid())
    return 1;

  ast.dump();

  return 0;
}

int loadMLIR(libadalang::AdaAST ast, mlir::MLIRContext &context,
             mlir::OwningOpRef<mlir::ModuleOp> &module) {
  // Handle '.ad[bs]' input to the compiler.
  if (inputType != InputType::MLIR &&
      !llvm::StringRef(inputFilename).ends_with(".mlir")) {
    if (!ast.isValid())
      return 1;
    module = ada::mlirGen(context, ast.getUnitRootNode());
    return !module ? 1 : 0;
  }

  // Otherwise, the input is '.mlir'.
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
      llvm::MemoryBuffer::getFileOrSTDIN(inputFilename);
  if (std::error_code ec = fileOrErr.getError()) {
    llvm::errs() << "Could not open input file: " << ec.message() << "\n";
    return 1;
  }

  // Parse the input mlir.
  llvm::SourceMgr sourceMgr;
  sourceMgr.AddNewSourceBuffer(std::move(*fileOrErr), llvm::SMLoc());
  module = mlir::parseSourceFile<mlir::ModuleOp>(sourceMgr, &context);
  if (!module) {
    llvm::errs() << "Error can't load file " << inputFilename << "\n";
    return 1;
  }
  return 0;
}

int dumpMLIR(libadalang::AdaAST ast) {
  mlir::MLIRContext context;
  context.getOrLoadDialect<mlir::ada::AdaDialect>();
  context.getOrLoadDialect<mlir::arith::ArithDialect>();
  mlir::OwningOpRef<mlir::ModuleOp> module;
  if (int error = loadMLIR(ast, context, module))
    return error;
  module->print(llvm::outs());
  llvm::outs() << "\n";
  return 0;
}

/// Attach an Ada-correct DICompileUnitAttr to the module location so that
/// DIScopeForLLVMFuncOp picks it up (via its FusedLocWith<DICompileUnitAttr>
/// hook) instead of defaulting to DW_LANG_C / "MLIR".
static void setAdaDebugInfo(mlir::ModuleOp module) {
  mlir::MLIRContext *ctx = module.getContext();

  llvm::StringRef filePath;
  if (auto loc = mlir::dyn_cast<mlir::FileLineColRange>(module.getLoc()))
    filePath = loc.getFilename().getValue();

  auto fileAttr =
      mlir::LLVM::DIFileAttr::get(ctx, llvm::sys::path::filename(filePath),
                                  llvm::sys::path::parent_path(filePath));

  // DW_LANG_Ada2012 = 0x002f (DWARF5, §7.12 table 7.17)
  constexpr unsigned kDW_LANG_Ada2012 = 0x002f;
  auto cuAttr = mlir::LLVM::DICompileUnitAttr::get(
      mlir::DistinctAttr::create(mlir::UnitAttr::get(ctx)), kDW_LANG_Ada2012,
      fileAttr, mlir::StringAttr::get(ctx, "lalvm"),
      /*isOptimized=*/false, mlir::LLVM::DIEmissionKind::Full);

  module->setLoc(mlir::FusedLoc::get(ctx, {module.getLoc()}, cuAttr));
}

// Full compilation pipeline: Ada source → Ada MLIR dialect → LLVM dialect.
// The MLIR module is modified in place; the caller then translates it to LLVM
// IR. Pre-condition: context must have AdaDialect and ArithDialect loaded.
int loadAndProcessMLIR(libadalang::AdaAST ast, mlir::MLIRContext &context,
                       mlir::OwningOpRef<mlir::ModuleOp> &module) {
  if (int error = loadMLIR(ast, context, module))
    return error;

  // DI attribute types (DIFileAttr, DICompileUnitAttr, …) belong to the LLVM
  // dialect; load it before creating them.
  context.getOrLoadDialect<mlir::LLVM::LLVMDialect>();

  // Pre-set Ada debug info so DIScopeForLLVMFuncOp uses our compile unit.
  setAdaDebugInfo(*module);

  mlir::PassManager pm(module.get()->getName());
  // Lower Ada dialect ops to the LLVM dialect.
  pm.addPass(mlir::ada::createLowerToLLVMPass());
  // Attach DI scope metadata so debuggers can map LLVM IR back to source lines.
  pm.addPass(mlir::LLVM::createDIScopeForLLVMFuncOpPass());

  if (mlir::failed(pm.run(*module)))
    return 1;
  return 0;
}

int dumpLLVMIR(mlir::ModuleOp module) {
  // Register the translation to LLVM IR with the MLIR context.
  mlir::registerBuiltinDialectTranslation(*module->getContext());
  mlir::registerLLVMDialectTranslation(*module->getContext());

  // Convert the module to LLVM IR in a new LLVM IR context.
  llvm::LLVMContext llvmContext;
  auto llvmModule = mlir::translateModuleToLLVMIR(module, llvmContext);
  if (!llvmModule) {
    llvm::errs() << "Failed to emit LLVM IR\n";
    return 1;
  }

  llvmModule->setModuleIdentifier(llvm::sys::path::filename(inputFilename));
  llvmModule->setSourceFileName(inputFilename);

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
  cl::ParseCommandLineOptions(argc, argv, "ada compiler\n");

  libadalang::AdaAST ast(inputFilename);

  switch (emitAction) {
  case Action::DumpAST:
    return dumpAST(ast);
  case Action::DumpMLIR:
    return dumpMLIR(ast);
  case Action::DumpLLVMIR: {
    mlir::MLIRContext context;
    // Load our Dialect in this MLIR Context.
    context.getOrLoadDialect<mlir::ada::AdaDialect>();
    context.getOrLoadDialect<mlir::arith::ArithDialect>();
    mlir::OwningOpRef<mlir::ModuleOp> module;
    if (int error = loadAndProcessMLIR(ast, context, module))
      return error;
    return dumpLLVMIR(*module);
  }
  default:
    llvm::errs()
        << "No action specified (parsing only?), use --emit=<action>\n";
    return 1;
  }

  return 0;
}
