#include "ada/MLIRGen.h"

#include "ada/Dialect.h"
#include "frontend/AST.h"
#include "frontend/DiagnosticPrinter.h"
#include "libadalang.h"
#include "mlir/IR/Block.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LogicalResult.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/IR/Verifier.h"

#include "mlir/Dialect/Arith/IR/Arith.h"

#include "llvm/Support/Debug.h"

namespace libadalang = frontend::libadalang;

#define DEBUG_TYPE "ada-mlirgen"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <optional>
#include <vector>

using llvm::ArrayRef;
using llvm::cast;
using llvm::isa;
using llvm::SmallVector;

// Known limitations of this codegen:
//
//  - Integer overflow: Ada defines integer arithmetic over mathematical
//    integers with range checks that raise Constraint_Error on overflow. This
//    implementation lowers to arith.addi/subi/muli, which use two's-complement
//    wrapping semantics with no overflow check. Any Ada code relying on
//    Constraint_Error for integer overflow will silently produce wrong results.
//
//  - Uninitialized variables: variables without an initializer are not
//    supported; a reference to one is diagnosed as an error. Supporting them
//    requires switching to an alloca-based model (stack slot per variable,
//    llvm.load/llvm.store, mem2reg to promote to SSA).
//
//  - `in out` parameters: write-back to the caller's variable is not
//    implemented. The updated value is stored in the local symbol table only.

namespace {

/// Walks a Libadalang AST and emits Ada dialect MLIR operations into a module
/// (i.e.: a compilation unit). The public entry point is mlirGen() at the
/// bottom of this file.
class MLIRGenImpl {
public:
  MLIRGenImpl(mlir::MLIRContext &context) : builder(&context) {}

  /// Public API: convert an Ada compilation unit to an MLIR module.
  mlir::ModuleOp mlirGen(ada_node &compilationUnit) {
    // Install a diagnostic handler that prints all diagnostics (including
    // warnings) to stderr. Without this, MLIR's diagnostic engine silently
    // drops non-error diagnostics.
    mlir::ScopedDiagnosticHandler diagHandler(
        builder.getContext(), [](mlir::Diagnostic &diag) {
          frontend::DiagnosticPrinter().emitDiag(diag);
          return mlir::success();
        });

    // We create an empty MLIR module and walk the entire compilation unit to
    // codegen its contents into it.
    adaModule = mlir::ModuleOp::create(loc(compilationUnit));

    // Use a simple Libadalang AST traversal approach based on the C API.
    if (mlir::failed(visit(compilationUnit)))
      return nullptr;

    // Verify the module after we have finished constructing it, this will check
    // the structural properties of the IR and invoke any specific verifiers we
    // have on the Ada operations.
    if (failed(mlir::verify(adaModule))) {
      adaModule.emitError("module verification error");
      return nullptr;
    }

    return adaModule;
  }

private:
  /// The MLIR module being built. In MLIR parlance, a module is the top-level
  /// container for operations; it maps to an Ada compilation unit.
  mlir::ModuleOp adaModule;

  /// Helper for creating MLIR operations. Stateful: it tracks an "insertion
  /// point" that determines where the next operation will be emitted.
  mlir::OpBuilder builder;

  // Maps each DefiningName node to its current SSA Value. The key is the
  // ada_base_node pointer, which is Libadalang's unique node identity.
  // Using node identity instead of name strings means references always
  // resolve to the correct declaration regardless of name shadowing —
  // Libadalang's cross-references handle scoping for us.
  //
  // In the SSA model, assignment rebinds the same key to a new value rather
  // than mutating it; nodeValues always holds the latest SSA value for each
  // declaration. No scope cleanup is needed: for valid Ada, Libadalang
  // rejects references to out-of-scope declarations before MLIRGen runs.
  llvm::DenseMap<ada_base_node, mlir::Value> nodeValues;

  // Named numbers (RM 3.3.2) have universal integer or real type with no
  // concrete type annotation.  We stash the expression node at declaration
  // time and re-emit the constant at each use site so that the use-site
  // identifier's p_expected_expression_type resolves the concrete MLIR type
  // (e.g. Integer → i32 for `return Max_Size;` in a function returning
  // Integer).
  llvm::DenseMap<ada_base_node, ada_node> numberDeclExprs;

  /// Helper conversion for a Libadalang AST location to an MLIR location.
  mlir::Location loc(const ada_node &node) {
    // INFO: MLIR provides richer location kinds (NameLoc, FusedLoc,
    // CallSiteLoc, etc.) that could be used to improve diagnostics and debug
    // info.

    // const_cast: libadalang C API doesn't have const-qualified overloads;
    // the underlying objects are never actually const.
    ada_node *n = const_cast<ada_node *>(&node);
    ada_source_location_range loc_range;
    ada_node_sloc_range(n, &loc_range);

    ada_source_location loc_start = loc_range.start;
    ada_source_location loc_end = loc_range.end;
    char *filename = ada_unit_filename(ada_node_unit(n));

    // getStringAttr copies the string into the MLIR context, so filename can
    // be freed immediately.
    auto result = mlir::FileLineColRange::get(builder.getStringAttr(filename),
                                              loc_start.line, loc_start.column,
                                              loc_end.line, loc_end.column);
    free(filename);
    return result;
  }

  /// Bind a DefiningName node to an SSA value. For assignment, this overwrites
  /// the previous binding for the same node — the SSA model's way of tracking
  /// the latest value of a mutable variable without alloca/load/store.
  void declare(ada_node &def_name, mlir::Value value) {
    LLVM_DEBUG(llvm::dbgs()
               << "declare: " << libadalang::image(&def_name) << "\n");
    nodeValues[def_name.node] = value;
  }

