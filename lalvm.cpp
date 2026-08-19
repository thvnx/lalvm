#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

#include "ada/Dialect.h"
#include "ada/MLIRGen.h"
#include "ada/Passes.h"
#include "ada/PostTranslationDITypes.h"

#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"

#include "mlir/IR/AsmState.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Diagnostics.h"
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
    cl::values(clEnumValN(Ada, "Ada", "load the input file as an Ada source")),
    cl::values(clEnumValN(MLIR, "mlir", "load the input file as an MLIR file")),
    cl::cat(lalvmCategory));

namespace {
enum Action { EmitAST, EmitMLIR, EmitLLVMIR, EmitObject, EmitAssembly };
} // namespace

static cl::opt<enum Action> emitAction(
    "emit", cl::desc("Output format (default: obj)"),
    cl::values(clEnumValN(EmitAST, "ast", "output the AST dump")),
    cl::values(clEnumValN(EmitMLIR, "mlir", "output the MLIR dump")),
    cl::values(clEnumValN(EmitLLVMIR, "llvm", "output the LLVM IR dump")),
    cl::values(clEnumValN(EmitObject, "obj", "output an object file")),
    cl::values(clEnumValN(EmitAssembly, "asm", "output target assembly")),
    cl::init(EmitObject), cl::cat(lalvmCategory));

static cl::opt<std::string> outputFilename("o",
                                           cl::desc("Output filename "
                                                    "(default: stdout)"),
                                           cl::value_desc("filename"),
                                           cl::init("-"),
                                           cl::cat(lalvmCategory));

// GPR project: resolve `with`ed units / separate specs via its unit provider.
// Empty = single file from buffer.
static cl::opt<std::string>
    projectFile("P", cl::desc("GPR project file for unit resolution"),
                cl::value_desc("project.gpr"), cl::init(""),
                cl::cat(lalvmCategory));
static cl::alias projectFileAlias("project", cl::desc("Alias for -P"),
                                  cl::aliasopt(projectFile));

// Optimization level. -O1 enables mem2reg (promoting locals to SSA); -O0, the
// default, leaves locals in memory so they stay breakable and inspectable.
static cl::opt<unsigned> optLevel("O", cl::Prefix, cl::init(0),
                                  cl::desc("Optimization level"),
                                  cl::value_desc("level"),
                                  cl::cat(lalvmCategory));

// Debug info is emitted only under -g; without it the DI passes are skipped.
// Source locations still ride on ops (diagnostics, exception messages).
static cl::opt<bool> debugInfo("g", cl::desc("Generate debug information"),
                               cl::init(false), cl::cat(lalvmCategory));

// Record the invocation in `llvm.commandline` metadata (emitted into the
// object's `.GCC.command.line` section). Off by default: it embeds input/output
// paths, hurting build reproducibility. The joined command line is captured in
// `main`.
//
// @todo A DWARF variant would record into the compile unit's `flags` field,
//       which MLIR's `DICompileUnitAttr` does not expose (only `producer`), so
//       it is blocked on an upstream MLIR change.
static cl::opt<bool>
    recordCommandLine("record-command-line",
                      cl::desc("Record the invocation in llvm.commandline"),
                      cl::init(false), cl::cat(lalvmCategory));
