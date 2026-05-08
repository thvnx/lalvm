#include "mlir/Dialect/Arith/IR/Arith.h"
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
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include "ada/MLIRGen.h"
#include "lal/AST.h"

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
  enum Action { None, DumpAST, DumpMLIR, DumpLLVMIR };
} // namespace

static cl::opt<enum Action>
emitAction("emit",
           cl::desc("Select the kind of output desired"),
           cl::values(clEnumValN(DumpAST, "ast", "output the AST dump")),
           cl::values(clEnumValN(DumpMLIR, "mlir", "output the MLIR dump")),
           cl::values(clEnumValN(DumpLLVMIR, "llvm", "output the LLVM IR dump")));

int dumpAST(libadalang::AdaAST ast) {
  if (inputType == InputType::MLIR) {
    llvm::errs() << "Can't dump a Libadalang AST when the input is MLIR\n";
    return 5;
  }

  if (!ast.isValid())
    return 1;

  ast.dump();

  return 0;
}

int dumpMLIR(libadalang::AdaAST ast) {
  mlir::MLIRContext context;
  // Load our Dialect in this MLIR Context.
  context.getOrLoadDialect<mlir::ada::AdaDialect>();
  context.getOrLoadDialect<mlir::arith::ArithDialect>();

  // Handle '.ad[bs]' input to the compiler.
  if (inputType != InputType::MLIR &&
      !llvm::StringRef(inputFilename).ends_with(".mlir")) {
    if (!ast.isValid())
      return 6;
    mlir::OwningOpRef<mlir::ModuleOp> module = ada::mlirGen(context, ast.getUnitRootNode());
    if (!module)
      return 1;

    module->dump();
    return 0;
  }

  // Otherwise, the input is '.mlir'.
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
    llvm::MemoryBuffer::getFileOrSTDIN(inputFilename);
  if (std::error_code ec = fileOrErr.getError()) {
    llvm::errs() << "Could not open input file: " << ec.message() << "\n";
    return -1;
  }

  // Parse the input mlir.
  llvm::SourceMgr sourceMgr;
  sourceMgr.AddNewSourceBuffer(std::move(*fileOrErr), llvm::SMLoc());
  mlir::OwningOpRef<mlir::ModuleOp> module =
    mlir::parseSourceFile<mlir::ModuleOp>(sourceMgr, &context);
  if (!module) {
    llvm::errs() << "Error can't load file " << inputFilename << "\n";
    return 3;
  }

  module->dump();
  return 0;
}

int loadMLIR(libadalang::AdaAST ast,
             mlir::MLIRContext &context,
             mlir::OwningOpRef<mlir::ModuleOp> &module) {
  // Handle '.toy' input to the compiler.
  if (inputType != InputType::MLIR &&
      !llvm::StringRef(inputFilename).ends_with(".mlir")) {
    if (!ast.isValid())
      return 6;
    module = ada::mlirGen(context, ast.getUnitRootNode());
    return !module ? 1 : 0;
  }

  // Otherwise, the input is '.mlir'.
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
      llvm::MemoryBuffer::getFileOrSTDIN(inputFilename);
  if (std::error_code ec = fileOrErr.getError()) {
    llvm::errs() << "Could not open input file: " << ec.message() << "\n";
    return -1;
  }

  // Parse the input mlir.
  llvm::SourceMgr sourceMgr;
  sourceMgr.AddNewSourceBuffer(std::move(*fileOrErr), llvm::SMLoc());
  module = mlir::parseSourceFile<mlir::ModuleOp>(sourceMgr, &context);
  if (!module) {
    llvm::errs() << "Error can't load file " << inputFilename << "\n";
    return 3;
  }
  return 0;
}

int loadAndProcessMLIR(libadalang::AdaAST ast,
                       mlir::MLIRContext &context,
                       mlir::OwningOpRef<mlir::ModuleOp> &module) {
  if (int error = loadMLIR(ast, context, module))
    return error;

  mlir::PassManager pm(module.get()->getName());
  // Apply any generic pass manager command line options and run the pipeline.
  //if (mlir::failed(mlir::applyPassManagerCLOptions(pm)))
  //  return 4;

  // // Check to see what granularity of MLIR we are compiling to.
  // bool isLoweringToAffine = emitAction >= Action::DumpMLIRAffine;
  // bool isLoweringToLLVM = emitAction >= Action::DumpMLIRLLVM;

  // if (enableOpt || isLoweringToAffine) {
  //   // Inline all functions into main and then delete them.
  //   pm.addPass(mlir::createInlinerPass());

  //   // Now that there is only one function, we can infer the shapes of each of
  //   // the operations.
  //   mlir::OpPassManager &optPM = pm.nest<mlir::toy::FuncOp>();
  //   optPM.addPass(mlir::createCanonicalizerPass());
  //   optPM.addPass(mlir::toy::createShapeInferencePass());
  //   optPM.addPass(mlir::createCanonicalizerPass());
  //   optPM.addPass(mlir::createCSEPass());
  // }

  // if (isLoweringToAffine) {
  //   // Partially lower the toy dialect.
  //   pm.addPass(mlir::toy::createLowerToAffinePass());

  //   // Add a few cleanups post lowering.
  //   mlir::OpPassManager &optPM = pm.nest<mlir::func::FuncOp>();
  //   optPM.addPass(mlir::createCanonicalizerPass());
  //   optPM.addPass(mlir::createCSEPass());

  //   // Add optimizations if enabled.
  //   if (enableOpt) {
  //     optPM.addPass(mlir::affine::createLoopFusionPass());
  //     optPM.addPass(mlir::affine::createAffineScalarReplacementPass());
  //   }
  // }

  //if (isLoweringToLLVM) {
    // Finish lowering the toy IR to the LLVM dialect.
    pm.addPass(mlir::ada::createLowerToLLVMPass());
    // This is necessary to have line tables emitted and basic
    // debugger working. In the future we will add proper debug information
    // emission directly from our frontend.
    pm.addPass(mlir::LLVM::createDIScopeForLLVMFuncOpPass());
  //}

  if (mlir::failed(pm.run(*module)))
    return 8;
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
    return -1;
  }

  // Initialize LLVM targets.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  // Create target machine and configure the LLVM Module
  auto tmBuilderOrError = llvm::orc::JITTargetMachineBuilder::detectHost();
  if (!tmBuilderOrError) {
    llvm::errs() << "Could not create JITTargetMachineBuilder\n";
    return -1;
  }

  auto tmOrError = tmBuilderOrError->createTargetMachine();
  if (!tmOrError) {
    llvm::errs() << "Could not create TargetMachine\n";
    return -1;
  }
  mlir::ExecutionEngine::setupTargetTripleAndDataLayout(llvmModule.get(),
                                                        tmOrError.get().get());

  // /// Optionally run an optimization pipeline over the llvm module.
  // auto optPipeline = mlir::makeOptimizingTransformer(
  //   /*optLevel=*/enableOpt ? 3 : 0, /*sizeLevel=*/0,
  //   /*targetMachine=*/nullptr);
  // if (auto err = optPipeline(llvmModule.get())) {
  //   llvm::errs() << "Failed to optimize LLVM IR " << err << "\n";
  //   return -1;
  // }
  llvm::errs() << *llvmModule << "\n";
  return 0;
}

int main(int argc, char **argv) {
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
      llvm::errs() << "No action specified (parsing only?), use --emit=<action>\n";
  }

  return 0;
}