  // Recursive AST walker. Handles the node kinds we know how to codegen;
  // everything else is ignored at this level and its children are visited.
  // Returning early (without visiting children) stops descent into a subtree
  // (used when a handler already walked it, e.g. mlirGenSubpBody visits stmts).
  mlir::LogicalResult visit(ada_node &moduleAST) {
    ada_bool isEntryPoint = 0;
    if (ada_ada_node_p_xref_entry_point(&moduleAST, &isEntryPoint) &&
        isEntryPoint && libadalang::emitSolverDiagnostics(&moduleAST))
      return mlir::failure();

    switch (ada_node_kind(&moduleAST)) {
    case ada_subp_body:
      // Top-level subprograms are emitted at module scope. The insertion point
      // is set here rather than inside mlirGenSubpBody so that nested
      // subprograms (processed via mlirGenDeclarativePart) are instead emitted
      // at the current insertion point inside the enclosing body region.
      builder.setInsertionPointToEnd(adaModule.getBody());
      if (!mlirGenSubpBody(moduleAST))
        return mlir::failure();
      return mlir::success();
    case ada_return_stmt:
      return mlirGenReturn(moduleAST);
    case ada_assign_stmt:
      if (mlir::failed(mlirGenAssign(moduleAST)))
        return mlir::failure();
      return mlir::success();
    case ada_null_stmt:
      builder.create<mlir::ada::NullOp>(loc(moduleAST));
      return mlir::success();
    case ada_call_stmt:
      return mlirGenCallStmt(moduleAST);
    case ada_named_stmt: {
      // Named block statement: "Name: [declare] begin ... end Name;"
      // The name lives on the wrapping named_stmt; the actual block is f_stmt.
      ada_node decl, nameNode, stmt;
      ada_named_stmt_f_decl(&moduleAST, &decl);
      ada_named_stmt_decl_f_name(&decl, &nameNode);
      ada_named_stmt_f_stmt(&moduleAST, &stmt);
      ada_text nameText;
      ada_node_text(&nameNode, &nameText);
      return mlirGenBlockStmt(stmt, libadalang::textToString(nameText));
    }
    case ada_begin_block:
    case ada_decl_block:
      return mlirGenBlockStmt(moduleAST, {});
    case ada_compilation_unit:
    case ada_ada_node_list:
    case ada_library_item:
    case ada_private_absent:
    case ada_private_present:
    case ada_pragma_node_list:
    case ada_handled_stmts:
    case ada_stmt_list:
      // Transparent nodes — visit children without warning.
      break;
    default: {
      // TODO: turn this into an Error when lalvm is mature enough.
      mlir::emitWarning(loc(moduleAST), "visit: unhandled node '")
          << libadalang::image(&moduleAST) << "'";
      break;
    }
    }

    unsigned i, count = ada_node_children_count(&moduleAST);
    for (i = 0; i < count; ++i) {
      ada_node child;
      if (ada_node_child(&moduleAST, i, &child) == 0) {
        mlir::emitError(loc(moduleAST), "failed to get child node");
        return mlir::failure();
      }
      if (!ada_node_is_null(&child) && mlir::failed(visit(child)))
        return mlir::failure();
    }
    return mlir::success();
  }

  /// Emit a variable reference. Resolves via Libadalang cross-reference to
  /// the unique DefiningName node, then looks up the current SSA value.
  mlir::Value mlirGenVariable(ada_node &expr) {
    // Fast path: resolve to DefiningName and look up in nodeValues.
    ada_node def_name;
    if (ada_name_p_referenced_defining_name(&expr, /*imprecise_fallback=*/0,
                                            &def_name) &&
        !ada_node_is_null(&def_name)) {
      if (auto it = nodeValues.find(def_name.node); it != nodeValues.end())
        return it->second;
    }

    // Slow path: distinguish error kinds for better diagnostics.
    ada_node ref_decl;
    if (ada_name_p_referenced_decl(&expr, /*imprecise_fallback=*/0,
                                   &ref_decl) &&
        !ada_node_is_null(&ref_decl)) {
      if (ada_node_kind(&ref_decl) == ada_object_decl) {
        // Declared but not yet in nodeValues: never initialised or assigned.
        ada_node default_expr;
        ada_object_decl_f_default_expr(&ref_decl, &default_expr);
        if (ada_node_is_null(&default_expr)) {
          // TODO: downgrade to a warning once the alloca-based model is in
          // place.
          mlir::emitError(loc(ref_decl), "variable '")
              << libadalang::getName(&expr, false)
              << "' is read but never assigned";
          return nullptr;
        }
      } else {
        // Declared but not a variable (type, subprogram, etc.).
        mlir::emitError(loc(expr), "cannot use '")
            << libadalang::getName(&expr, false) << "' as a value";
        return nullptr;
      }
    }

    mlir::emitError(loc(expr), "undeclared identifier '")
        << libadalang::getName(&expr, false) << "'";
    return nullptr;
  }

