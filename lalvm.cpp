#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"

#include "ada/Dialect.h"
#include "ada/MLIRGen.h"
#include "ada/Passes.h"

#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
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
#include "llvm/Support/MathExtras.h"
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

/// Attach Ada DWARF debug info to the LLVM module using collected metadata.
/// Uses LLVM's DIBuilder API directly since MLIR 21 lacks DIEnumeratorAttr
/// and retainedTypes on DICompileUnitAttr.
///
/// Emits DW_TAG_enumeration_type for every AdaEnumInfo (enum type
/// declarations). Locally-declared types use the enclosing DISubprogram as
/// scope instead of the compile unit.
static void
attachAdaDebugInfo(llvm::Module &llvmModule,
                   llvm::SmallVector<mlir::ada::AdaEnumInfo> &enumInfos) {
  if (enumInfos.empty())
    return;

  auto *cuMeta = llvmModule.getNamedMetadata("llvm.dbg.cu");
  if (!cuMeta || cuMeta->getNumOperands() == 0)
    return;
  auto *cu = llvm::cast<llvm::DICompileUnit>(cuMeta->getOperand(0));

  llvm::DIBuilder db(llvmModule, /*AllowUnresolved=*/false, cu);
  llvm::DenseMap<llvm::StringRef, llvm::DIFile *> fileCache;

  auto getOrCreateFile = [&](llvm::StringRef filePath) -> llvm::DIFile * {
    auto *&file = fileCache[filePath];
    if (!file)
      file = db.createFile(llvm::sys::path::filename(filePath),
                           llvm::sys::path::parent_path(filePath));
    return file;
  };
  auto getFileAndLine =
      [](mlir::Location loc) -> std::pair<llvm::StringRef, unsigned> {
    if (auto flc = mlir::dyn_cast<mlir::FileLineColRange>(loc))
      return {flc.getFilename().getValue(), flc.getStartLine()};
    return {{}, 0};
  };

  for (auto &info : enumInfos) {
    auto [filePath, line] = getFileAndLine(info.loc);

    llvm::DIScope *scope = cu;
    if (info.subpScope) {
      auto *fn = llvmModule.getFunction(*info.subpScope);
      if (fn && fn->getSubprogram())
        scope = fn->getSubprogram();
    }

    llvm::SmallVector<llvm::Metadata *, 8> elems;
    for (auto [name, val] : llvm::zip(info.names, info.values))
      elems.push_back(db.createEnumerator(name, static_cast<uint64_t>(val)));

    auto *enumType = db.createEnumerationType(
        scope, info.typeName, getOrCreateFile(filePath), line,
        llvm::alignTo(info.bitWidth, 8),
        /*AlignInBits=*/0, db.getOrCreateArray(elems),
        /*UnderlyingType=*/nullptr);
    db.retainType(enumType);
  }

  db.finalize();
}

// Lower Ada dialect ops to LLVM dialect and attach debug info.
// Pre-condition: context must have AdaDialect and ArithDialect loaded.
static int
applyLoweringPasses(mlir::MLIRContext &context,
                    mlir::OwningOpRef<mlir::ModuleOp> &module,
                    llvm::SmallVector<mlir::ada::AdaEnumInfo> &enumInfos) {
  // DI attribute types (DIFileAttr, DICompileUnitAttr, …) belong to the LLVM
  // dialect; load it before creating them.
  context.getOrLoadDialect<mlir::LLVM::LLVMDialect>();
  context.getOrLoadDialect<mlir::memref::MemRefDialect>();

  // Pre-set Ada debug info so DIScopeForLLVMFuncOp uses our compile unit.
  setAdaDebugInfo(*module);

  mlir::PassManager pm(module.get()->getName());
  // Promote alloca-backed variables to SSA values where possible.
  pm.addPass(mlir::createMem2Reg());
  // Lower Ada dialect ops to the LLVM dialect.
  pm.addPass(mlir::ada::createLowerToLLVMPass());
  // Attach DI scope metadata so debuggers can map LLVM IR back to source lines.
  pm.addPass(mlir::LLVM::createDIScopeForLLVMFuncOpPass());
  // Emit debug intrinsics and collect Ada enum type metadata from surviving
  // ada.type ops.
  pm.addPass(mlir::ada::createFinalizeAdaObjectPass(enumInfos));

  if (mlir::failed(pm.run(*module)))
    return 1;
  return 0;
}

static int dumpLLVMIR(mlir::MLIRContext &context,
                      mlir::OwningOpRef<mlir::ModuleOp> &module) {
  llvm::SmallVector<mlir::ada::AdaEnumInfo> enumInfos;
  if (int error = applyLoweringPasses(context, module, enumInfos))
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

  attachAdaDebugInfo(*llvmModule, enumInfos);

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
  case Action::DumpMLIR:
    module->print(llvm::outs());
    llvm::outs() << "\n";
    return 0;
  case Action::DumpLLVMIR:
    return dumpLLVMIR(context, module);
  default:
    llvm_unreachable("unhandled emit action");
  }
}
