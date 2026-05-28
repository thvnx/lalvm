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
#include "mlir/Dialect/MemRef/IR/MemRef.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/Path.h"

namespace libadalang = frontend::libadalang;

#define DEBUG_TYPE "ada-mlirgen"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <optional>

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

namespace {

/// DenseMapInfo for ada_node: identity is the raw node pointer; entity info
/// (rebindings) is ignored for map purposes.
struct AdaNodeDenseMapInfo {
  static ada_node getEmptyKey() {
    return {
        static_cast<ada_base_node>(llvm::DenseMapInfo<void *>::getEmptyKey()),
        {}};
  }
  static ada_node getTombstoneKey() {
    return {static_cast<ada_base_node>(
                llvm::DenseMapInfo<void *>::getTombstoneKey()),
            {}};
  }
  static unsigned getHashValue(const ada_node &n) {
    return llvm::DenseMapInfo<ada_base_node>::getHashValue(n.node);
  }
  static bool isEqual(const ada_node &a, const ada_node &b) {
    return a.node == b.node;
  }
};

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
    char *filename = ada_unit_filename(ada_node_unit(&compilationUnit));
    llvm::StringRef stem = llvm::sys::path::stem(filename);
    adaModule = mlir::ModuleOp::create(loc(compilationUnit), stem);
    free(filename);

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

  /// Insertion-point cursor: tracks where the next MLIR operation is emitted.
  mlir::OpBuilder builder;

  mlir::StringAttr getNameAttr(ada_node *node) {
    return builder.getStringAttr(libadalang::getName(node));
  }

  /// Return `loc` wrapped in a FusedLoc carrying `typeOp`'s symbol as metadata.
  /// The info survives MLIR conversion since locations are preserved verbatim.
  mlir::Location makeAdaTypeLoc(mlir::Location loc, mlir::ada::TypeOp typeOp) {
    auto *ctx = loc.getContext();
    auto typeRef = mlir::FlatSymbolRefAttr::get(ctx, typeOp.getSymName());
    return mlir::FusedLoc::get(ctx, {loc}, typeRef);
  }

  /// Wrap `op`'s location in a FusedLoc carrying `typeOp`'s symbol as metadata.
  /// The info survives MLIR conversion since locations are preserved.
  void setAdaTypeLoc(mlir::Operation *op, mlir::ada::TypeOp typeOp) {
    op->setLoc(makeAdaTypeLoc(op->getLoc(), typeOp));
  }

  /// Attach an Ada source name and type reference to `op`. Encodes the type
  /// reference as FusedLoc metadata in the NameLoc inner location so the info
  /// survives MLIR conversion patterns.
  void setAdaNameLoc(mlir::Operation *op, mlir::StringAttr name,
                     mlir::ada::TypeOp typeOp) {
    op->setLoc(mlir::NameLoc::get(name, makeAdaTypeLoc(op->getLoc(), typeOp)));
  }

  /// Return the location to use for a load/store on `ptr`, with `srcLoc` as the
  /// inner source location. Propagates the variable name and type-reference
  /// metadata from `ptr.getLoc()` if it is a NameLoc; otherwise returns
  /// `srcLoc` unchanged.
  mlir::Location propagateAdaNameLoc(mlir::Value ptr, mlir::Location srcLoc) {
    auto nameLoc = mlir::dyn_cast<mlir::NameLoc>(ptr.getLoc());
    if (!nameLoc)
      return srcLoc;
    auto fusedLoc = mlir::dyn_cast<mlir::FusedLoc>(nameLoc.getChildLoc());
    return mlir::NameLoc::get(
        nameLoc.getName(),
        fusedLoc ? mlir::FusedLoc::get(srcLoc.getContext(), {srcLoc},
                                       fusedLoc.getMetadata())
                 : srcLoc);
  }

  // Maps each DefiningName node to its SSA Value, keyed by ada_base_node
  // pointer (Libadalang's unique node identity). Node identity rather than name
  // strings ensures correct resolution under name shadowing.
  //
  // In the alloca model: ObjectDecl names map to their memref.alloca pointer
  // (memref<T>); `in` parameters map to their scalar block argument; `in
  // out`/`out` parameters map to a pointer to the caller's storage. Reads emit
  // memref.load, stores emit memref.store, and scalar parameters are returned
  // directly. No scope cleanup is needed: Libadalang rejects out-of-scope
  // references before MLIRGen runs.
  llvm::DenseMap<ada_base_node, mlir::Value> declValues;

  // Tracks alloca pointers for ObjectDecl variables declared without an
  // initializer that have not yet been written to. Used to detect reads before
  // first assignment. Erased on the first memref.store to the alloca.
  llvm::DenseSet<mlir::Value> uninitAllocas;

  // Named numbers (RM 3.3.2): maps each DefiningName node to the pre-evaluated
  // arith.constant (null when the expression could not be folded at declaration
  // time, e.g. composite real expressions). Use-site resolution re-emits the
  // constant in the concrete target type, or falls back to visit_static_expr
  // via the NumberDecl recovered from the key.
  llvm::DenseMap<ada_node, mlir::Value, AdaNodeDenseMapInfo> numberDecls;

  // Cache from type decl node to its emitted ada.type op. Populated by
  // mlirGenTypeDecl; queried via lookupOrEmitTypeOp (which also handles lazy
  // emission for predefined types such as Boolean) and getMLIRTypeFromDecl.
  llvm::DenseMap<ada_base_node, mlir::ada::TypeOp> typeDecls;