  /// Emit the arithmetic operation for two precomputed operands.
  /// `op` is the ada_op node (ada_op_plus, ada_op_minus, etc.).
  mlir::Value emitBinOp(ada_node &op, mlir::Value lhs, mlir::Value rhs) {
    auto location = loc(op);
    mlir::ada::AdaBinaryOp kind;
    switch (ada_node_kind(&op)) {
    case ada_op_plus:
      kind = mlir::ada::AdaBinaryOp::Plus;
      break;
    case ada_op_minus:
      kind = mlir::ada::AdaBinaryOp::Minus;
      break;
    case ada_op_mult:
      kind = mlir::ada::AdaBinaryOp::Mult;
      break;
    default:
      mlir::emitError(location, "invalid binary operator '")
          << libadalang::image(&op) << "'";
      return nullptr;
    }
    // Op nodes (ada_op_plus, etc.) derive from Name and support
    // p_referenced_decl. For user-defined (overloaded) operators, the resolved
    // declaration is the operator function and p_is_predefined_operator returns
    // false. Predefined operators may resolve to a built-in decl or to null.
    ada_node refDecl;
    if (ada_name_p_referenced_decl(&op, /*imprecise_fallback=*/0, &refDecl) &&
        !ada_node_is_null(&refDecl)) {
      ada_bool isPredefined = 0;
      ada_basic_decl_p_is_predefined_operator(&refDecl, &isPredefined);
      if (!isPredefined) {
        mlir::emitError(location, "operator overloading is not supported");
        return nullptr;
      }
    }
    return builder.create<mlir::ada::BinOp>(location, kind, lhs, rhs);
  }

  /// Emit a binary operation.
  mlir::Value mlirGenBinOp(ada_node &binop) {
    ada_node left;
    ada_bin_op_f_left(&binop, &left);
    mlir::Value lhs = visit_expr(left);
    if (!lhs)
      return nullptr;
    ada_node right;
    ada_bin_op_f_right(&binop, &right);
    mlir::Value rhs = visit_expr(right);
    if (!rhs)
      return nullptr;
    ada_node op;
    ada_bin_op_f_op(&binop, &op);
    return emitBinOp(op, lhs, rhs);
  }

  /// Resolve the type of a literal expression. For universal types
  /// (universal_int_type_ / universal_real_type_), falls back to the expected
  /// type from the surrounding context. Returns a null node on failure.
  ada_node resolveLiteralType(ada_node &node, mlir::Location location,
                              llvm::StringRef universalTypeName) {
    ada_node type_decl;
    if (!ada_expr_p_expression_type(&node, &type_decl) ||
        ada_node_is_null(&type_decl)) {
      mlir::emitError(location, "failed to resolve type of literal");
      return {};
    }
    ada_node type_name;
    if (ada_base_type_decl_f_name(&type_decl, &type_name) &&
        !ada_node_is_null(&type_name) &&
        libadalang::getName(&type_name) == universalTypeName) {
      if (!ada_expr_p_expected_expression_type(&node, &type_decl) ||
          ada_node_is_null(&type_decl)) {
        mlir::emitError(location, "failed to resolve expected type of literal");
        return {};
      }
    }
    return type_decl;
  }

  /// Emit an arith.ConstantIntOp. Range-checks the value against the target
  /// integer width; emits a diagnostic and returns nullptr if it doesn't fit.
  mlir::Value emitIntConstant(int64_t value, mlir::IntegerType type,
                              mlir::Location location) {
    unsigned width = type.getWidth();
    if (width < 64) {
      int64_t maxVal = (1LL << (width - 1)) - 1;
      int64_t minVal = -(1LL << (width - 1));
      if (value < minVal || value > maxVal) {
        mlir::emitError(location, "integer constant ")
            << value << " out of range for i" << width;
        return nullptr;
      }
    }
    return builder.create<mlir::arith::ConstantIntOp>(location, value, width);
  }

  /// Emit an arith.ConstantOp for a floating-point value. Checks for f32
  /// overflow; emits a diagnostic and returns nullptr on failure.
  mlir::Value emitRealConstant(double value, mlir::FloatType type,
                               mlir::Location location) {
    if (type.getWidth() == 32 && std::isinf(static_cast<float>(value))) {
      mlir::emitError(location, "real constant ")
          << value << " out of range for f32";
      return nullptr;
    }
    return builder.create<mlir::arith::ConstantOp>(
        location, builder.getFloatAttr(type, value));
  }

  /// Extract the integer value of an ada_int_literal node via
  /// `p_denoted_value`. Returns nullopt and emits a diagnostic on failure.
  std::optional<int64_t> evalIntLiteral(ada_node &node) {
    // p_denoted_value gives us the evaluated integer value as a big integer.
    // We convert it through its UTF-8 text representation since there is no
    // direct C API to extract a 64-bit integer from ada_big_integer.
    ada_big_integer bigint;
    if (!ada_int_literal_p_denoted_value(&node, &bigint)) {
      mlir::emitError(loc(node), "failed to evaluate integer literal");
      return std::nullopt;
    }
    ada_text text;
    ada_big_integer_text(bigint, &text);
    std::string literal = libadalang::textToString(text);
    ada_big_integer_decref(bigint);
    errno = 0;
    char *endptr;
    int64_t value =
        static_cast<int64_t>(std::strtoll(literal.c_str(), &endptr, 10));
    if (endptr == literal.c_str()) {
      mlir::emitError(loc(node), "failed to parse integer literal '")
          << literal << "'";
      return std::nullopt;
    }
    if (errno == ERANGE) {
      mlir::emitError(loc(node), "integer literal ")
          << literal << " out of range for i64";
      return std::nullopt;
    }
    return value;
  }

  mlir::Value mlirGenIntLiteral(ada_node &node) {
    auto value = evalIntLiteral(node);
    if (!value)
      return nullptr;
    // p_expression_type on an integer literal returns universal_integer
    // (Libadalang's "universal_int_type_"), not the concrete type.
    // When that happens, p_expected_expression_type gives the type required
    // by the surrounding context (e.g. the return type of the enclosing
    // function).
    ada_node type_decl =
        resolveLiteralType(node, loc(node), "universal_int_type_");
    if (ada_node_is_null(&type_decl))
      return nullptr;
    mlir::Type type = getMLIRTypeFromDecl(type_decl, loc(node));
    if (!type)
      return nullptr;
    return emitIntConstant(*value, mlir::cast<mlir::IntegerType>(type),
                           loc(node));
  }

