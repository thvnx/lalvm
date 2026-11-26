// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "lalvm/Binder.h"

#include "ada/Dialect.h"
#include "frontend/AST.h"
#include "frontend/DiagnosticPrinter.h"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Verifier.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Path.h"

#include <cstdlib>
#include <optional>
#include <string>

/// @todo Handle elaboration order: the closure of the main unit (its `with`
/// clauses, transitively, through the project's unit provider) (@rm{10-2},
/// @rm{10-2-1})
///
/// @todo The runtime hooks: its initialization and finalization entry points,
/// and the globals receiving `argc`, `argv` and the exit status.

namespace libadalang = frontend::libadalang;
namespace LLVM = mlir::LLVM;

namespace {

/// Represent the program to bind. `describeProgram` fills it from the main
/// unit and `emitBindModule` consumes it (it never looks at Libadalang).
struct Program {
  /// Unit name of the program to bind.
  std::string unitName;
  /// Mangled symbol of the main subprogram.
  std::string mainSymbol;
  /// Whether the main subprogram is a function.
  bool mainIsFunction;
  /// Location of the main subprogram's specification (used by every op in the
  /// bind module).
  mlir::Location loc;
};

} // namespace

/// Whether `typeExpr` designates `Standard.Integer` or one of its subtypes.
static bool isStandardInteger(ada_node &typeExpr) {
  std::optional<ada_node> decl = libadalang::designatedTypeDecl(typeExpr);
  if (!decl)
    return false;
  std::optional<ada_node> canon = libadalang::canonicalType(*decl);
  if (!canon)
    return false;
  ada_node intType = {};
  return ada_ada_node_p_int_type(&*canon, &intType) &&
         !ada_node_is_null(&intType) &&
         ada_node_is_equivalent(&*canon, &intType);
}

/// Describe the program whose main unit is `root`. The main unit must be the
/// body of a parameterless library-level subprogram returning `Integer` or
/// nothing (@rm{10-2}). Return nullopt on error.
static std::optional<Program> describeProgram(mlir::MLIRContext &context,
                                              ada_node &root) {
  auto error = [&](ada_node &at, const llvm::Twine &message) {
    mlir::emitError(libadalang::sourceLocation(context, at), message);
    return std::nullopt;
  };

  if (ada_node_kind(&root) != ada_compilation_unit)
    return error(root, "the main unit must be a single compilation unit");
  if (std::optional<llvm::StringRef> kind = libadalang::specKind(root))
    return error(root, "cannot bind a " + *kind +
                           ": the main unit must be a subprogram body");

  auto item = libadalang::libraryItem(root);
  if (!item)
    return error(root, "the main unit must be a library-level subprogram body");
  ada_node &body = item.value();
  if (ada_node_kind(&body) != ada_subp_body)
    return error(body, "the main unit must be a subprogram body");

  ada_node spec = {};
  ada_base_subp_body_f_subp_spec(&body, &spec);

  ada_node_array params = nullptr;
  if (ada_base_subp_spec_p_params(&spec, &params) && params->n > 0) {
    ada_node first = params->items[0];
    ada_node_array_dec_ref(params);
    return error(first, "the main subprogram cannot have parameters");
  }
  if (params)
    ada_node_array_dec_ref(params);

  auto resultType = libadalang::subpResultType(spec);
  if (resultType && !isStandardInteger(resultType.value()))
    return error(resultType.value(), "the main function must return Integer");

  auto defName = libadalang::declDefiningName(body);
  if (!defName)
    return std::nullopt;
  std::string symbol =
      mlir::ada::libraryLevelSymbol(libadalang::getName(&defName.value()));

  std::string unitName =
      llvm::sys::path::stem(libadalang::unitFilename(root)).str();

  return Program{unitName, symbol, resultType.has_value(),
                 libadalang::sourceLocation(context, spec)};
}

/// Emit the bind module of `program`. It contains the external declaration of
/// the main subprogram, and the `main (argc, argv)` entry point calling the
/// main subprogram and returning the exit status.
static mlir::OwningOpRef<mlir::ModuleOp>
emitBindModule(mlir::MLIRContext &context, const Program &program) {
  context.getOrLoadDialect<LLVM::LLVMDialect>();
  mlir::OpBuilder builder(&context);
  mlir::Location loc = program.loc;

  std::string moduleName = "b_" + program.unitName;
  mlir::OwningOpRef<mlir::ModuleOp> module =
      mlir::ModuleOp::create(loc, llvm::StringRef(moduleName));
  builder.setInsertionPointToEnd(module->getBody());

  mlir::Type i32Ty = builder.getI32Type();
  mlir::Type ptrTy = LLVM::LLVMPointerType::get(&context);
  mlir::Type voidTy = LLVM::LLVMVoidType::get(&context);

  auto mainSubp = LLVM::LLVMFuncOp::create(
      builder, loc, program.mainSymbol,
      LLVM::LLVMFunctionType::get(program.mainIsFunction ? i32Ty : voidTy,
                                  llvm::ArrayRef<mlir::Type>{}));

  // The two arguments are unused until the runtime stores them.
  auto mainFn = LLVM::LLVMFuncOp::create(
      builder, loc, "main", LLVM::LLVMFunctionType::get(i32Ty, {i32Ty, ptrTy}));
  mlir::Block *entry = mainFn.addEntryBlock(builder);
  builder.setInsertionPointToStart(entry);
  auto call = LLVM::CallOp::create(builder, loc, mainSubp, mlir::ValueRange{});
  mlir::Value status;
  if (program.mainIsFunction)
    status = call.getResult();
  else
    status = LLVM::ConstantOp::create(builder, loc, i32Ty,
                                      builder.getI32IntegerAttr(0));
  LLVM::ReturnOp::create(builder, loc, mlir::ValueRange{status});

  if (mlir::failed(mlir::verify(*module))) {
    module->emitError("module verification error");
    return nullptr;
  }
  return module;
}

mlir::OwningOpRef<mlir::ModuleOp> lalvm::bind(mlir::MLIRContext &context,
                                              ada_node &compilationUnit) {
  frontend::ScopedDiagnosticPrinter diagPrinter(&context);

  std::optional<Program> program = describeProgram(context, compilationUnit);
  if (!program)
    return nullptr;
  return emitBindModule(context, *program);
}
