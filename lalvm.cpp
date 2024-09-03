#include "mlir/Dialect/LLVMIR/LLVMDialect.h"

#include "Dialect.h"
#include "MLIRGen.h"
#include "Passes.h"

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
  enum Action { None, DumpAST, DumpMLIR, DumpLLVMIR };
} // namespace

static cl::opt<enum Action>
emitAction("emit",
           cl::desc("Select the kind of output desired"),
           cl::values(clEnumValN(DumpAST, "ast", "output the AST dump")),
           cl::values(clEnumValN(DumpMLIR, "mlir", "output the MLIR dump")),
           cl::values(clEnumValN(DumpLLVMIR, "llvm", "output the LLVM IR dump")));


ada_analysis_context ctx;
ada_analysis_unit unit;
ada_node root;

/// Returns an Ada AST resulting from parsing the file or a nullptr on error.
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

  dump_image(lalAST, 0);
  return 0;
}

int dumpMLIR() {
  mlir::MLIRContext context;
  // Load our Dialect in this MLIR Context.
  context.getOrLoadDialect<mlir::ada::AdaDialect>();

  // Handle '.ad[bs]' input to the compiler.
  if (inputType != InputType::MLIR &&
      !llvm::StringRef(inputFilename).ends_with(".mlir")) {
    auto lalAST = parseInputFile(inputFilename);
    if (!lalAST)
      return 6;
    mlir::OwningOpRef<mlir::ModuleOp> module = ada::mlirGen(context, *lalAST);
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

int loadMLIR(mlir::MLIRContext &context,
             mlir::OwningOpRef<mlir::ModuleOp> &module) {
  // Handle '.toy' input to the compiler.
  if (inputType != InputType::MLIR &&
      !llvm::StringRef(inputFilename).ends_with(".mlir")) {
    auto moduleAST = parseInputFile(inputFilename);
    if (!moduleAST)
      return 6;
    module = ada::mlirGen(context, *moduleAST);
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

int loadAndProcessMLIR(mlir::MLIRContext &context,
                       mlir::OwningOpRef<mlir::ModuleOp> &module) {
  if (int error = loadMLIR(context, module))
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

  switch (emitAction) {
    case Action::DumpAST:
      return dumpAST();
    case Action::DumpMLIR:
      return dumpMLIR();
    case Action::DumpLLVMIR: {
      mlir::MLIRContext context;
      // Load our Dialect in this MLIR Context.
      context.getOrLoadDialect<mlir::ada::AdaDialect>();
      mlir::OwningOpRef<mlir::ModuleOp> module;
      if (int error = loadAndProcessMLIR(context, module))
        return error;
      return dumpLLVMIR(*module);
    }
    default:
      llvm::errs() << "No action specified (parsing only?), use --emit=<action>\n";
  }

  ada_context_decref(ctx);
  abort_on_exception ();

  return 0;
}