  /// Extract the floating-point value of an ada_real_literal node via its
  /// source text. Ada allows underscores as digit separators; they are
  /// stripped before parsing. Returns nullopt and emits a diagnostic on
  /// failure.
  std::optional<double> evalRealLiteral(ada_node &node) {
    // Unlike ada_int_literal, ada_real_literal has no p_denoted_value in the
    // C API, so we extract the value by reading the literal's source text.
    ada_text text;
    ada_node_text(&node, &text);
    std::string raw = libadalang::textToString(text);
    std::string literal;
    literal.reserve(raw.size());
    std::copy_if(raw.begin(), raw.end(), std::back_inserter(literal),
                 [](char c) { return c != '_'; });
    errno = 0;
    char *endptr;
    double value = std::strtod(literal.c_str(), &endptr);
    if (endptr == literal.c_str()) {
      mlir::emitError(loc(node), "failed to parse real literal '")
          << literal << "'";
      return std::nullopt;
    }
    if (errno == ERANGE || std::isinf(value)) {
      mlir::emitError(loc(node), "real literal ")
          << literal << " out of range for f64";
      return std::nullopt;
    }
    return value;
  }

  mlir::Value mlirGenRealLiteral(ada_node &node) {
    auto value = evalRealLiteral(node);
    if (!value)
      return nullptr;
    // Real literals have universal_real type; fall back to the expected type
    // to get the concrete type required by the surrounding context.
    ada_node type_decl =
        resolveLiteralType(node, loc(node), "universal_real_type_");
    if (ada_node_is_null(&type_decl))
      return nullptr;
    mlir::Type type = getMLIRTypeFromDecl(type_decl, loc(node));
    if (!type)
      return nullptr;
    return emitRealConstant(*value, mlir::cast<mlir::FloatType>(type),
                            loc(node));
  }

  /// Emit a subprogram call from an ada_identifier (no-arg) or ada_call_expr
  /// (with args) node. Returns the CallOp on success, nullptr on error.
  /// For function calls the op has one result; for procedure calls none.
  mlir::ada::CallOp mlirGenCallExpr(ada_node &call) {
    auto location = loc(call);

    ada_node name_node;
    llvm::SmallVector<mlir::Value> args;

    switch (ada_node_kind(&call)) {
    case ada_identifier:
      name_node = call;
      break;
    case ada_call_expr: {
      ada_call_expr_f_name(&call, &name_node);
      ada_node suffix;
      ada_call_expr_f_suffix(&call, &suffix);
      int n = ada_node_children_count(&suffix);
      for (int i = 0; i < n; ++i) {
        ada_node assoc, r_expr;
        ada_node_child(&suffix, i, &assoc);
        ada_param_assoc_f_r_expr(&assoc, &r_expr);
        mlir::Value val = visit_expr(r_expr);
        if (!val)
          return nullptr;
        args.push_back(val);
      }
      break;
    }
    default:
      mlir::emitError(location, "unsupported call expression");
      return nullptr;
    }

    auto calleeName = libadalang::getName(&name_node);

    // Walk the chain of enclosing ada.subp ops looking for the callee, then
    // fall back to the module.  ada.subp carries SymbolTable, so
    // lookupNearestSymbolFrom would stop at the immediately enclosing
    // subprogram and never see a sibling nested subprogram.  Walking the parent
    // chain manually gives us Ada's "visible from any enclosing scope" rule
    // while still respecting SymbolTable opacity toward the outside.
    mlir::Operation *op =
        builder.getInsertionBlock()->getParent()->getParentOp();
    mlir::Operation *calleeOp = nullptr;
    while (isa<mlir::ada::SubpOp>(op) && !calleeOp) {
      calleeOp = mlir::SymbolTable::lookupSymbolIn(op, calleeName.data());
      op = op->getParentOp();
    }
    if (!calleeOp)
      calleeOp =
          mlir::SymbolTable::lookupSymbolIn(adaModule, calleeName.data());

    if (!calleeOp || !isa<mlir::ada::SubpOp>(calleeOp)) {
      mlir::emitError(location, "unknown subprogram '") << calleeName << "'";
      return nullptr;
    }

    auto callLoc = mlir::CallSiteLoc::get(calleeOp->getLoc(), location);

    // Function call: resolve the return type from LAL and pass it to CallOp.
    // Procedure call: no result, fall through to the no-result builder.
    if (mlir::cast<mlir::ada::SubpOp>(calleeOp)
            .getFunctionType()
            .getNumResults() > 0) {
      ada_node type_decl;
      if (!ada_expr_p_expression_type(&call, &type_decl) ||
          ada_node_is_null(&type_decl)) {
        mlir::emitError(location,
                        "failed to resolve return type of function call");
        return nullptr;
      }
      mlir::Type retType = getMLIRTypeFromDecl(type_decl, location);
      if (!retType)
        return nullptr;
      return builder.create<mlir::ada::CallOp>(callLoc, calleeName.data(),
                                               retType, args);
    }

    return builder.create<mlir::ada::CallOp>(callLoc, calleeName.data(), args);
  }