  /// Helper conversion for a Libadalang AST location to an MLIR location.
  mlir::Location loc(const ada_node &node) {
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

  /// Bind a DefiningName node to a Value in declValues.
  void declare(ada_node &def_name, mlir::Value value) {
    LLVM_DEBUG(llvm::dbgs()
               << "declare: " << libadalang::image(&def_name) << "\n");
    declValues[def_name.node] = value;
  }

  // Recursive AST walker. Handles the node kinds we know how to codegen;
  // everything else is ignored at this level and its children are visited.
  // Returning early (without visiting children) stops descent into a subtree
  // (used when a handler already walked it, e.g. mlirGenSubpBody visits stmts).
  mlir::LogicalResult visit(ada_node &node) {
    ada_bool isEntryPoint = 0;
    if (ada_ada_node_p_xref_entry_point(&node, &isEntryPoint) && isEntryPoint &&
        libadalang::emitSolverDiagnostics(&node))
      return mlir::failure();

    switch (ada_node_kind(&node)) {
    case ada_subp_body:
      // Top-level subprograms are emitted at module scope. The insertion point
      // is set here rather than inside mlirGenSubpBody so that nested
      // subprograms (processed via mlirGenDeclarativePart) are instead emitted
      // at the current insertion point inside the enclosing body region.
      builder.setInsertionPointToEnd(adaModule.getBody());
      if (!mlirGenSubpBody(node))
        return mlir::failure();
      return mlir::success();
    case ada_return_stmt:
      return mlirGenReturn(node);
    case ada_assign_stmt:
      if (mlir::failed(mlirGenAssign(node)))
        return mlir::failure();
      return mlir::success();
    case ada_null_stmt:
      builder.create<mlir::ada::NullOp>(loc(node));
      return mlir::success();
    case ada_call_stmt:
      return mlirGenCallStmt(node);
    case ada_named_stmt: {
      // Named block statement: "Name: [declare] begin ... end Name;"
      // The name lives on the wrapping named_stmt; the actual block is f_stmt.
      ada_node decl, nameNode, stmt;
      ada_named_stmt_f_decl(&node, &decl);
      ada_named_stmt_decl_f_name(&decl, &nameNode);
      ada_named_stmt_f_stmt(&node, &stmt);
      ada_text nameText;
      ada_node_text(&nameNode, &nameText);
      return mlirGenBlock(stmt, libadalang::textToString(nameText));
    }
    case ada_begin_block:
    case ada_decl_block:
      return mlirGenBlock(node, {});
    case ada_compilation_unit:
    case ada_ada_node_list:
    case ada_library_item:
    case ada_private_absent:
    case ada_private_present:
    case ada_pragma_node_list:
    case ada_handled_stmts:
    case ada_stmt_list:
      // Transparent nodes: visit children without warning.
      break;
    default: {
      // TODO: turn this into an Error when lalvm is mature enough.
      mlir::emitWarning(loc(node), "visit: unhandled node '")
          << libadalang::image(&node) << "'";
      break;
    }
    }

    unsigned i, count = ada_node_children_count(&node);
    for (i = 0; i < count; ++i) {
      ada_node child;
      if (ada_node_child(&node, i, &child) == 0) {
        mlir::emitError(loc(node), "failed to get child node");
        return mlir::failure();
      }
      if (!ada_node_is_null(&child) && mlir::failed(visit(child)))
        return mlir::failure();
    }
    return mlir::success();
  }

  /// Resolve a name expression to its bound Value via Libadalang
  /// cross-reference + declValues lookup. Returns a null Value (without
  /// emitting any diagnostic) if the name cannot be resolved or is not bound.
  mlir::Value findVarValue(ada_node &expr) {
    ada_node def_name;
    if (!ada_name_p_referenced_defining_name(&expr, /*imprecise_fallback=*/0,
                                             &def_name) ||
        ada_node_is_null(&def_name))
      return {};
    auto it = declValues.find(def_name.node);
    return it != declValues.end() ? it->second : mlir::Value{};
  }

  /// Return the alloca pointer (memref<T>) for a variable expression without
  /// emitting a load. Used to pass `in out` / `out` actual parameters by
  /// reference. Ada requires the actual for such a formal to be a variable
  /// (RM 6.4.1), so the resolved value is always a memref.
  mlir::Value resolveVarPtr(ada_node &expr) {
    mlir::Value val = findVarValue(expr);
    if (!val || !mlir::isa<mlir::MemRefType>(val.getType())) {
      mlir::emitError(loc(expr),
                      "actual for `in out`/`out` parameter must be a variable");
      return nullptr;
    }
    return val;
  }

  /// Emit a variable reference. Resolves via Libadalang cross-reference to
  /// the unique DefiningName node, then looks up the bound value.
  /// For ObjectDecl variables (alloca-backed), emits a memref.load.
  /// For `in`/default-in parameters (direct SSA values), returns the value
  /// directly.
  mlir::Value mlirGenVariable(ada_node &expr) {
    if (mlir::Value val = findVarValue(expr)) {
      if (auto memrefTy = mlir::dyn_cast<mlir::MemRefType>(val.getType())) {
        if (uninitAllocas.contains(val))
          mlir::emitWarning(loc(expr), "variable '")
              << libadalang::getName(&expr, false)
              << "' is read before first assignment";
        return builder.create<mlir::memref::LoadOp>(
            propagateAdaNameLoc(val, loc(expr)), memrefTy.getElementType(),
            val);
      }
      return val; // direct SSA value (in/default-in parameter)
    }

    // Slow path: distinguish error kinds for better diagnostics.
    ada_node ref_decl;
    if (ada_name_p_referenced_decl(&expr, /*imprecise_fallback=*/0,
                                   &ref_decl) &&
        !ada_node_is_null(&ref_decl)) {
      if (ada_node_kind(&ref_decl) != ada_object_decl) {
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
    auto callerLoc = loc(op);
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
    case ada_op_div:
      kind = mlir::ada::AdaBinaryOp::Div;
      break;
    default:
      mlir::emitError(callerLoc, "invalid binary operator '")
          << libadalang::image(&op) << "'";
      return nullptr;
    }
    return builder.create<mlir::ada::BinOp>(callerLoc, kind, lhs, rhs);
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
    auto val = emitBinOp(op, lhs, rhs);
    if (val) {
      ada_node type_decl{};
      if (ada_expr_p_expression_type(&binop, &type_decl))
        if (auto typeOp = lookupOrEmitTypeOp(type_decl, loc(binop)))
          setAdaTypeLoc(val.getDefiningOp(), typeOp);
    }
    return val;
  }

  /// Resolve the type of a literal expression. For universal types
  /// (universal_int_type_ / universal_real_type_), falls back to the expected
  /// type from the surrounding context. Returns a null node on failure.
  ada_node resolveLiteralType(ada_node &node, mlir::Location location) {
    ada_node type_decl;
    if (!ada_expr_p_expression_type(&node, &type_decl) ||
        ada_node_is_null(&type_decl)) {
      mlir::emitError(location, "failed to resolve type of literal");
      return {};
    }
    if (libadalang::isUniversalTypeDecl(type_decl)) {
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
    if (width == 1) {
      if (value < 0 || value > 1) {
        mlir::emitError(location, "integer constant ")
            << value << " out of range for i1";
        return nullptr;
      }
    } else if (width < 64) {
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
    std::string literal = libadalang::bigIntToString(bigint);
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
    // p_expression_type on an integer literal returns universal_integer,
    // not the concrete type. p_expected_expression_type gives the type
    // required by the surrounding context (e.g. the return type of the
    // enclosing function).
    auto location = loc(node);
    ada_node type_decl = resolveLiteralType(node, location);
    if (ada_node_is_null(&type_decl))
      return nullptr;
    mlir::Type type = getMLIRTypeFromDecl(type_decl, location);
    if (!type)
      return nullptr;
    auto val =
        emitIntConstant(*value, mlir::cast<mlir::IntegerType>(type), location);
    if (val)
      if (auto typeOp = lookupOrEmitTypeOp(type_decl, location))
        setAdaTypeLoc(val.getDefiningOp(), typeOp);
    return val;
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
    auto location = loc(node);
    ada_node type_decl = resolveLiteralType(node, location);
    if (ada_node_is_null(&type_decl))
      return nullptr;
    mlir::Type type = getMLIRTypeFromDecl(type_decl, location);
    if (!type)
      return nullptr;
    auto val =
        emitRealConstant(*value, mlir::cast<mlir::FloatType>(type), location);
    if (val)
      if (auto typeOp = lookupOrEmitTypeOp(type_decl, location))
        setAdaTypeLoc(val.getDefiningOp(), typeOp);
    return val;
  }

  /// Return the `ada.type` op for a type declaration, emitting it lazily at
  /// module level if not yet present in `typeDecls` (used for predefined and
  /// external types such as `Boolean`).
  ///
  /// @param type_decl  A `BaseTypeDecl` LAL node. If null or not a type decl,
  ///                   returns null without emitting a diagnostic.
  /// @param location   MLIR location used for any diagnostic emitted during
  ///                   lazy emission.
  /// @return           The `ada.type` op, or null if the type is unsupported or
  ///                   an error occurred during emission.
  mlir::ada::TypeOp lookupOrEmitTypeOp(ada_node &type_decl,
                                       mlir::Location location) {
    if (ada_node_is_null(&type_decl) || !libadalang::isBaseTypeDecl(type_decl))
      return {};
    auto it = typeDecls.find(type_decl.node);
    if (it == typeDecls.end()) {
      mlir::OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPointToStart(adaModule.getBody());
      if (mlir::failed(mlirGenTypeDecl(type_decl, /*qualifiedName=*/true)))
        return {};
      it = typeDecls.find(type_decl.node);
      if (it == typeDecls.end()) {
        mlir::emitError(location, "ada.type not found for type");
        return {};
      }
    }
    return it->second;
  }

  /// Emit an enum literal as its integer representation (Ada RM 13.4).
  /// The rep value and MLIR type are read from the `ada.type` op metadata;
  /// Libadalang is only consulted for the literal name and the enclosing type.
  ///
  /// @param enumLitDecl `EnumLiteralDecl` for the literal; supplies the
  ///                    canonical name used for the `NameLoc` and for the
  ///                    lookup in the `ada.type` op metadata.
  /// @param useExpr     Use-site expression node; provides the source location.
  ///
  /// @todo Support equality and relational operators on enumeration values.
  /// @todo Support enumeration attributes ('Pos, 'Val, 'Succ, 'Pred, 'Image,
  ///       'Value).
  /// @todo Support Boolean logical operators (and, or, xor, not, and then,
  ///       or else).
  mlir::Value mlirGenEnumLit(ada_node &enumLitDecl, ada_node &useExpr) {
    auto location = loc(useExpr);

    // Get the literal's canonical name for the NameLoc.
    ada_node lit_name_node;
    if (!ada_enum_literal_decl_f_name(&enumLitDecl, &lit_name_node) ||
        ada_node_is_null(&lit_name_node)) {
      mlir::emitError(location, "failed to get enum literal name");
      return nullptr;
    }
    std::string litName =
        libadalang::getName(&lit_name_node, /*canonical=*/true);

    ada_node type_decl;
    if (!ada_enum_literal_decl_p_enum_type(&enumLitDecl, &type_decl) ||
        ada_node_is_null(&type_decl)) {
      mlir::emitError(location, "failed to resolve type of enum literal");
      return nullptr;
    }

    mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(type_decl, location);
    if (!typeOp)
      return nullptr;

    // Derive the rep value and MLIR type from the ada.type op metadata rather
    // than querying Libadalang again: ada.type is the single source of truth
    // for enum type information within the pipeline.
    auto typeInfo =
        mlir::cast<mlir::ada::EnumTypeInfoAttr>(typeOp.getTypeInfo());
    auto value = typeInfo.enumRep(litName);
    if (!value) {
      mlir::emitError(location, "enum literal '")
          << litName << "' not found in ada.type '" << typeOp.getSymName()
          << "'";
      return nullptr;
    }
    mlir::Type type = typeOp.getMlirType();

    auto attr =
        mlir::IntegerAttr::get(mlir::cast<mlir::IntegerType>(type), *value);
    auto constOp = builder.create<mlir::arith::ConstantOp>(location, attr);
    setAdaNameLoc(constOp, builder.getStringAttr(litName), typeOp);
    return constOp.getResult();
  }

  /// Emit a subprogram call from an ada_identifier (no-arg) or ada_call_expr
  /// (with args) node. Returns the CallOp on success, nullptr on error.
  /// For function calls the op has one result; for procedure calls none.
  mlir::ada::CallOp mlirGenCallExpr(ada_node &call) {
    auto location = loc(call);

    ada_node name_node;
    ada_node suffix{};

    switch (ada_node_kind(&call)) {
    case ada_identifier:
      name_node = call;
      break;
    case ada_call_expr:
      ada_call_expr_f_name(&call, &name_node);
      ada_call_expr_f_suffix(&call, &suffix);
      break;
    default:
      mlir::emitError(location, "unsupported call expression");
      return nullptr;
    }

    auto calleeName = libadalang::getName(&name_node);

    mlir::Operation *from =
        builder.getInsertionBlock()->getParent()->getParentOp();
    mlir::Operation *calleeOp =
        mlir::ada::CallOp::lookupCallee(from, calleeName);

    if (!calleeOp || !isa<mlir::ada::SubpOp>(calleeOp)) {
      mlir::emitError(location, "unknown subprogram '") << calleeName << "'";
      return nullptr;
    }

    auto calleeSubp = mlir::cast<mlir::ada::SubpOp>(calleeOp);

    // Evaluate arguments. Formals with a memref type (in out / out) receive the
    // caller's alloca pointer directly; scalar formals (in) receive a value.
    auto funcTy = calleeSubp.getFunctionType();
    llvm::SmallVector<mlir::Value> args;
    if (!ada_node_is_null(&suffix)) {
      auto formalTypes = funcTy.getInputs();
      int n = ada_node_children_count(&suffix);
      for (int i = 0; i < n; ++i) {
        ada_node assoc, r_expr;
        ada_node_child(&suffix, i, &assoc);
        ada_param_assoc_f_r_expr(&assoc, &r_expr);
        if (mlir::isa<mlir::MemRefType>(formalTypes[i])) {
          mlir::Value ptr = resolveVarPtr(r_expr);
          if (!ptr)
            return nullptr;
          // After the call the variable is considered initialized: `out`
          // formals are contractually written by the callee (RM 6.4.1).
          uninitAllocas.erase(ptr);
          args.push_back(ptr);
        } else {
          mlir::Value val = visit_expr(r_expr);
          if (!val)
            return nullptr;
          args.push_back(val);
        }
      }
    }

    auto calleeRef =
        mlir::FlatSymbolRefAttr::get(builder.getContext(), calleeName);

    if (calleeSubp.isFunction()) {
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
      auto callOp =
          builder.create<mlir::ada::CallOp>(location, calleeRef, retType, args);
      if (auto typeOp = lookupOrEmitTypeOp(type_decl, location))
        setAdaTypeLoc(callOp, typeOp);
      return callOp;
    }

    return builder.create<mlir::ada::CallOp>(location, calleeRef, mlir::Type{},
                                             args);
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

    if (universalType == libadalang::kUniversalRealTypeName) {
      switch (ada_node_kind(&staticExpr)) {
      case ada_real_literal: {
        auto value = evalRealLiteral(staticExpr);
        if (!value)
          return nullptr;
        ada_node typDecl = resolveLiteralType(typeContext, loc(typeContext));
        if (ada_node_is_null(&typDecl))
          return nullptr;
        mlir::Type type = getMLIRTypeFromDecl(typDecl, loc(typeContext));
        if (!type)
          return nullptr;
        auto val = emitRealConstant(*value, mlir::cast<mlir::FloatType>(type),
                                    loc(typeContext));
        if (val)
          if (auto typeOp = lookupOrEmitTypeOp(typDecl, loc(typeContext)))
            setAdaTypeLoc(val.getDefiningOp(), typeOp);
        return val;
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
        auto it = numberDecls.find(def_name);
        if (it != numberDecls.end()) {
          if (mlir::Value value = it->second) {
            // Re-emit the pre-evaluated constant in the concrete type required
            // by the use-site context. The value is always a static constant so
            // we convert at compile time and range-check; no cast op is
            // emitted.
            mlir::Type srcType = value.getType();
            bool isIntKind = mlir::isa<mlir::IntegerType>(srcType);
            ada_node typDecl = resolveLiteralType(expr, loc(expr));
            if (ada_node_is_null(&typDecl))
              return nullptr;
            mlir::Type tgtType = getMLIRTypeFromDecl(typDecl, loc(expr));
            if (!tgtType)
              return nullptr;
            auto defOp = value.getDefiningOp<mlir::arith::ConstantOp>();
            mlir::Value useVal;
            if (isIntKind) {
              int64_t val =
                  mlir::cast<mlir::IntegerAttr>(defOp.getValue()).getInt();
              useVal = emitIntConstant(
                  val, mlir::cast<mlir::IntegerType>(tgtType), loc(expr));
            } else {
              double val = mlir::cast<mlir::FloatAttr>(defOp.getValue())
                               .getValueAsDouble();
              useVal = emitRealConstant(
                  val, mlir::cast<mlir::FloatType>(tgtType), loc(expr));
            }
            if (useVal)
              if (auto typeOp = lookupOrEmitTypeOp(typDecl, loc(expr)))
                setAdaTypeLoc(useVal.getDefiningOp(), typeOp);
            return useVal;
          }
          ada_node keyNode = it->first, numberDecl, fallbackExpr;
          ada_defining_name_p_basic_decl(&keyNode, &numberDecl);
          ada_number_decl_f_expr(&numberDecl, &fallbackExpr);
          return visit_static_expr(fallbackExpr, expr);
        }
      }
      // Enum literal: emit as its integer representation.
      ada_node ref_decl;
      if (ada_name_p_referenced_decl(&expr, /*imprecise_fallback=*/0,
                                     &ref_decl) &&
          !ada_node_is_null(&ref_decl) &&
          ada_node_kind(&ref_decl) == ada_enum_literal_decl)
        return mlirGenEnumLit(ref_decl, expr);
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
  /// **Implementation Details**: Each `DefiningName` is entered in
  /// `numberDecls` with an `arith.constant` SSA value when the expression can
  /// be folded at declaration time (null otherwise). Use-site resolution
  /// re-emits the pre-computed value in the concrete type required by context;
  /// for unevaluated cases it falls back to `visit_static_expr`, recovering
  /// the expression via `ada_defining_name_p_basic_decl` on the map key.
  llvm::LogicalResult mlirGenNumberDecl(ada_node &number_decl) {
    ada_node expr;
    ada_number_decl_f_expr(&number_decl, &expr);

    // Determine the universal type of the static expression.
    ada_node exprType;
    bool hasConstantValue = ada_expr_p_expression_type(&expr, &exprType) &&
                            !ada_node_is_null(&exprType);

    enum class UniversalKind { Unknown, Int, Real };
    UniversalKind kind = UniversalKind::Unknown;
    if (hasConstantValue) {
      ada_node typeNameNode;
      ada_base_type_decl_f_name(&exprType, &typeNameNode);
      if (!ada_node_is_null(&typeNameNode))
        kind =
            llvm::StringSwitch<UniversalKind>(
                libadalang::getName(&typeNameNode))
                .Case(libadalang::kUniversalIntTypeName, UniversalKind::Int)
                .Case(libadalang::kUniversalRealTypeName, UniversalKind::Real)
                .Default(UniversalKind::Unknown);
    }

    // Eager evaluation for DWARF metadata; the expression node is also stashed
    // for lazy use-site evaluation, which resolves the concrete type from
    // context.
    mlir::TypedAttr constAttr;
    mlir::ada::TypeOp typeOp;
    switch (kind) {
    case UniversalKind::Int: {
      ada_big_integer bigint;
      if (ada_expr_p_eval_as_int(&expr, &bigint)) {
        std::string s = libadalang::bigIntToString(bigint);
        errno = 0;
        char *end;
        int64_t value = static_cast<int64_t>(std::strtoll(s.c_str(), &end, 10));
        if (end != s.c_str() && errno != ERANGE) {
          typeOp = lookupOrEmitTypeOp(exprType, loc(number_decl));
          if (typeOp)
            constAttr = mlir::IntegerAttr::get(builder.getI64Type(), value);
        }
      }
      break;
    }
    case UniversalKind::Real: {
      if (ada_node_kind(&expr) == ada_real_literal) {
        auto value = evalRealLiteral(expr);
        if (value) {
          typeOp = lookupOrEmitTypeOp(exprType, loc(number_decl));
          if (typeOp)
            constAttr = mlir::FloatAttr::get(builder.getF64Type(), *value);
        }
      }
      break;
    }
    case UniversalKind::Unknown:
      mlir::emitError(loc(number_decl), "named number has unsupported type");
      return mlir::failure();
    }

    ada_node ids;
    ada_number_decl_f_ids(&number_decl, &ids);
    unsigned count = ada_node_children_count(&ids);
    for (unsigned i = 0; i < count; ++i) {
      ada_node id;
      if (ada_node_child(&ids, i, &id) == 0) {
        mlir::emitError(loc(number_decl), "failed to get declared identifier");
        return mlir::failure();
      }
      mlir::Value constValue;
      if (constAttr && typeOp) {
        auto nameAttr = getNameAttr(&id);
        auto constOp =
            builder.create<mlir::arith::ConstantOp>(loc(id), constAttr);
        setAdaNameLoc(constOp, nameAttr, typeOp);
        constValue = constOp.getResult();
      }
      numberDecls[id] = constValue;
    }
    return mlir::success();
  }

  /// Emit an ada.type op for an Ada type declaration (RM 3.1). Currently only
  /// enumeration types (RM 3.5.1) are handled; other kinds are silently skipped
  /// and will be added as support for each kind is implemented.
  ///
  /// When `qualifiedName` is true the symbol name uses the canonical fully
  /// qualified Ada name (e.g. `standard.boolean`), suitable for module-level
  /// ops where multiple packages might export types with the same simple name.
  /// Local types within a subprogram use the simple name (default).
  ///
  /// @todo IntegerTypeInfoAttr, FloatTypeInfoAttr, RecordTypeInfoAttr, etc.
  llvm::LogicalResult mlirGenTypeDecl(ada_node &type_decl,
                                      bool qualifiedName = false) {
    auto location = loc(type_decl);

    // Resolve the Ada type name: fully qualified for module-level types,
    // simple canonical name for locals.
    auto resolveTypeName = [&]() -> llvm::FailureOr<std::string> {
      if (qualifiedName) {
        ada_string_type fqn;
        if (!ada_basic_decl_p_canonical_fully_qualified_name(&type_decl,
                                                             &fqn)) {
          mlir::emitError(location, "failed to get fully qualified type name");
          return mlir::failure();
        }
        char *buf;
        size_t len;
        ada_string_to_utf8(fqn, &buf, &len);
        std::string name(buf, len);
        free(buf);
        ada_string_dec_ref(fqn);
        return name;
      }
      ada_node nameNode;
      if (!ada_base_type_decl_f_name(&type_decl, &nameNode) ||
          ada_node_is_null(&nameNode)) {
        mlir::emitError(location, "failed to get type name");
        return mlir::failure();
      }
      return libadalang::getName(&nameNode, /*canonical=*/true);
    };

    // Universal types (RM 3.4.1) and numeric types (RM 3.5.4, 3.5.6, 3.5.7).
    if (libadalang::isUniversalTypeDecl(type_decl) ||
        libadalang::isNumericTypeDecl(type_decl)) {
      auto typeName = resolveTypeName();
      if (mlir::failed(typeName))
        return mlir::failure();

      mlir::Type mlirType;
      uint64_t modulus = 0;
      ada_node type_def;
      if (ada_type_decl_f_type_def(&type_decl, &type_def) &&
          !ada_node_is_null(&type_def) &&
          ada_node_kind(&type_def) == ada_mod_int_type_def) {
        ada_node expr;
        ada_mod_int_type_def_f_expr(&type_def, &expr);
        ada_bool isStatic = false;
        if (!ada_expr_p_is_static_expr(&expr, /*imprecise_fallback=*/false,
                                       &isStatic) ||
            !isStatic)
          return mlir::emitError(
              location, "modular type modulus is not a static expression");
        ada_big_integer bigint;
        if (!ada_expr_p_eval_as_int(&expr, &bigint))
          return mlir::emitError(location,
                                 "failed to evaluate modular type modulus");
        std::string s = libadalang::bigIntToString(bigint);
        errno = 0;
        char *end;
        modulus = std::strtoull(s.c_str(), &end, 10);
        if (end == s.c_str())
          return mlir::emitError(location,
                                 "failed to parse modular type modulus '")
                 << s << "'";
        if (errno == ERANGE)
          return mlir::emitError(location,
                                 "modular type modulus out of range: ")
                 << s;
        unsigned width = modulus <= (1ULL << 8)    ? 8
                         : modulus <= (1ULL << 16) ? 16
                         : modulus <= (1ULL << 32) ? 32
                                                   : 64;
        mlirType = builder.getIntegerType(width);
      } else {
        mlirType = getMLIRTypeFromDecl(type_decl, location);
        if (!mlirType)
          return mlir::failure();
      }

      auto typeInfo =
          mlir::ada::NumericTypeInfoAttr::get(builder.getContext(), modulus);
      auto typeOp = builder.create<mlir::ada::TypeOp>(location, *typeName,
                                                      mlirType, typeInfo);
      typeDecls[type_decl.node] = typeOp;
      return mlir::success();
    }

    ada_node type_def{};
    if (!libadalang::isEnumTypeDecl(type_decl) ||
        !ada_type_decl_f_type_def(&type_decl, &type_def))
      return mlir::failure();

    // Get the MLIR integer type for this enum.
    mlir::Type mlirType = getMLIRTypeFromDecl(type_decl, location);
    if (!mlirType)
      return mlir::failure();

    auto typeName = resolveTypeName();
    if (mlir::failed(typeName))
      return mlir::failure();

    // Collect enumerator names (canonical) and representation values.
    ada_node literals;
    ada_enum_type_def_f_enum_literals(&type_def, &literals);
    unsigned litCount = ada_node_children_count(&literals);

    llvm::SmallVector<mlir::Attribute> nameAttrs;
    llvm::SmallVector<int64_t> values;
    nameAttrs.reserve(litCount);
    values.reserve(litCount);

    for (unsigned i = 0; i < litCount; ++i) {
      ada_node lit;
      if (ada_node_child(&literals, i, &lit) == 0) {
        mlir::emitError(location, "failed to get enum literal");
        return mlir::failure();
      }

      ada_node lit_name;
      if (!ada_enum_literal_decl_f_name(&lit, &lit_name) ||
          ada_node_is_null(&lit_name)) {
        mlir::emitError(location, "failed to get enum literal name");
        return mlir::failure();
      }
      nameAttrs.push_back(mlir::StringAttr::get(
          builder.getContext(),
          libadalang::getName(&lit_name, /*canonical=*/true)));

      ada_big_integer bigint;
      if (!ada_enum_literal_decl_p_enum_rep(&lit, &bigint)) {
        mlir::emitError(location, "failed to get enum literal rep value");
        return mlir::failure();
      }
      std::string s = libadalang::bigIntToString(bigint);
      errno = 0;
      char *end;
      int64_t val = static_cast<int64_t>(std::strtoll(s.c_str(), &end, 10));
      if (end == s.c_str() || errno == ERANGE) {
        mlir::emitError(location, "enum rep value out of range: ") << s;
        return mlir::failure();
      }
      values.push_back(val);
    }

    auto typeInfo = mlir::ada::EnumTypeInfoAttr::get(
        builder.getContext(),
        mlir::ArrayAttr::get(builder.getContext(), nameAttrs), values);
    auto typeOp = builder.create<mlir::ada::TypeOp>(location, *typeName,
                                                    mlirType, typeInfo);
    typeDecls[type_decl.node] = typeOp;
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
  llvm::LogicalResult mlirGenObjectDecl(ada_node &object_decl) {
    auto declLoc = loc(object_decl);

    ada_node type_expr;
    ada_object_decl_f_type_expr(&object_decl, &type_expr);

    ada_node typeDecl{};
    ada_type_expr_p_designated_type_decl(&type_expr, &typeDecl);
    mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(typeDecl, declLoc);
    if (!typeOp)
      return mlir::failure();

    ada_bool isConstant = 0;
    ada_basic_decl_p_is_constant_object(&object_decl, &isConstant);

    ada_node default_expr;
    ada_object_decl_f_default_expr(&object_decl, &default_expr);

    ada_node ids;
    ada_object_decl_f_ids(&object_decl, &ids);
    unsigned count = ada_node_children_count(&ids);
    for (unsigned i = 0; i < count; ++i) {
      ada_node id;
      if (ada_node_child(&ids, i, &id) == 0) {
        mlir::emitError(declLoc, "failed to get declared identifier");
        return mlir::failure();
      }
      auto nameAttr = getNameAttr(&id);

      mlir::Value init;
      if (!ada_node_is_null(&default_expr)) {
        init = visit_expr(default_expr);
        if (!init)
          return mlir::failure();
      }

      if (isConstant) {
        if (!init) {
          mlir::emitError(loc(id),
                          "deferred constant declarations are not supported");
          return mlir::failure();
        }
        declare(id, init);
      } else {
        mlir::MemRefType memrefType = getMLIRMemRefType(type_expr);
        if (!memrefType)
          return mlir::failure();
        auto allocaOp =
            builder.create<mlir::memref::AllocaOp>(loc(id), memrefType);
        setAdaNameLoc(allocaOp, nameAttr, typeOp);
        mlir::Value ptr = allocaOp;
        if (init) {
          auto storeOp =
              builder.create<mlir::memref::StoreOp>(loc(id), init, ptr);
          setAdaNameLoc(storeOp, nameAttr, typeOp);
        } else
          uninitAllocas.insert(ptr);
        declare(id, ptr);
      }
    }
    return mlir::success();
  }

  /// Emit declarations from a subprogram's declarative part.
  /// Supported: ObjectDecl (initialized only), SubpBody (nested subprograms),
  ///            NumberDecl (expression stashed for lazy use-site emission).
  /// Silently skipped: SubpDecl (forward declarations), and everything else.
  /// The AST structure is: DeclarativePart -> AdaNodeList -> decl...
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
        case ada_concrete_type_decl:
          if (mlir::failed(mlirGenTypeDecl(decl)))
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

    // Collect (id, mode, typeDecl) triples for all parameters.
    struct ParamEntry {
      ada_node id;
      ada_node_kind_enum mode;
      ada_node typeDecl;
    };
    llvm::SmallVector<ParamEntry, 4> args_v;

    ada_node_array params;
    ada_node ids;
    ada_base_subp_spec_p_params(&ada_subp_spec, &params);

    for (int i = 0; i < params->n; i++) {
      ada_node mode_node;
      ada_param_spec_f_mode(&params->items[i], &mode_node);
      ada_node_kind_enum mode = ada_node_kind(&mode_node);

      ada_node type_expr;
      ada_param_spec_f_type_expr(&params->items[i], &type_expr);
      ada_node typeDecl{};
      ada_type_expr_p_designated_type_decl(&type_expr, &typeDecl);

      ada_param_spec_f_ids(&params->items[i], &ids);
      for (unsigned int j = 0; j < ada_node_children_count(&ids); j++) {
        ada_node child;
        if (ada_node_child(&ids, j, &child) == 0) {
          ada_node_array_dec_ref(params);
          mlir::emitError(loc(ids), "failed to get parameter identifier");
          return nullptr;
        }
        args_v.push_back({child, mode, typeDecl});
      }
    }
    ada_node_array_dec_ref(params);

    builder.setInsertionPointToStart(entryBlock);

    // Bind parameters. `in` / default parameters are direct SSA values.
    // `in out` / `out` parameters have memref<T> type and carry the caller's
    // alloca pointer; stores to them are immediately visible at the call site.
    // `out` parameters are additionally marked uninitialized.
    for (auto [entry, arg] : llvm::zip(args_v, entryBlock->getArguments())) {
      auto nameAttr = getNameAttr(&entry.id);
      mlir::Location srcLoc = loc(entry.id);
      mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(entry.typeDecl, srcLoc);
      if (typeOp)
        arg.setLoc(
            mlir::NameLoc::get(nameAttr, makeAdaTypeLoc(srcLoc, typeOp)));
      else
        arg.setLoc(mlir::NameLoc::get(nameAttr, srcLoc));
      if (entry.mode == ada_mode_out)
        uninitAllocas.insert(arg);
      declare(entry.id, arg);
    }

    ada_node decls;
    ada_subp_body_f_decls(&subp_body, &decls);
    if (!ada_node_is_null(&decls))
      if (mlir::failed(mlirGenDeclarativePart(decls)))
        return nullptr;

    ada_node stmts;
    ada_subp_body_f_stmts(&subp_body, &stmts);

    if (mlir::failed(visit(stmts))) {
      op->erase();
      return nullptr;
    }

    // Procedures have no explicit return statement; add an implicit one.
    if (isProc)
      builder.create<mlir::ada::ReturnOp>(
          mlir::UnknownLoc::get(builder.getContext()), ArrayRef<mlir::Value>{});

    // Block arguments are subprogram-local; erase any that were marked
    // uninitialized so entries don't accumulate across nested subprograms.
    for (mlir::Value arg : entryBlock->getArguments())
      uninitAllocas.erase(arg);

    return op;
  }

  /// Lower an Ada block statement (ada_begin_block or ada_decl_block) to an
  /// ada.block op. The block's declarative part (if any) and statements
  /// are emitted into the op's region; a new symbol table scope is opened for
  /// the duration so that local declarations are invisible outside the block.
  mlir::LogicalResult mlirGenBlock(ada_node &blockNode, llvm::StringRef name) {
    bool isDecl = ada_node_kind(&blockNode) == ada_decl_block;

    mlir::StringAttr nameAttr =
        name.empty() ? mlir::StringAttr{}
                     : mlir::StringAttr::get(builder.getContext(), name);
    auto blockOp = builder.create<mlir::ada::BlockOp>(loc(blockNode), nameAttr);

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

    // Restore the insertion point to after the ada.block in the parent block.
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

      ada_node mode_node;
      ada_param_spec_f_mode(&params->items[i], &mode_node);
      ada_node_kind_enum mode = ada_node_kind(&mode_node);
      bool writable = (mode == ada_mode_in_out || mode == ada_mode_out);
      mlir::Type argType =
          writable ? mlir::MemRefType::get({}, paramType) : paramType;

      for (unsigned j = 0; j < ada_node_children_count(&ids); j++)
        argTypes.push_back(argType);
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
    auto subpOp = builder.create<mlir::ada::SubpOp>(
        location, libadalang::getName(&name).data(), funcType);

    return subpOp;
  }

  /// Emit a procedure call statement. The callee is looked up first in the
  /// enclosing ada.subp's SymbolTable (for nested subprograms), then in the
  /// module-level SymbolTable (for top-level subprograms).
  llvm::LogicalResult mlirGenCallStmt(ada_node &call_stmt) {
    ada_node call;
    ada_call_stmt_f_call(&call_stmt, &call);
    return mlirGenCallExpr(call) ? mlir::success() : mlir::failure();
  }

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

    mlir::Value ptr = findVarValue(dest_node);
    if (!ptr) {
      mlir::emitError(loc(dest_node), "unknown variable '")
          << libadalang::getName(&dest_node, false) << "'";
      return mlir::failure();
    }
    if (!mlir::isa<mlir::MemRefType>(ptr.getType())) {
      mlir::emitError(loc(dest_node), "cannot assign to '")
          << libadalang::getName(&dest_node, false) << "'";
      return mlir::failure();
    }

    builder.create<mlir::memref::StoreOp>(
        propagateAdaNameLoc(ptr, loc(dest_node)), rhs, ptr);
    uninitAllocas.erase(ptr);
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
    ada_node canon_type;
    if (!ada_base_type_decl_p_canonical_type(
            &type_decl, &libadalang::kNullOrigin, &canon_type) ||
        ada_node_is_null(&canon_type))
      canon_type = type_decl;

    // Enumeration types (RM 3.5.1): choose the smallest integer width that
    // can hold all literals (GNAT convention: up to 256 -> i8, up to 65536
    // -> i16, else i32).
    // TODO: representation clauses (RM 13.4) can assign arbitrary values to
    // literals; the width should then cover the range of those values, not the
    // literal count. Until representation clauses are supported,
    // emitIntConstant will catch out-of-range rep values and report an error.
    ada_node type_def;
    if (!ada_type_decl_f_type_def(&canon_type, &type_def) ||
        ada_node_is_null(&type_def))
      type_def = {};

    if (!ada_node_is_null(&type_def) &&
        ada_node_kind(&type_def) == ada_mod_int_type_def) {
      auto it = typeDecls.find(canon_type.node);
      if (it == typeDecls.end()) {
        mlir::emitError(diagLoc, "modular type used before its declaration");
        return {};
      }
      return it->second.getMlirType();
    }

    if (!ada_node_is_null(&type_def) &&
        ada_node_kind(&type_def) == ada_enum_type_def) {
      ada_node literals;
      ada_enum_type_def_f_enum_literals(&type_def, &literals);
      unsigned count = ada_node_children_count(&literals);
      if (count <= 2)
        return builder.getIntegerType(1);
      if (count <= 256)
        return builder.getIntegerType(8);
      if (count <= 65536)
        return builder.getIntegerType(16);
      return builder.getIntegerType(32);
    }

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
    if (name == libadalang::kUniversalIntTypeName)
      return builder.getI64Type();
    if (name == libadalang::kUniversalRealTypeName)
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

  mlir::MemRefType getMLIRMemRefType(ada_node &type_expr) {
    mlir::Type type = getMLIRType(type_expr);
    if (!type)
      return {};
    return mlir::MemRefType::get({}, type);
  }
};

} // namespace

namespace lalvm {

mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &compilationUnit) {
  return MLIRGenImpl(context).mlirGen(compilationUnit);
}

} // namespace lalvm