static std::string commandLine;

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
  // Route MLIR parse diagnostics through the source manager so errors print
  // with file:line:col and a source snippet instead of only the terse "can't
  // load file" fallback below.
  mlir::SourceMgrDiagnosticHandler sourceMgrHandler(sourceMgr, &context);
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
  // -O0 leaves locals in memory (see optLevel); mem2reg promotion runs from
  // -O1.
  if (optLevel < 1)
    return 0;
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
  // The DI passes run only under -g. The Ada DI markers earlier passes fuse
  // onto locations stay, but are inert without these consumers: translation
  // drops them and, with no DISubprogram scope, emits no !dbg.
  if (debugInfo)
    // Attach Ada DICompileUnitAttr so DIScopeForLLVMFuncOpPass uses Ada
    // metadata.
    pm.addPass(mlir::ada::createDICompileUnitAdaPass());
  // Hoist nested symbol ops (subprograms and types) to module level and apply
  // GNAT ABI name mangling to subprograms.
  pm.addPass(mlir::ada::createHoistNestedSymbolOperationsPass());
  // Lower Ada dialect ops to the LLVM dialect.
  pm.addPass(mlir::ada::createLowerToLLVMPass());
  if (debugInfo) {
    // Attach DI scope metadata so debuggers can map LLVM IR back to source
    // lines.
    pm.addPass(mlir::LLVM::createDIScopeForLLVMFuncOpPass());
    // Emit debug intrinsics for Ada objects and parameters.
    pm.addPass(mlir::ada::createAdaDebugInfoPass());
  }

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

  // Stamp the compiler identity into `llvm.ident` (it lands in the object's
  // `.comment` section), mirroring how clang and GNAT record their version.
  // lalvm has no version of its own yet, so report the LLVM it was built with.
  llvm::NamedMDNode *ident = llvmModule->getOrInsertNamedMetadata("llvm.ident");
  ident->addOperand(llvm::MDNode::get(
      llvmContext, llvm::MDString::get(
                       llvmContext, "lalvm (LLVM " LLVM_VERSION_STRING ")")));

  // Under -record-command-line, stamp the invocation into `llvm.commandline`
  // (see the flag). The backend lowers it to the object's command-line section.
  if (recordCommandLine) {
    llvm::NamedMDNode *cmd =
        llvmModule->getOrInsertNamedMetadata("llvm.commandline");
    cmd->addOperand(llvm::MDNode::get(
        llvmContext, llvm::MDString::get(llvmContext, commandLine)));
  }

  if (debugInfo) {
    // Request DWARF 5 so the backend emits the modern `.debug_names`
    // accelerator table instead of the deprecated GNU `.debug_pubnames`
    // (the name-table kind stays at its default; the DWARF version is the
    // selector, see `DwarfCompileUnit::hasDwarfPubSections`).
    llvmModule->addModuleFlag(llvm::Module::Max, "Dwarf Version", 5);

    mlir::ada::buildEnumDITypes(*llvmModule, *module);
    mlir::ada::buildSubrangeDITypes(*llvmModule, *module);
  }

  // Initialize the host target backend.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  // Build the host target machine, honoring the codegen flags (-mcpu, -mattr,
  // --relocation-model, ...) registered above, and stamp its triple/datalayout
  // onto the LLVM module. Default the relocation model to PIC when the user did
  // not pass --relocation-model: GNAT links default-PIE on our target, so PIC
  // objects link cleanly against GNAT-compiled code (an explicit flag wins).
  // This is why we build the TargetMachine by hand rather than via
  // codegen::createTargetMachineForTriple, which has no default-override hook.
  llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
  std::string lookupError;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(triple, lookupError);
  if (!target) {
    llvm::errs() << "Could not look up target: " << lookupError << "\n";
    return 1;
  }
  llvm::Reloc::Model relocModel =
      llvm::codegen::getExplicitRelocModel().value_or(llvm::Reloc::PIC_);

  // Record the PIC level so the IR matches the relocation model (clang/GNAT do
  // the same). No PIE level: we emit PIC, not PIE, objects (see note above).
  if (relocModel == llvm::Reloc::PIC_)
    llvmModule->setPICLevel(llvm::PICLevel::BigPIC);

  std::unique_ptr<llvm::TargetMachine> tmOwner(target->createTargetMachine(
      triple, llvm::codegen::getCPUStr(), llvm::codegen::getFeaturesStr(),
      llvm::codegen::InitTargetOptionsFromCodeGenFlags(triple), relocModel,
      llvm::codegen::getExplicitCodeModel()));
  if (!tmOwner) {
    llvm::errs() << "Could not create target machine for " << triple.str()
                 << "\n";
    return 1;
  }
  llvm::TargetMachine &tm = *tmOwner;
  mlir::ExecutionEngine::setupTargetTripleAndDataLayout(llvmModule.get(), &tm);

  // @todo Add an optional optimization pipeline via
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

  // Join the raw invocation for `--record-command-line` (cl leaves argv
  // intact); emitted into `llvm.commandline` in `emitLLVMIR`.
  if (recordCommandLine)
    for (int i = 0; i < argc; ++i)
      commandLine += (i ? " " : "") + std::string(argv[i]);

  bool isMLIRInput = inputType == InputType::MLIR ||
                     llvm::StringRef(inputFilename).ends_with(".mlir");

  // EmitAST is Ada-only and needs no MLIR context.
  if (emitAction == Action::EmitAST) {
    if (isMLIRInput) {
      llvm::errs() << "Can't dump a Libadalang AST when the input is MLIR\n";
      return 1;
    }
    libadalang::AdaAST ast(inputFilename, projectFile);
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
  context.getOrLoadDialect<mlir::cf::ControlFlowDialect>();
  context.getOrLoadDialect<mlir::memref::MemRefDialect>();
  context.getOrLoadDialect<mlir::scf::SCFDialect>();
  mlir::OwningOpRef<mlir::ModuleOp> module;

  if (isMLIRInput) {
    if (int error = loadMLIRFile(context, module))
      return error;
  } else {
    libadalang::AdaAST ast(inputFilename, projectFile);
    if (ast.emitParserDiagnostics())
      return 1;
    if (int error = loadMLIR(ast, context, module))
      return error;
  }

  switch (emitAction) {
  case Action::EmitMLIR: {
    if (int error = applyMLIRPasses(module))
      return error;
    // Under -g, show the debug info in the dump (source locations and local
    // scope), so the NameLocs and DI markers are visible without the explicit
    // --mlir-print-debuginfo / --mlir-print-local-scope flags.
    mlir::OpPrintingFlags flags;
    if (debugInfo)
      flags.enableDebugInfo(/*enable=*/true, /*prettyForm=*/false)
          .useLocalScope();
    return writeTextOutput([&](llvm::raw_ostream &os) {
      module->print(os, flags);
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