  /// Emit a static expression at its use site, with the concrete MLIR type
  /// resolved from `typeContext` (typically the identifier that names the
  /// constant, so `p_expected_expression_type` on it returns the type
  /// required by the surrounding context).
  ///
  /// Universal integer: evaluated in full via `eval_as_int`.
  /// Universal real literal: parsed from source text via `evalRealLiteral`.
  /// Universal real bin_op: emitted op-by-op recursively with a warning
  ///   (libadalang has no `eval_as_real`; LLVM folds the resulting ops).
  mlir::Value visit_static_expr(ada_node &staticExpr, ada_node &typeContext) {
    ada_node exprType;
    if (!ada_expr_p_expression_type(&staticExpr, &exprType) ||
        ada_node_is_null(&exprType)) {
      mlir::emitError(loc(staticExpr),
                      "failed to resolve type of static expression");
      return nullptr;
    }
    ada_node typeNameNode;
    ada_base_type_decl_f_name(&exprType, &typeNameNode);
    std::string universalType = ada_node_is_null(&typeNameNode)
                                    ? ""
                                    : libadalang::getName(&typeNameNode);

    if (universalType == "universal_int_type_") {
      ada_big_integer bigint;
      if (!ada_expr_p_eval_as_int(&staticExpr, &bigint)) {
        mlir::emitError(loc(typeContext),
                        "failed to evaluate static integer expression");
        return nullptr;
      }
      ada_text text;
      ada_big_integer_text(bigint, &text);
      std::string s = libadalang::textToString(text);
      ada_big_integer_decref(bigint);
      errno = 0;
      char *end;
      int64_t value = static_cast<int64_t>(std::strtoll(s.c_str(), &end, 10));
      if (end == s.c_str() || errno == ERANGE) {
        mlir::emitError(loc(typeContext),
                        "static integer value out of range for i64");
        return nullptr;
      }
      ada_node typDecl = resolveLiteralType(typeContext, loc(typeContext),
                                            "universal_int_type_");
      if (ada_node_is_null(&typDecl))
        return nullptr;
      mlir::Type type = getMLIRTypeFromDecl(typDecl, loc(typeContext));
      if (!type)
        return nullptr;
      return emitIntConstant(value, mlir::cast<mlir::IntegerType>(type),
                             loc(typeContext));
    }

    if (universalType == "universal_real_type_") {
      switch (ada_node_kind(&staticExpr)) {
      case ada_real_literal: {
        auto value = evalRealLiteral(staticExpr);
        if (!value)
          return nullptr;
        ada_node typDecl = resolveLiteralType(typeContext, loc(typeContext),
                                              "universal_real_type_");
        if (ada_node_is_null(&typDecl))
          return nullptr;
        mlir::Type type = getMLIRTypeFromDecl(typDecl, loc(typeContext));
        if (!type)
          return nullptr;
        return emitRealConstant(*value, mlir::cast<mlir::FloatType>(type),
                                loc(typeContext));
      }
      case ada_bin_op: {
        mlir::emitWarning(loc(staticExpr),
                          "libadalang has no `eval_as_real`; real expression "
                          "emitted as-is and expected to be folded by LLVM");
        ada_node left, right, op;
        ada_bin_op_f_left(&staticExpr, &left);
        ada_bin_op_f_right(&staticExpr, &right);
        ada_bin_op_f_op(&staticExpr, &op);
        mlir::Value lhs = visit_static_expr(left, typeContext);
        if (!lhs)
          return nullptr;
        mlir::Value rhs = visit_static_expr(right, typeContext);
        if (!rhs)
          return nullptr;
        return emitBinOp(op, lhs, rhs);
      }
      default:
        mlir::emitError(loc(staticExpr), "unsupported real static expression");
        return nullptr;
      }
    }

    mlir::emitError(loc(staticExpr), "unsupported static expression type '")
        << universalType << "'";
    return nullptr;
  }

  /// Codegen an expression node. Returns the SSA Value for the result, or
  /// nullptr on failure (unsupported expression kind or codegen error).
  mlir::Value visit_expr(ada_node &expr) {
    switch (ada_node_kind(&expr)) {
    case ada_identifier: {
      ada_node def_name;
      if (ada_name_p_referenced_defining_name(&expr, /*imprecise_fallback=*/0,
                                              &def_name) &&
          !ada_node_is_null(&def_name)) {
        auto it = numberDeclExprs.find(def_name.node);
        if (it != numberDeclExprs.end())
          return visit_static_expr(it->second, expr);
      }
      return mlirGenVariable(expr);
    }
    case ada_int_literal:
      return mlirGenIntLiteral(expr);
    case ada_real_literal:
      return mlirGenRealLiteral(expr);
    case ada_bin_op:
      return mlirGenBinOp(expr);
    case ada_call_expr: {
      auto callOp = mlirGenCallExpr(expr);
      if (!callOp)
        return nullptr;
      if (callOp.getNumResults() == 0) {
        mlir::emitError(loc(expr), "procedure called in expression context");
        return nullptr;
      }
      return callOp->getResult(0);
    }
    default:
      mlir::emitError(loc(expr), "unsupported expression: ")
          << libadalang::image(&expr);
    }

    return nullptr;
  }

  /// Emit a named number declaration (RM 3.3.2).
  ///
  /// Syntax:
  /// @code{.txt}
  /// number_declaration ::=
  ///      defining_identifier_list : constant := static_expression;
  /// @endcode
  ///
  /// **Static Semantics**: The named number denotes a value of type
  /// `universal_integer` if the type of the static_expression is an integer
  /// type. The named number denotes a value of type `universal_real` if the
  /// type of the static_expression is a real type. The value denoted by the
  /// named number is the value of the static_expression, converted to the
  /// corresponding universal type.
  ///
  /// **Legality Rules**: The static_expression shall be a static expression and
  /// is evaluated at compile time.
  ///
  /// @todo Named numbers can be used with (non-numeric) types that define
  ///       user-defined literals (Ada 2012).
  ///
  /// **Implementation Details**: Rather than evaluating the expression now (the
  /// universal type has no concrete annotation to resolve to), we stash the
  /// expression node in `numberDeclExprs` keyed by each `DefiningName`. The
  /// `arith.constant` is emitted lazily in `visit_static_expr` when the name is
  /// first used, at which point the use-site context provides the concrete MLIR
  /// type via `p_expected_expression_type`.
  llvm::LogicalResult mlirGenNumberDecl(ada_node &number_decl) {
    ada_node expr;
    ada_number_decl_f_expr(&number_decl, &expr);

    ada_node ids;
    ada_number_decl_f_ids(&number_decl, &ids);
    unsigned count = ada_node_children_count(&ids);
    for (unsigned i = 0; i < count; ++i) {
      ada_node id;
      if (ada_node_child(&ids, i, &id) == 0) {
        mlir::emitError(loc(number_decl), "failed to get declared identifier");
        return mlir::failure();
      }
      numberDeclExprs[id.node] = expr;
    }
    return mlir::success();
  }

  /// Emit an object declaration (RM 3.3.1).
  ///
  /// Syntax:
  /// @code{.txt}
  /// object_declaration ::=
  ///     defining_identifier_list : [aliased] [constant] subtype_indication
  ///         [:= expression] [aspect_specification];
  ///   | defining_identifier_list : [aliased] [constant] access_definition
  ///         [:= expression] [aspect_specification];
  ///   | defining_identifier_list : [aliased] [constant] array_type_definition
  ///         [:= expression] [aspect_specification];
  ///   | single_task_declaration
  ///   | single_protected_declaration
  ///
  /// defining_identifier_list ::=
  ///   defining_identifier {, defining_identifier}
  /// @endcode
  ///
  /// **Legality Rules**: An `object_declaration` without the reserved word
  /// `constant` declares a variable object. If it has a `subtype_indication` or
  /// an `array_type_definition` that defines an indefinite subtype, then there
  /// shall be an initialization expression.
  ///
  /// **Static Semantics**: An `object_declaration` with the reserved word
  /// `constant` declares a constant object. If it has an initialization
  /// expression, then it is called a full constant declaration. Otherwise, it
  /// is called a deferred constant declaration. The rules for deferred constant
  /// declarations are given in 7.4. The rules for full constant declarations
  /// are given in this subclause.
  ///
  /// Any declaration that includes a `defining_identifier_list` with more than
  /// one `defining_identifier` is equivalent to a series of declarations each
  /// containing one `defining_identifier` from the list, with the rest of the
  /// text of the declaration copied for each declaration in the series, in the
  /// same order as the list.
  ///
  /// The `subtype_indication`, `access_definition`, or full type definition of
  /// an `object_declaration` defines the nominal subtype of the object. The
  /// `object_declaration` declares an object of the type of the nominal
  /// subtype.
  ///
  /// **Implementation details**:
  ///
  /// @attention Only the first grammar form (with `subtype_indication`) is
  ///            supported, and only for scalar types. The `subtype_indication`
  ///            is not consulted for the object's type; the MLIR type is
  ///            inferred entirely from the initializer expression via
  ///            Libadalang's `p_expected_expression_type`. The
  ///            `access_definition`, `array_type_definition`,
  ///            `single_task_declaration`, and `single_protected_declaration`
  ///            forms are not handled.
  ///
  /// @warning Declarations without an initializer are silently skipped: the
  ///          variable will not be in the symbol table; a later reference will
  ///          produce an "unknown variable" error.
  ///
  /// @todo Support uninitialized scalar variables: requires switching from the
  ///       current pure-SSA model to an alloca-based model (stack slot per
  ///       variable via `llvm.alloca`, reads via `llvm.load`, writes via
  ///       `llvm.store`); the same strategy used by clang and GNAT for stack
  ///       locals before mem2reg promotes them to SSA values.
  ///
  /// @todo Typed constants (`ada_object_decl` with `ada_constant_present`) are
  ///       currently treated as mutable variables; the `constant` keyword is
  ///       ignored.
  llvm::LogicalResult mlirGenObjectDecl(ada_node &object_decl) {
    ada_node default_expr;
    ada_object_decl_f_default_expr(&object_decl, &default_expr);

    if (ada_node_is_null(&default_expr))
      return mlir::success();

    mlir::Value init = visit_expr(default_expr);
    if (!init)
      return mlir::failure();

    ada_node ids;
    ada_object_decl_f_ids(&object_decl, &ids);
    unsigned count = ada_node_children_count(&ids);
    for (unsigned i = 0; i < count; ++i) {
      ada_node id;
      if (ada_node_child(&ids, i, &id) == 0) {
        mlir::emitError(loc(object_decl), "failed to get declared identifier");
        return mlir::failure();
      }
      declare(id, init);
    }
    return mlir::success();
  }

  /// Emit declarations from a subprogram's declarative part.
  /// Supported: ObjectDecl (initialized only), SubpBody (nested subprograms),
  ///            NumberDecl (expression stashed for lazy use-site emission).
  /// Silently skipped: SubpDecl (forward declarations), and everything else.
  /// The AST structure is: DeclarativePart → AdaNodeList → decl...
  llvm::LogicalResult mlirGenDeclarativePart(ada_node &decls) {
    unsigned listCount = ada_node_children_count(&decls);
    for (unsigned i = 0; i < listCount; ++i) {
      ada_node list;
      if (ada_node_child(&decls, i, &list) == 0) {
        mlir::emitError(loc(decls), "failed to get declarative list");
        return mlir::failure();
      }
      unsigned count = ada_node_children_count(&list);
      for (unsigned j = 0; j < count; ++j) {
        ada_node decl;
        if (ada_node_child(&list, j, &decl) == 0) {
          mlir::emitError(loc(decls), "failed to get declaration");
          return mlir::failure();
        }
        switch (ada_node_kind(&decl)) {
        case ada_number_decl:
          if (mlir::failed(mlirGenNumberDecl(decl)))
            return mlir::failure();
          break;
        case ada_object_decl:
          if (mlir::failed(mlirGenObjectDecl(decl)))
            return mlir::failure();
          break;
        case ada_subp_body: {
          // Save and restore the insertion point: mlirGenSubpBody moves it to
          // the nested function's entry block, which would corrupt the
          // enclosing function's emit position.
          mlir::OpBuilder::InsertionGuard guard(builder);
          if (!mlirGenSubpBody(decl))
            return mlir::failure();
          break;
        }
        default:
          break;
        }
      }
    }
    return mlir::success();
  }

  /// Lower one Ada subprogram body to an `ada.subp` operation.
  /// This is the main codegen entry point for a subprogram: it creates the
  /// `ada.subp` op, binds argument SSA values in the symbol table, then walks
  /// the statement list to emit the body.
  mlir::Operation *mlirGenSubpBody(ada_node &subp_body) {
    ada_node ada_subp_spec;
    ada_base_subp_body_f_subp_spec(&subp_body, &ada_subp_spec);

    ada_node ret_type_expr;
    ada_subp_spec_f_subp_returns(&ada_subp_spec, &ret_type_expr);
    bool isProc = ada_node_is_null(&ret_type_expr);

    mlir::Operation *op = mlirGenSubpSpec(ada_subp_spec, isProc);
    if (!op)
      return nullptr;
    mlir::Block *entryBlock = &op->getRegion(0).front();

    std::vector<ada_node> args_v;

    ada_node_array params;
    ada_node ids;
    ada_base_subp_spec_p_params(&ada_subp_spec, &params);

    for (int i = 0; i < params->n; i++) {
      ada_param_spec_f_ids(&params->items[i], &ids);
      for (unsigned int j = 0; j < ada_node_children_count(&ids); j++) {
        ada_node child;
        if (ada_node_child(&ids, j, &child) == 0) {
          ada_node_array_dec_ref(params);
          mlir::emitError(loc(ids), "failed to get parameter identifier");
          return nullptr;
        }
        args_v.push_back(child);
      }
    }
    ada_node_array_dec_ref(params);

    // Declare all the function arguments in the symbol table.
    // C++17 structured bindings unpack each zip pair into named variables.
    for (auto [p, arg] : llvm::zip(args_v, entryBlock->getArguments())) {
      declare(p, arg);
      arg.setLoc(mlir::NameLoc::get(
          builder.getStringAttr(libadalang::getName(&p, false)), loc(p)));
    }

    builder.setInsertionPointToStart(entryBlock);

    ada_node decls;
    ada_subp_body_f_decls(&subp_body, &decls);
    if (!ada_node_is_null(&decls))
      if (mlir::failed(mlirGenDeclarativePart(decls)))
        return nullptr;

    ada_node stmts;
    ada_subp_body_f_stmts(&subp_body, &stmts);

    {
      if (mlir::failed(visit(stmts))) {
        op->erase();
        return nullptr;
      }
    }

    // Procedures have no explicit return statement; add an implicit one.
    if (isProc)
      builder.create<mlir::ada::ReturnOp>(loc(subp_body),
                                          ArrayRef<mlir::Value>{});

    return op;
  }

  /// Lower an Ada block statement (ada_begin_block or ada_decl_block) to an
  /// ada.block_stmt op. The block's declarative part (if any) and statements
  /// are emitted into the op's region; a new symbol table scope is opened for
  /// the duration so that local declarations are invisible outside the block.
  mlir::LogicalResult mlirGenBlockStmt(ada_node &blockNode,
                                       llvm::StringRef name) {
    bool isDecl = ada_node_kind(&blockNode) == ada_decl_block;

    mlir::StringAttr nameAttr =
        name.empty() ? mlir::StringAttr{}
                     : mlir::StringAttr::get(builder.getContext(), name);
    auto blockOp =
        builder.create<mlir::ada::BlockStmtOp>(loc(blockNode), nameAttr);

    // Create the entry block of the region; the builder now inserts into it.
    builder.createBlock(&blockOp.getBody());

    // Codegen the declarative part (ada_decl_block only).
    if (isDecl) {
      ada_node decls;
      ada_decl_block_f_decls(&blockNode, &decls);
      if (!ada_node_is_null(&decls))
        if (mlir::failed(mlirGenDeclarativePart(decls)))
          return mlir::failure();
    }

    // Codegen the statement sequence.
    ada_node stmts;
    if (isDecl)
      ada_decl_block_f_stmts(&blockNode, &stmts);
    else
      ada_begin_block_f_stmts(&blockNode, &stmts);
    if (mlir::failed(visit(stmts)))
      return mlir::failure();

    // Restore the insertion point to after the block_stmt in the parent block.
    builder.setInsertionPointAfter(blockOp);

    return mlir::success();
  }

  /// Create an ada.subp with the signature derived from the Ada subprogram
  /// spec. Returns nullptr on failure.
  mlir::Operation *mlirGenSubpSpec(ada_node &subp_spec, bool isProc) {
    auto location = loc(libadalang::parent(&subp_spec));

    ada_node name;
    ada_subp_spec_f_subp_name(&subp_spec, &name);

    ada_node_array params;
    ada_node ids;
    ada_base_subp_spec_p_params(&subp_spec, &params);

    llvm::SmallVector<mlir::Type, 4> argTypes;
    for (int i = 0; i < params->n; i++) {
      ada_node type_expr;
      ada_param_spec_f_type_expr(&params->items[i], &type_expr);
      mlir::Type paramType = getMLIRType(type_expr);
      ada_param_spec_f_ids(&params->items[i], &ids);
      for (unsigned j = 0; j < ada_node_children_count(&ids); j++)
        argTypes.push_back(paramType);
    }
    ada_node_array_dec_ref(params);

    llvm::SmallVector<mlir::Type, 1> retTypes;
    if (!isProc) {
      ada_node ret_type_expr;
      ada_subp_spec_f_subp_returns(&subp_spec, &ret_type_expr);
      if (ada_node_is_null(&ret_type_expr)) {
        mlir::emitError(location, "function has no return type");
        return nullptr;
      }
      mlir::Type retType = getMLIRType(ret_type_expr);
      if (!retType)
        return nullptr;
      retTypes.push_back(retType);
    }
    auto funcType = builder.getFunctionType(argTypes, retTypes);
    return builder.create<mlir::ada::SubpOp>(
        location, libadalang::getName(&name).data(), funcType);
  }

  /// Emit an assignment statement. In SSA form this rebinds the name to the
  /// new value.
  ///
  /// Emit a procedure call statement. Only simple identifier calls (no
  /// arguments) are supported for now. The callee is looked up first in the
  /// enclosing ada.subp's SymbolTable (for nested subprograms), then
  /// in the module-level SymbolTable (for top-level subprograms).
  llvm::LogicalResult mlirGenCallStmt(ada_node &call_stmt) {
    ada_node call;
    ada_call_stmt_f_call(&call_stmt, &call);
    return mlirGenCallExpr(call) ? mlir::success() : mlir::failure();
  }

  /// Known limitation: `in out` parameter write-back is not implemented.
  /// The new value is stored in the local symbol table only; it is never
  /// written back to the caller's variable. Any code relying on `in out`
  /// semantics will silently produce wrong results.
  llvm::LogicalResult mlirGenAssign(ada_node &assign_stmt) {
    ada_node dest_node, expr_node;
    ada_assign_stmt_f_dest(&assign_stmt, &dest_node);
    ada_assign_stmt_f_expr(&assign_stmt, &expr_node);

    mlir::Value rhs = visit_expr(expr_node);
    if (!rhs)
      return mlir::failure();

    if (ada_node_kind(&dest_node) != ada_identifier) {
      mlir::emitError(loc(assign_stmt), "unsupported assignment destination");
      return mlir::failure();
    }

    ada_node def_name;
    if (!ada_name_p_referenced_defining_name(
            &dest_node, /*imprecise_fallback=*/0, &def_name) ||
        ada_node_is_null(&def_name)) {
      mlir::emitError(loc(dest_node), "unknown variable '")
          << libadalang::getName(&dest_node, false) << "'";
      return mlir::failure();
    }

    nodeValues[def_name.node] = rhs;
    return mlir::success();
  }

  /// Emit a return operation. This will return failure if any generation fails.
  llvm::LogicalResult mlirGenReturn(ada_node &return_stmt) {
    auto location = loc(return_stmt);

    ada_node return_expr;
    ada_return_stmt_f_return_expr(&return_stmt, &return_expr);

    // In Ada, a procedure return carries no value; only function returns do.
    mlir::Value expr = nullptr;
    if (!ada_node_is_null(&return_expr)) {
      expr = visit_expr(return_expr);
      if (!expr)
        return mlir::failure();
    }

    builder.create<mlir::ada::ReturnOp>(
        location, expr ? ArrayRef(expr) : ArrayRef<mlir::Value>());
    return mlir::success();
  }

  /// Map a type declaration node to an MLIR type. Follows the subtype chain
  /// to the canonical base type, then matches its name against known Ada types.
  /// diagLoc is used only for the "unsupported type" warning.
  mlir::Type getMLIRTypeFromDecl(ada_node &type_decl, mlir::Location diagLoc) {
    // Follow the subtype chain to the canonical (base) type so that subtypes
    // of Integer map to the same MLIR type as Integer itself.
    // TODO: nullptr would be the correct origin but crashes with a
    // CONSTRAINT_ERROR in libadalang-implementation-c.adb; using self for now.
    ada_node canon_type;
    if (!ada_base_type_decl_p_canonical_type(&type_decl, &type_decl,
                                             &canon_type) ||
        ada_node_is_null(&canon_type))
      canon_type = type_decl;

    // f_name gives the defining identifier of the type declaration, whose
    // lower-cased text we use to drive the mapping below.
    ada_node type_name;
    if (!ada_base_type_decl_f_name(&canon_type, &type_name) ||
        ada_node_is_null(&type_name)) {
      mlir::emitError(diagLoc, "failed to get name of type declaration");
      return {};
    }

    std::string name = libadalang::getName(&type_name);
    if (name == "integer")
      return builder.getI32Type();
    if (name == "long_integer")
      return builder.getI64Type();
    if (name == "short_integer")
      return builder.getIntegerType(16);
    if (name == "float")
      return builder.getF32Type();
    if (name == "long_float")
      return builder.getF64Type();

    mlir::emitError(diagLoc, "unsupported Ada type '") << name << "'";
    return {};
  }

  /// Resolve an Ada type expression (e.g. a SubtypeIndication node like
  /// "Long_Integer") to the corresponding MLIR type. Returns a null type
  /// and emits an error on failure.
  mlir::Type getMLIRType(ada_node &type_expr) {
    // p_designated_type_decl resolves a type expression to its declaration.
    // This works on SubtypeIndication nodes (parameter / return types), unlike
    // p_expression_type which only works on value expressions.
    ada_node type_decl;
    if (!ada_type_expr_p_designated_type_decl(&type_expr, &type_decl) ||
        ada_node_is_null(&type_decl)) {
      mlir::emitError(loc(type_expr), "failed to resolve type expression");
      return {};
    }

    return getMLIRTypeFromDecl(type_decl, loc(type_expr));
  }
};

} // namespace

namespace lalvm {

mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &compilationUnit) {
  return MLIRGenImpl(context).mlirGen(compilationUnit);
}

} // namespace lalvm
