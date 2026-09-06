// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2024-2026 The LALVM Project

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

#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/Path.h"

namespace libadalang = frontend::libadalang;

#define DEBUG_TYPE "ada-mlirgen"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Twine.h"
#include "llvm/ADT/bit.h"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <optional>

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

    verifyUnitFileName(compilationUnit);

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

  mlir::StringAttr getNameAttr(ada_node &node) {
    return builder.getStringAttr(libadalang::getName(&node));
  }

  /// Attach an Ada source name to `op`. The Ada type symbol is carried in the
  /// op's ada.qual result type and will be re-encoded into the loc by
  /// LowerToLLVM patterns so AdaDebugInfoPass can recover it post-conversion.
  void setAdaNameLoc(mlir::Operation *op, mlir::StringAttr name) {
    op->setLoc(mlir::NameLoc::get(name, op->getLoc()));
  }

  /// Overload for values: handles block arguments (which have no defining op)
  /// as well as op results.
  void setAdaNameLoc(mlir::Value val, mlir::StringAttr name) {
    auto newLoc = mlir::NameLoc::get(name, val.getLoc());
    if (auto arg = mlir::dyn_cast<mlir::BlockArgument>(val))
      arg.setLoc(newLoc);
    else
      val.getDefiningOp()->setLoc(newLoc);
  }

  /// Overload with explicit source location: replaces the value's existing
  /// location entirely rather than wrapping it.
  void setAdaNameLoc(mlir::Value val, mlir::StringAttr name,
                     mlir::Location srcLoc) {
    auto newLoc = mlir::NameLoc::get(name, srcLoc);
    if (auto arg = mlir::dyn_cast<mlir::BlockArgument>(val))
      arg.setLoc(newLoc);
    else
      val.getDefiningOp()->setLoc(newLoc);
  }

  /// Return the location to use for a load on `ptr`, with `srcLoc` as the
  /// inner source location. Propagates the variable name from `ptr.getLoc()`
  /// if it is a NameLoc; otherwise returns `srcLoc` unchanged.
  mlir::Location propagateAdaNameLoc(mlir::Value ptr, mlir::Location srcLoc) {
    auto nameLoc = mlir::dyn_cast<mlir::NameLoc>(ptr.getLoc());
    if (!nameLoc)
      return srcLoc;
    return mlir::NameLoc::get(nameLoc.getName(), srcLoc);
  }

  /// Verify that the file name matches the Ada unit name and emit a warning
  /// if it does not.
  ///
  /// @todo Does not support child units (e.g. `Parent.Child`): only the
  ///       last element of the FQN is checked against the file stem.
  void verifyUnitFileName(ada_node &node) {
    // Precondition: adaModule has been created with a name by the caller.
    ada_symbol_type_array fqn = nullptr;
    if (!ada_compilation_unit_p_syntactic_fully_qualified_name(&node, &fqn))
      return;
    if (!fqn || fqn->n == 0) {
      ada_symbol_type_array_dec_ref(fqn);
      return;
    }
    ada_text nameText;
    ada_symbol_text(&fqn->items[fqn->n - 1], &nameText);
    std::string unitName = libadalang::textToString(nameText);
    ada_symbol_type_array_dec_ref(fqn);

    ada_analysis_unit_kind kind;
    if (!ada_compilation_unit_p_unit_kind(&node, &kind))
      return;
    llvm::StringRef ext =
        (kind == ADA_ANALYSIS_UNIT_KIND_UNIT_BODY) ? ".adb" : ".ads";

    if (adaModule.getName()->lower() != unitName)
      mlir::emitWarning(loc(node),
                        "file name does not match unit name, should be \"" +
                            llvm::Twine(unitName) + ext + "\"");
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

  // Named numbers (@rm{3-3-2}): maps each DefiningName node to the
  // pre-evaluated arith.constant (null only when Libadalang fails to evaluate
  // the static expression, diagnosed at the use site). Use-site resolution
  // re-emits the constant in the concrete target type.
  llvm::DenseMap<ada_node, mlir::Value, AdaNodeDenseMapInfo> numberDecls;

  // Cache from type decl node to its emitted ada.type op. Populated by
  // mlirGenTypeDecl; queried via lookupOrEmitTypeOp (which also handles lazy
  // emission for predefined types such as Boolean) and getMLIRTypeFromDecl.
  llvm::DenseMap<ada_base_node, mlir::ada::TypeOp> typeDecls;

  // Every symbol string handed out, for `__N` collision disambiguation.
  llvm::StringSet<> usedSymbols;

  // Unique qualified name of each scope currently open (subprograms and
  // blocks); the top is the prefix for declarations emitted inside the
  // innermost scope.
  llvm::SmallVector<std::string> scopeStack;

  // Enclosing loops, innermost last: the block to branch to on `exit`, and the
  // loop's source name (empty when unnamed) for `exit Loop_Name`.
  llvm::SmallVector<std::pair<mlir::Block *, std::string>> loopStack;

  // Goto labels (@rm{5-8}): canonical label defining-name node -> the CFG block
  // it marks. Created lazily so a forward `goto` and its `<<label>>` converge
  // on one block; keys are unique per label, so no cross-subprogram collision.
  llvm::DenseMap<ada_base_node, mlir::Block *> labelBlocks;

  // Goto labels (@rm{5-8}) recorded while building the current subprogram body
  // as (label block, source name, source loc); swept afterward to fuse a
  // `DILabelRef` marker onto each label's anchor op.
  llvm::SmallVector<std::tuple<mlir::Block *, mlir::StringAttr, mlir::Location>>
      pendingLabels;

  // Canonical defining-name node -> its unique qualified dialect symbol. Keyed
  // on the canonical node so a subprogram spec and body (or a private type's
  // partial and full view) collapse to one entry; this is the declaration-time
  // dedup point, not a resolution map.
  llvm::DenseMap<ada_base_node, std::string> symbolNames;

  // Canonical defining-name node -> the emitted ada.subp, for resolving calls
  // to the callee op (function type, procedure-vs-function).
  llvm::DenseMap<ada_base_node, mlir::ada::SubpOp> subpDecls;

  /// Return the canonical defining name for a defining-name node, collapsing
  /// spec/body and partial/full views. Falls back to the input on failure.
  ada_node canonicalDefName(ada_node defName) {
    ada_node decl, canonDecl, canonName;
    if (!ada_defining_name_p_basic_decl(&defName, &decl) ||
        ada_node_is_null(&decl))
      return defName;
    if (!ada_basic_decl_p_canonical_part(&decl, /*imprecise_fallback=*/false,
                                         &canonDecl) ||
        ada_node_is_null(&canonDecl))
      return defName;
    if (!ada_basic_decl_p_defining_name(&canonDecl, &canonName) ||
        ada_node_is_null(&canonName))
      return defName;
    return canonName;
  }

  /// Return `candidate` if unused, else `candidate__2`, `__3`, ... until
  /// unique. Reserves the chosen name.
  std::string makeUnique(std::string candidate) {
    if (usedSymbols.insert(candidate).second)
      return candidate;
    for (unsigned n = 2;; ++n) {
      std::string c = candidate + "__" + std::to_string(n);
      if (usedSymbols.insert(c).second)
        return c;
    }
  }

  /// The qualified name of the current enclosing scope, or "" at module level.
  /// Tracked by the generator rather than read from the IR because a block
  /// leaves no op to recover its scope from.
  std::string scopePrefixForInsertion() {
    return scopeStack.empty() ? std::string() : scopeStack.back();
  }

  /// Canonical fully-qualified name of a defining name (e.g.
  /// `standard.integer`), falling back to its simple name on failure.
  std::string canonicalFqn(ada_node defName) {
    ada_string_type fqn;
    if (ada_defining_name_p_canonical_fully_qualified_name(&defName, &fqn)) {
      char *buf;
      size_t len;
      ada_string_to_utf8(fqn, &buf, &len);
      std::string name(buf, len);
      free(buf);
      ada_string_dec_ref(fqn);
      return name;
    }
    return libadalang::getName(&defName, /*canonical=*/true);
  }

  /// Assign (or recall) the unique dialect symbol for a declaration's canonical
  /// defining name. Local declarations get `prefix.simple`, made unique with
  /// `__N`. When `useFqn` is set (predefined/external types emitted via the
  /// lazy lookupOrEmitTypeOp path), the canonical fully-qualified name is used
  /// instead (e.g. `standard.integer`). Keyed on the canonical node so a
  /// subprogram spec and body (or a private type's partial and full view)
  /// share one symbol.
  std::string declareSymbol(ada_node canonDef, llvm::StringRef simple,
                            bool useFqn) {
    if (auto it = symbolNames.find(canonDef.node); it != symbolNames.end())
      return it->second;
    std::string name;
    if (useFqn) {
      name = canonicalFqn(canonDef);
    } else {
      std::string prefix = scopePrefixForInsertion();
      name = makeUnique(prefix.empty() ? simple.str()
                                       : prefix + "." + simple.str());
    }
    symbolNames[canonDef.node] = name;
    return name;
  }

  /// Resolve a reference to its qualified dialect symbol by canonical defining
  /// name. Falls back to the canonical fully-qualified name when the referenced
  /// declaration was never emitted by us (i.e. lives in another unit).
  std::string resolveSymbol(ada_node refDefName) {
    ada_node canon = canonicalDefName(refDefName);
    if (auto it = symbolNames.find(canon.node); it != symbolNames.end())
      return it->second;
    return canonicalFqn(canon);
  }

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
  /// If `node` is an xref entry point that fails name resolution, emit
  /// libadalang's diagnostics and return failure. The statement walker
  /// (`visit`) and the declaration walker (`emitDecl`) both route through this,
  /// so a resolution error is reported once, at the entry point, before codegen
  /// descends into the unresolved subtree.
  mlir::LogicalResult checkResolution(ada_node &node) {
    ada_bool isEntryPoint = 0;
    if (ada_ada_node_p_xref_entry_point(&node, &isEntryPoint) && isEntryPoint &&
        libadalang::emitSolverDiagnostics(&node))
      return mlir::failure();
    return mlir::success();
  }

  mlir::LogicalResult visit(ada_node &node) {
    if (mlir::failed(checkResolution(node)))
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
      return mlirGenAssign(node);
    case ada_null_stmt:
      mlir::ada::NullOp::create(builder, loc(node));
      return mlir::success();
    case ada_call_stmt:
      return mlirGenCallStmt(node);
    case ada_if_stmt:
      return mlirGenIf(node);
    // `while`, bare `loop`, and `for` are distinct node kinds, all deriving
    // from BaseLoopStmt (@rm{5-5}).
    case ada_loop_stmt:
    case ada_while_loop_stmt:
    case ada_for_loop_stmt:
      return mlirGenLoop(node, {});
    case ada_exit_stmt:
      return mlirGenExit(node);
    case ada_goto_stmt:
      return mlirGenGoto(node);
    case ada_label:
      return mlirGenLabel(node);
    case ada_named_stmt: {
      // Named statement: "Name: ... end Name;". The name lives on the wrapping
      // named_stmt; the actual statement is f_stmt: a loop or a block.
      ada_node decl, nameNode, stmt;
      ada_named_stmt_f_decl(&node, &decl);
      ada_named_stmt_decl_f_name(&decl, &nameNode);
      ada_named_stmt_f_stmt(&node, &stmt);
      std::string name = libadalang::getName(&nameNode);
      switch (ada_node_kind(&stmt)) {
      case ada_loop_stmt:
      case ada_while_loop_stmt:
      case ada_for_loop_stmt:
        return mlirGenLoop(stmt, name);
      default:
        return mlirGenBlock(stmt, name);
      }
    }
    case ada_begin_block:
    case ada_decl_block:
      return mlirGenBlock(node, {});
    case ada_with_clause:
    case ada_use_package_clause:
    case ada_use_type_clause:
      // Context clauses (@rm{10-1-2}): resolved by libadalang, no codegen.
      // Return without descending, so their names do not warn as unhandled.
      return mlir::success();
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
      // @todo Turn this into an error when lalvm is mature enough.
      mlir::emitWarning(loc(node), "visit: unhandled node '")
          << libadalang::image(&node) << "'";
      break;
    }
    }

    // Only a statement sequence threads a single block across its children; in
    // other contexts (e.g. a list of declarations) a terminated insertion block
    // is an artifact of the previously emitted op, not unreachable code.
    bool inStmtList = ada_node_kind(&node) == ada_stmt_list;
    bool warnedDead = false;
    unsigned i, count = ada_node_children_count(&node);
    for (i = 0; i < count; ++i) {
      ada_node child;
      if (ada_node_child(&node, i, &child) == 0) {
        mlir::emitError(loc(node), "failed to get child node");
        return mlir::failure();
      }
      if (ada_node_is_null(&child))
        continue;
      // A terminator (return, goto) ends its block, so following statements are
      // unreachable, except a label: a live branch target that reopens the flow
      // (@rm{5-8}). Visit labels; warn once and skip the rest.
      if (inStmtList && currentBlockTerminated() &&
          ada_node_kind(&child) != ada_label) {
        if (!warnedDead) {
          mlir::emitWarning(loc(child), "unreachable code");
          warnedDead = true;
        }
        continue;
      }
      warnedDead = false;
      if (mlir::failed(visit(child)))
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
  /// (@rm{6-4-1}), so the resolved value is always a memref.
  mlir::Value resolveVarPtr(ada_node &expr) {
    mlir::Value val = findVarValue(expr);
    if (!val || !mlir::isa<mlir::MemRefType>(val.getType())) {
      mlir::emitError(loc(expr),
                      "actual for `in out`/`out` parameter must be a variable");
      return nullptr;
    }
    return val;
  }

  /// Whether reading `val` (a memref-backed variable) here precedes its first
  /// write: no `memref.store` or by-reference `ada.call` user yet (IR is built
  /// in source order, so only prior writes are present). Covers `out` params
  /// and uninitialized object decls; `in out` params are caller-initialized.
  bool isReadBeforeFirstWrite(mlir::Value val) {
    if (auto arg = mlir::dyn_cast<mlir::BlockArgument>(val)) {
      auto subp =
          mlir::dyn_cast<mlir::ada::SubpOp>(arg.getOwner()->getParentOp());
      auto mode = subp ? subp.getArgAttrOfType<mlir::ada::AdaParamModeAttr>(
                             arg.getArgNumber(), "ada.mode")
                       : mlir::ada::AdaParamModeAttr();
      if (!mode || mode.getValue() != mlir::ada::AdaParamMode::Out)
        return false;
    }
    return llvm::none_of(val.getUsers(), [](mlir::Operation *op) {
      return mlir::isa<mlir::memref::StoreOp, mlir::ada::CallOp>(op);
    });
  }

  /// Emit a variable reference. Resolves via Libadalang cross-reference to
  /// the unique DefiningName node, then looks up the bound value.
  /// For ObjectDecl variables (alloca-backed), emits a memref.load.
  /// For `in`/default-in parameters (direct SSA values), returns the value
  /// directly.
  mlir::Value mlirGenVariable(ada_node &expr) {
    if (mlir::Value val = findVarValue(expr)) {
      if (auto memrefTy = mlir::dyn_cast<mlir::MemRefType>(val.getType())) {
        if (isReadBeforeFirstWrite(val))
          mlir::emitWarning(loc(expr), "variable '")
              << libadalang::getName(&expr, false)
              << "' is read before first assignment";
        return mlir::memref::LoadOp::create(builder,
                                            propagateAdaNameLoc(val, loc(expr)),
                                            memrefTy.getElementType(), val);
      }
      return val; // direct SSA value (in/default-in parameter)
    }

    // Slow path: distinguish error kinds for better diagnostics.
    std::optional<ada_node> ref_decl = libadalang::referencedDecl(expr);
    if (ref_decl && ada_node_kind(&ref_decl.value()) != ada_object_decl) {
      // Declared but not a variable (type, subprogram, etc.).
      mlir::emitError(loc(expr), "cannot use '")
          << libadalang::getName(&expr, false) << "' as a value";
      return nullptr;
    }

    mlir::emitError(loc(expr), "undeclared identifier '")
        << libadalang::getName(&expr, false) << "'";
    return nullptr;
  }

  /// Emit the arithmetic or Boolean logical operation for two precomputed
  /// operands. `op` is the ada_op node (ada_op_plus, ada_op_and, etc.).
  mlir::Value emitBinOp(ada_node &op, mlir::Value lhs, mlir::Value rhs,
                        bool isInteger = false, bool modular = false) {
    auto callerLoc = loc(op);
    bool signedInt = isInteger && !modular;
    mlir::ada::AdaBinaryOp kind;
    auto checks = mlir::ada::AdaChecks{};
    switch (ada_node_kind(&op)) {
    // Boolean logical operators (@rm{4-5-1}): bitwise on `i1`, no checks.
    case ada_op_and:
      kind = mlir::ada::AdaBinaryOp::And;
      break;
    case ada_op_or:
      kind = mlir::ada::AdaBinaryOp::Or;
      break;
    case ada_op_xor:
      kind = mlir::ada::AdaBinaryOp::Xor;
      break;
    case ada_op_plus:
      kind = mlir::ada::AdaBinaryOp::Plus;
      if (signedInt)
        checks = mlir::ada::AdaChecks::Overflow;
      break;
    case ada_op_minus:
      kind = mlir::ada::AdaBinaryOp::Minus;
      if (signedInt)
        checks = mlir::ada::AdaChecks::Overflow;
      break;
    case ada_op_mult:
      kind = mlir::ada::AdaBinaryOp::Mult;
      if (signedInt)
        checks = mlir::ada::AdaChecks::Overflow;
      break;
    case ada_op_div:
      kind = mlir::ada::AdaBinaryOp::Div;
      if (isInteger) {
        checks = mlir::ada::AdaChecks::Division;
        // Resolve the Division_Check against a static divisor at compile time
        // (@rm{11-5}), GNAT-style: a constant nonzero divisor needs no run-time
        // check, while a constant zero divisor fails statically. The divisor
        // may sit behind a representation `ada.coerce`.
        mlir::Value divisor = rhs;
        if (auto co = divisor.getDefiningOp<mlir::ada::CoerceOp>())
          divisor = co.getInput();
        if (auto cst = divisor.getDefiningOp<mlir::ada::ConstantOp>())
          if (auto iv = mlir::dyn_cast<mlir::IntegerAttr>(cst.getValue())) {
            if (iv.getValue().isZero()) {
              mlir::emitError(callerLoc, "division by zero");
              mlir::emitError(callerLoc,
                              "static expression fails Constraint_Check");
              return nullptr;
            }
            // A nonzero divisor cannot raise Divide_By_Zero, and only a divisor
            // of -1 can trigger the signed `Integer'First / -1` overflow, so
            // the check is unnecessary unless the (signed) divisor is exactly
            // -1.
            if (modular || !iv.getValue().isAllOnes())
              checks = mlir::ada::AdaChecks{};
          }
      }
      break;
    default:
      // @todo Support the short-circuit forms `and then` / `or else`
      //       (@rm{4-5-1}): unlike `and`/`or`/`xor` they need short-circuit
      //       evaluation (a CFG branch), so they cannot lower as an eager
      //       BinOp here.
      mlir::emitError(callerLoc, "invalid binary operator '")
          << libadalang::image(&op) << "'";
      return nullptr;
    }
    // Overflow on signed-integer +/-/* (@rm{4-5}, @rm{3-5-4}); the division
    // check on any integer / (@rm{11-5}, zero divisor, and Integer'First / -1
    // for signed). Modular arithmetic wraps, so it carries only the divisor
    // check. A null attr means no checks.
    auto checksAttr =
        checks != mlir::ada::AdaChecks{}
            ? mlir::ada::AdaChecksAttr::get(builder.getContext(), checks)
            : mlir::ada::AdaChecksAttr{};
    return mlir::ada::BinOp::create(builder, callerLoc, kind, lhs, rhs,
                                    checksAttr);
  }

  /// Emit the relational operation for two precomputed operands.
  /// `op` is the relational `ada_op` node; `resultType` is the Boolean result
  /// type resolved from libadalang.
  ///
  /// Predefined scalar `=`/`/=` and the ordering operators `<`/`<=`/`>`/`>=`
  /// (@rm{4-5-2}) are handled, via `ada.cmp`. A Boolean `/=` is the complement
  /// of `=` (@rm{6-6}); `ada.cmp` lowers it to the complementary predicate.
  /// @todo When a type provides a user-defined `"="` (@rm{6-6}), `/=` must be
  ///       lowered as `not ("=" (lhs, rhs))` -- a call to the user `=` negated
  ///       -- rather than as an `ada.cmp`. Needs Boolean `not` support.
  mlir::Value emitCmpOp(ada_node &op, mlir::Value lhs, mlir::Value rhs,
                        mlir::ada::QualType resultType) {
    auto callerLoc = loc(op);
    mlir::ada::AdaRelationalOp kind;
    switch (ada_node_kind(&op)) {
    case ada_op_eq:
      kind = mlir::ada::AdaRelationalOp::Eq;
      break;
    case ada_op_neq:
      kind = mlir::ada::AdaRelationalOp::Neq;
      break;
    case ada_op_lt:
      kind = mlir::ada::AdaRelationalOp::Lt;
      break;
    case ada_op_lte:
      kind = mlir::ada::AdaRelationalOp::Lte;
      break;
    case ada_op_gt:
      kind = mlir::ada::AdaRelationalOp::Gt;
      break;
    case ada_op_gte:
      kind = mlir::ada::AdaRelationalOp::Gte;
      break;
    default:
      mlir::emitError(callerLoc, "invalid relational operator '")
          << libadalang::image(&op) << "'";
      return nullptr;
    }
    if (!resultType) {
      mlir::emitError(callerLoc,
                      "failed to resolve Boolean result type of comparison");
      return nullptr;
    }
    return mlir::ada::CmpOp::create(builder, callerLoc, resultType, kind, lhs,
                                    rhs);
  }

  /// If `expr` is a static expression (@rm{4-9}) of an integer type, evaluate
  /// it in full via `eval_as_int` and emit one range-checked `ada.constant`: a
  /// static expression is computed at compile time, so it carries no run-time
  /// check and no arithmetic op. Returns nullopt when `expr` is not a static
  /// integer expression (the caller emits it normally); returns a value (null
  /// on a failed static Constraint_Check, @rm{11-5}) when handled. Enumeration
  /// types (including Boolean and Character) are static too but are emitted via
  /// their representation, not `eval_as_int`, so they are left to the caller.
  std::optional<mlir::Value> tryEmitStaticIntExpr(ada_node &expr) {
    if (!libadalang::isStaticExpr(expr))
      return std::nullopt;
    mlir::Location location = loc(expr);
    ada_node type_decl = resolveLiteralType(expr, location);
    if (ada_node_is_null(&type_decl))
      return std::nullopt;
    mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(type_decl, location);
    if (!typeOp || !mlir::isa_and_nonnull<mlir::ada::IntegerTypeInfoAttr>(
                       typeOp.getTypeInfoAttr()))
      return std::nullopt;
    auto value = libadalang::evalExprAsInt(expr);
    if (!value) {
      mlir::emitError(location, "failed to evaluate static integer expression");
      return mlir::Value(nullptr);
    }
    return mlir::Value(emitCheckedIntConstant(*value, type_decl, location));
  }

  /// Emit a binary operation.
  mlir::Value mlirGenBinOp(ada_node &binop) {
    if (auto folded = tryEmitStaticIntExpr(binop))
      return *folded;

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

    // Resolve the operation's result type from libadalang.
    ada_node type_decl{};
    mlir::ada::QualType resultType;
    if (ada_expr_p_expression_type(&binop, &type_decl) &&
        !ada_node_is_null(&type_decl))
      resultType = getAdaQualType(type_decl, loc(binop));

    // Relational operators (@rm{4-5-2}) take operands of a common type and
    // yield Boolean. Coerce both operands to that operand type; the result is
    // Boolean, so unlike arithmetic the result type cannot double as the
    // operand coercion target. `coerce` is a no-op today, but is the eventual
    // home for each operand's range/constraint check (@rm{4-5},
    // Constraint_Error)
    // -- which is required even when the operand and operator types are
    // nominally identical, since the static type does not guarantee the value
    // is in range. The Boolean result is coerced at the assignment/decl-init
    // site.
    switch (ada_node_kind(&op)) {
    case ada_op_eq:
    case ada_op_neq:
    case ada_op_lt:
    case ada_op_lte:
    case ada_op_gt:
    case ada_op_gte:
      if (auto operandType =
              mlir::dyn_cast<mlir::ada::QualType>(lhs.getType())) {
        lhs = coerce(lhs, operandType, loc(binop));
        rhs = coerce(rhs, operandType, loc(binop));
      }
      return emitCmpOp(op, lhs, rhs, resultType);
    default:
      break;
    }

    // Arithmetic (@rm{4-5-3}..@rm{4-5-5}) and Boolean logical (@rm{4-5-1})
    // operators: operands and result share one type. Coerce both operands to
    // the result type so SameOperandsAndResultType is satisfied when the sides
    // differ.
    if (resultType) {
      lhs = coerce(lhs, resultType, loc(binop));
      rhs = coerce(rhs, resultType, loc(binop));
    }
    // Integer iff the result type has an `int_info`; modular iff a `base` link
    // carries a modulus (a subtype's own `int_info` has only its range). Floats
    // and enums have no `int_info`, so they are flagged neither.
    bool isInteger = false, modular = false;
    if (mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(type_decl, loc(binop)))
      if (mlir::dyn_cast_or_null<mlir::ada::IntegerTypeInfoAttr>(
              typeOp.getTypeInfoAttr())) {
        isInteger = true;
        modular = isModularTypeOp(typeOp);
      }
    return emitBinOp(op, lhs, rhs, isInteger, modular);
  }

  /// Codegen a unary operation (@rm{4-5-6}) as `ada.unop`. Operand and result
  /// share the operator's type, so the operand is coerced to the resolved
  /// result type (which also takes a subtype operand to its base).
  mlir::Value mlirGenUnOp(ada_node &unop) {
    ada_node operandNode;
    ada_un_op_f_expr(&unop, &operandNode);
    mlir::Value operand = visit_expr(operandNode);
    if (!operand)
      return nullptr;

    ada_node op;
    ada_un_op_f_op(&unop, &op);

    mlir::ada::AdaUnaryOp kind;
    switch (ada_node_kind(&op)) {
    case ada_op_not:
      kind = mlir::ada::AdaUnaryOp::Not;
      break;
    default:
      mlir::emitError(loc(unop), "invalid unary operator '")
          << libadalang::image(&op) << "'";
      return nullptr;
    }

    ada_node type_decl{};
    if (!ada_expr_p_expression_type(&unop, &type_decl) ||
        ada_node_is_null(&type_decl)) {
      mlir::emitError(loc(unop), "failed to resolve type of unary operation");
      return nullptr;
    }
    auto resultType = getAdaQualType(type_decl, loc(unop));
    if (!resultType)
      return nullptr;

    // `not` is defined only for Boolean and modular types (@rm{4-5-6}):
    // Boolean is `i1`; a modular type (or a subtype of one) carries a modulus
    // on its base. Libadalang already rejects other operands; this guards the
    // dialect.
    bool boolOrModular = resultType.getMlirType().isInteger(1);
    if (!boolOrModular)
      if (mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(type_decl, loc(unop)))
        boolOrModular = isModularTypeOp(typeOp);
    if (!boolOrModular) {
      mlir::emitError(loc(unop),
                      "operator \"not\" requires a Boolean or modular operand");
      return nullptr;
    }

    operand = coerce(operand, resultType, loc(unop));
    if (!operand)
      return nullptr;
    return mlir::ada::UnOp::create(builder, loc(unop), operand.getType(), kind,
                                   operand);
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

  /// Resolve a literal/static expression's concrete `ada.qual` type:
  /// `resolveLiteralType` (expression type, falling back to the expected type
  /// for universal types) wrapped via `getAdaQualType`. Returns a null QualType
  /// (diagnostic already emitted) on failure.
  mlir::ada::QualType resolveLiteralQualType(ada_node &node,
                                             mlir::Location location) {
    ada_node type_decl = resolveLiteralType(node, location);
    if (ada_node_is_null(&type_decl))
      return {};
    return getAdaQualType(type_decl, location);
  }

  /// Emit an `ada.constant` for an integer value already range-checked by the
  /// caller against its target type. The value is fit to the target width
  /// (`sextOrTrunc`); the bounds check guarantees the value is representable.
  mlir::Value emitIntConstant(const llvm::APInt &value,
                              mlir::ada::QualType type,
                              mlir::Location location) {
    auto intType = mlir::cast<mlir::IntegerType>(type.getMlirType());
    return mlir::ada::ConstantOp::create(
        builder, location, type,
        mlir::IntegerAttr::get(intType, value.sextOrTrunc(intType.getWidth())));
  }

  /// Emit an ada.constant for a floating-point value. Checks for f32 overflow;
  /// emits a diagnostic and returns nullptr on failure.
  mlir::Value emitRealConstant(double value, mlir::ada::QualType type,
                               mlir::Location location) {
    auto floatType = mlir::cast<mlir::FloatType>(type.getMlirType());
    if (floatType.getWidth() == 32 && std::isinf(static_cast<float>(value))) {
      mlir::emitError(location, "real constant ")
          << value << " out of range for f32";
      return nullptr;
    }
    return mlir::ada::ConstantOp::create(
        builder, location, type, mlir::FloatAttr::get(floatType, value));
  }

  /// Extract the value of an ada_int_literal node via `p_denoted_value`, as an
  /// APInt (exact at any width). Returns nullopt and emits a diagnostic on
  /// failure.
  std::optional<llvm::APInt> evalIntLiteral(ada_node &node) {
    ada_big_integer bigint;
    if (!ada_int_literal_p_denoted_value(&node, &bigint)) {
      mlir::emitError(loc(node), "failed to evaluate integer literal");
      return std::nullopt;
    }
    auto value = libadalang::bigIntToAPInt(bigint);
    if (!value) {
      ada_text text;
      ada_node_text(&node, &text);
      mlir::emitError(loc(node), "invalid integer literal '")
          << libadalang::textToString(text) << "'";
    }
    return value;
  }

  /// Both static bounds recorded in a type's `int_info`, or nullopt when the
  /// type has no integer info or a bound is dynamic. Bounds are kept as APInts
  /// so they stay exact at any integer width.
  std::optional<std::pair<llvm::APInt, llvm::APInt>>
  ownStaticBounds(ada_node &type_decl, mlir::Location location) {
    mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(type_decl, location);
    if (!typeOp)
      return std::nullopt;
    auto info = mlir::dyn_cast_or_null<mlir::ada::IntegerTypeInfoAttr>(
        typeOp.getTypeInfoAttr());
    if (!info)
      return std::nullopt;
    mlir::IntegerAttr lo = info.staticLower(), hi = info.staticUpper();
    if (!lo || !hi)
      return std::nullopt;
    return std::make_pair(lo.getValue(), hi.getValue());
  }

  /// The static range an integer literal of type `type_decl` must satisfy at
  /// compile time, with the (sub)type whose `int_info` it came from (for
  /// diagnostics): the subtype's own bounds when static, else its canonical
  /// base type's. Empty for a universal type or a fully dynamic subtype, whose
  /// check is left to run time.
  struct StaticRange {
    llvm::APInt lo, hi;
    ada_node type_decl;
  };
  std::optional<StaticRange> staticIntCheckRange(ada_node type_decl,
                                                 mlir::Location location) {
    if (libadalang::isUniversalTypeDecl(type_decl))
      return std::nullopt;
    if (auto b = ownStaticBounds(type_decl, location))
      return StaticRange{b->first, b->second, type_decl};
    ada_node canon = libadalang::canonicalType(type_decl);
    if (!libadalang::isUniversalTypeDecl(canon))
      if (auto b = ownStaticBounds(canon, location))
        return StaticRange{b->first, b->second, canon};
    return std::nullopt;
  }

  /// Emit the two static Constraint_Check (@rm{11-5}) diagnostics when `value`
  /// lies outside the static range `[lo, hi]` of type `typeName`; returns true
  /// when out of range. `APSInt` spans the operands' differing widths and
  /// signedness, so `value` is compared exactly, never width-truncated first.
  bool diagnoseOutOfRange(const llvm::APInt &value, const llvm::APInt &lo,
                          const llvm::APInt &hi, llvm::StringRef typeName,
                          mlir::Location location) {
    llvm::APSInt v(value, /*isUnsigned=*/false);
    if (llvm::APSInt::compareValues(v, llvm::APSInt(lo, false)) < 0 ||
        llvm::APSInt::compareValues(v, llvm::APSInt(hi, false)) > 0) {
      mlir::emitError(location, "value not in range of type \"")
          << typeName << "\"";
      mlir::emitError(location, "static expression fails Constraint_Check");
      return true;
    }
    return false;
  }

  /// Emit a range-checked `ada.constant` for integer `value` of type
  /// `type_decl`: the static Constraint_Check (@rm{11-5}), resolved at emission
  /// like GNAT's front end (a value outside the target subtype's static range
  /// fails at compile time; a dynamic-bound subtype falls through to the
  /// run-time check), then the constant. Returns null on a failed check (a
  /// diagnostic is emitted).
  mlir::Value emitCheckedIntConstant(const llvm::APInt &value,
                                     ada_node type_decl,
                                     mlir::Location location) {
    if (auto sr = staticIntCheckRange(type_decl, location)) {
      ada_node defName;
      std::string name =
          ada_basic_decl_p_defining_name(&sr->type_decl, &defName)
              ? canonicalFqn(defName)
              : libadalang::getName(&sr->type_decl);
      if (diagnoseOutOfRange(value, sr->lo, sr->hi, name, location))
        return nullptr;
    }
    mlir::ada::QualType type = getAdaQualType(type_decl, location);
    if (!type)
      return nullptr;
    return emitIntConstant(value, type, location);
  }

  mlir::Value mlirGenIntLiteral(ada_node &node) {
    auto value = evalIntLiteral(node);
    if (!value)
      return nullptr;
    // p_expression_type on an integer literal returns universal_integer, not
    // the concrete type; resolveLiteralType falls back to the expected type
    // from the surrounding context (e.g. the enclosing function's return type).
    auto location = loc(node);
    ada_node type_decl = resolveLiteralType(node, location);
    if (ada_node_is_null(&type_decl))
      return nullptr;
    return emitCheckedIntConstant(*value, type_decl, location);
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
    mlir::ada::QualType type = resolveLiteralQualType(node, location);
    if (!type)
      return nullptr;
    return emitRealConstant(*value, type, location);
  }

  /// Build the `ada.qual<mlirType, @sym>` type pairing `mlirType` with the
  /// Ada type identity of `typeOp` (its symbol name).
  mlir::ada::QualType qualTypeFor(mlir::ada::TypeOp typeOp,
                                  mlir::Type mlirType) {
    auto *ctx = builder.getContext();
    auto typeRef = mlir::FlatSymbolRefAttr::get(ctx, typeOp.getSymName());
    return mlir::ada::QualType::get(ctx, mlirType, typeRef);
  }

  /// Build the `ada.qual<mlirType, @adaSym>` type for a type declaration.
  /// Returns a null type if the declaration is not supported.
  mlir::ada::QualType getAdaQualType(ada_node &type_decl,
                                     mlir::Location location) {
    mlir::Type mlirType = getMLIRTypeFromDecl(type_decl, location);
    if (!mlirType)
      return {};
    mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(type_decl, location);
    if (!typeOp) {
      mlir::emitWarning(location, "ada.type not emitted for this type; "
                                  "Ada type identity will be lost");
      return {};
    }
    return qualTypeFor(typeOp, mlirType);
  }

  /// Build the `ada.qual` type from a type expression (SubtypeIndication).
  mlir::ada::QualType getAdaQualType(ada_node &type_expr) {
    ada_node type_decl;
    if (!ada_type_expr_p_designated_type_decl(&type_expr, &type_decl) ||
        ada_node_is_null(&type_decl)) {
      mlir::emitError(loc(type_expr), "failed to resolve type expression");
      return {};
    }
    return getAdaQualType(type_decl, loc(type_expr));
  }

  /// Insert `ada.coerce` if `val` does not already have type `expected`, then
  /// a Constraint_Check (@rm{11-5}) when `expected` is a constrained subtype.
  mlir::Value coerce(mlir::Value val, mlir::ada::QualType expected,
                     mlir::Location location) {
    if (val.getType() == expected) {
      // A value already of `expected` is trusted to satisfy the constraint,
      // except a constant that acquired the subtype from context without a
      // check: re-run the Constraint_Check so a dynamic-bound subtype is still
      // verified at run time (a static one is already resolved at emission).
      if (val.getDefiningOp<mlir::ada::ConstantOp>())
        return constrainToSubtype(val, val, expected, location);
      return val;
    }
    mlir::Value coerced =
        mlir::ada::CoerceOp::create(builder, location, expected, val);
    return constrainToSubtype(val, coerced, expected, location);
  }

  /// Build an `ada.range` descriptor of subtype `sym` from its bounds (`lo` and
  /// `hi` are `ada.qual` values of the base type and share their type). The
  /// range's machine `boundType` is the bounds' underlying type.
  mlir::Value emitRange(mlir::Value lo, mlir::Value hi,
                        mlir::FlatSymbolRefAttr sym, mlir::Location location) {
    auto boundType =
        mlir::cast<mlir::ada::QualType>(lo.getType()).getMlirType();
    auto rangeType =
        mlir::ada::RangeType::get(builder.getContext(), boundType, sym);
    return mlir::ada::RangeOp::create(builder, location, rangeType, lo, hi);
  }

  /// Return a constant-bounds `ada.range` descriptor for the static-bound
  /// subtype `sym`, emitted lazily: reuse one already present in the current
  /// block, otherwise emit one here, at the first check that needs it. MLIRGen
  /// builds forward, so any descriptor already in the block precedes this check
  /// and dominates it; later checks in the same block then reuse it. The bounds
  /// are `ada.constant`s of `baseQual` (the subtype's base type, RM 3.5).
  mlir::Value staticRangeFor(mlir::FlatSymbolRefAttr sym,
                             mlir::ada::QualType baseQual, llvm::APInt lo,
                             llvm::APInt hi, mlir::Location location) {
    if (mlir::Block *block = builder.getInsertionBlock())
      for (mlir::Operation &op : *block)
        if (auto rangeOp = mlir::dyn_cast<mlir::ada::RangeOp>(&op))
          if (mlir::cast<mlir::ada::RangeType>(rangeOp.getType())
                  .getConstrainedType() == sym)
            return rangeOp.getResult();
    return emitRange(emitIntConstant(lo, baseQual, location),
                     emitIntConstant(hi, baseQual, location), sym, location);
  }

  /// Emit a Constraint_Check (@rm{11-5}) when `coerced` flows into a
  /// constrained scalar subtype `target`. A static source value (`src` an
  /// `ada.constant`) is resolved at emission like a literal site: in range ->
  /// no check, out of range -> diagnostics. A dynamic source is checked at run
  /// time against a descriptor: constant bounds for a static-bound subtype, or
  /// the one elaborated at the subtype declaration for a dynamic-bound one.
  /// Returns `coerced` unchanged when no integer range applies (base types,
  /// unconstrained subtypes, floats).
  mlir::Value constrainToSubtype(mlir::Value src, mlir::Value coerced,
                                 mlir::ada::QualType target,
                                 mlir::Location location) {
    auto typeOp =
        mlir::dyn_cast_or_null<mlir::ada::TypeOp>(mlir::ada::lookupSymbolFrom(
            coerced.getDefiningOp(), target.getAdaType().getValue()));
    if (!typeOp || !typeOp.getBaseAttr())
      return coerced; // base types are unconstrained at this layer
    auto info = mlir::dyn_cast_or_null<mlir::ada::IntegerTypeInfoAttr>(
        typeOp.getTypeInfoAttr());
    if (!info)
      return coerced; // unconstrained subtype (pure renaming)

    mlir::Value range;
    mlir::IntegerAttr loAttr = info.staticLower(), hiAttr = info.staticUpper();
    if (!loAttr || !hiAttr) {
      // Dynamic bounds: the descriptor elaborated once at the subtype decl.
      range = findDynamicRange(coerced.getDefiningOp(), target.getAdaType());
      if (!range)
        return coerced;
    } else {
      // Static bounds: resolve a static source value here like a literal site
      // (on the exact value); a dynamic one checks against constant bounds,
      // reusing one descriptor per block rather than re-emitting per check.
      if (auto cst = src.getDefiningOp<mlir::ada::ConstantOp>())
        if (auto valAttr = mlir::dyn_cast<mlir::IntegerAttr>(cst.getValue())) {
          diagnoseOutOfRange(valAttr.getValue(), loAttr.getValue(),
                             hiAttr.getValue(), typeOp.getSymName(), location);
          return coerced;
        }
      // The bounds have the base type (RM 3.5); it shares the subtype's
      // machine type, so reuse `target.getMlirType()`.
      auto baseQual = mlir::ada::QualType::get(
          builder.getContext(), target.getMlirType(), typeOp.getBaseAttr());
      range = staticRangeFor(target.getAdaType(), baseQual, loAttr.getValue(),
                             hiAttr.getValue(), location);
    }
    return mlir::ada::RangeCheckOp::create(builder, location, target, coerced,
                                           range);
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
      if (mlir::failed(mlirGenTypeDecl(type_decl, /*external=*/true)))
        return {};
      it = typeDecls.find(type_decl.node);
      if (it == typeDecls.end()) {
        mlir::emitError(location, "ada.type not found for type");
        return {};
      }
    }
    return it->second;
  }

  /// Whether `typeOp` is a modular type or a subtype of one (@rm{3-5-4}). The
  /// modulus lives on the base (a subtype's `int_info` has only its range), so
  /// walk `base` links until one carries a modulus, like `getModularModulus`
  /// in `LowerToLLVM`.
  bool isModularTypeOp(mlir::ada::TypeOp typeOp) {
    while (typeOp) {
      if (auto info = mlir::dyn_cast_or_null<mlir::ada::IntegerTypeInfoAttr>(
              typeOp.getTypeInfoAttr()))
        if (info.getModulus())
          return true;
      auto base = typeOp.getBaseAttr();
      if (!base)
        break;
      typeOp = mlir::dyn_cast_or_null<mlir::ada::TypeOp>(
          mlir::ada::lookupSymbolFrom(typeOp, base.getValue()));
    }
    return false;
  }

  /// Emit an enum literal as its integer representation (Ada @rm{13-4}).
  /// The rep value and MLIR type are read from the `ada.type` op metadata;
  /// Libadalang is only consulted for the literal name and the enclosing type.
  ///
  /// @param enumLitDecl `EnumLiteralDecl` for the literal; supplies the
  ///                    canonical name used to look up the rep value in the
  ///                    `ada.type` op metadata.
  /// @param useExpr     Use-site expression node; provides the source location.
  mlir::Value mlirGenEnumLit(ada_node &enumLitDecl, ada_node &useExpr) {
    auto location = loc(useExpr);

    // Get the literal's canonical name to look up its representation value.
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
        mlir::cast<mlir::ada::EnumTypeInfoAttr>(typeOp.getTypeInfoAttr());
    auto value = typeInfo.enumRep(litName);
    if (!value) {
      mlir::emitError(location, "enum literal '")
          << litName << "' not found in ada.type '" << typeOp.getSymName()
          << "'";
      return nullptr;
    }
    mlir::Type mlirType = typeOp.getMlirType();

    auto attr =
        mlir::IntegerAttr::get(mlir::cast<mlir::IntegerType>(mlirType), *value);
    auto typedType = qualTypeFor(typeOp, mlirType);
    auto constOp =
        mlir::ada::ConstantOp::create(builder, location, typedType, attr);
    // No NameLoc: an enum literal is not a named object, and naming the
    // constant would shadow the debug name of a variable initialized from it
    // (after mem2reg promotes the variable to this constant).
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

    ada_node defName;
    if (!ada_name_p_referenced_defining_name(&name_node,
                                             /*imprecise_fallback=*/false,
                                             &defName) ||
        ada_node_is_null(&defName)) {
      mlir::emitError(location, "unknown subprogram '") << calleeName << "'";
      return nullptr;
    }
    auto calleeSubp = subpDecls.lookup(canonicalDefName(defName).node);
    if (!calleeSubp) {
      mlir::emitError(location, "unknown subprogram '") << calleeName << "'";
      return nullptr;
    }

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
          args.push_back(ptr);
        } else {
          mlir::Value val = visit_expr(r_expr);
          if (!val)
            return nullptr;
          // Coerce to the formal Ada type when they differ (implicit
          // conversion).
          if (auto formalTyped =
                  mlir::dyn_cast<mlir::ada::QualType>(formalTypes[i]))
            val = coerce(val, formalTyped, location);
          args.push_back(val);
        }
      }
    }

    auto calleeRef = mlir::FlatSymbolRefAttr::get(builder.getContext(),
                                                  resolveSymbol(defName));

    if (calleeSubp.isFunction()) {
      ada_node type_decl;
      if (!ada_expr_p_expression_type(&call, &type_decl) ||
          ada_node_is_null(&type_decl)) {
        mlir::emitError(location,
                        "failed to resolve return type of function call");
        return nullptr;
      }
      mlir::ada::QualType retType = getAdaQualType(type_decl, location);
      if (!retType)
        return nullptr;
      return mlir::ada::CallOp::create(builder, location, calleeRef, retType,
                                       args);
    }

    return mlir::ada::CallOp::create(builder, location, calleeRef, mlir::Type{},
                                     args);
  }

  /// Emit a function call used in expression context, returning its result.
  /// Errors if the callee is a procedure (no result).
  mlir::Value mlirGenCallExprValue(ada_node &call) {
    auto callOp = mlirGenCallExpr(call);
    if (!callOp)
      return nullptr;
    if (callOp.getNumResults() == 0) {
      mlir::emitError(loc(call), "procedure called in expression context");
      return nullptr;
    }
    return callOp->getResult(0);
  }

  /// Helper returning the indexed element memref for an indexing operation
  /// (load and store). It normalizes the index and emits an ada.index
  /// operation.
  mlir::Value indexedElementRef(ada_node &index) {
    auto location = loc(index);

    ada_node name = {}, suffix = {};
    ada_call_expr_f_name(&index, &name);
    ada_call_expr_f_suffix(&index, &suffix);

    // One dimensional array support limitation.
    if (ada_node_children_count(&suffix) != 1) {
      mlir::emitError(location, "unsupported array index");
      return nullptr;
    }

    // Check that the referenced name is a memref of a qual-wrapped !ada.array.
    mlir::Value arrayRef = findVarValue(name);
    auto arrayMemref =
        arrayRef ? mlir::dyn_cast<mlir::MemRefType>(arrayRef.getType())
                 : mlir::MemRefType{};
    auto arrayQual =
        arrayMemref
            ? mlir::dyn_cast<mlir::ada::QualType>(arrayMemref.getElementType())
            : mlir::ada::QualType{};
    auto layout =
        arrayQual
            ? mlir::dyn_cast<mlir::ada::ArrayType>(arrayQual.getMlirType())
            : mlir::ada::ArrayType{};
    if (!layout) {
      mlir::emitError(location, "'")
          << libadalang::getName(&name, false) << "' is not an array object";
      return nullptr;
    }

    // Get the array's array_info metadata.
    std::optional<ada_node> objDecl = libadalang::referencedDecl(name);
    if (!objDecl) {
      mlir::emitError(location, "failed to resolve '")
          << libadalang::getName(&name, false) << "'";
      return nullptr;
    }
    ada_node typeExpr = {}, arrayTypeDecl = {};
    ada_object_decl_f_type_expr(&objDecl.value(), &typeExpr);
    ada_type_expr_p_designated_type_decl(&typeExpr, &arrayTypeDecl);
    auto typeOp = typeDecls.lookup(arrayTypeDecl.node);
    auto info =
        mlir::cast<mlir::ada::ArrayTypeInfoAttr>(typeOp.getTypeInfoAttr());

    // Raw Ada index, (@todo emit a range check).
    ada_node assoc = {}, indexExpr = {};
    ada_node_child(&suffix, 0, &assoc);
    ada_param_assoc_f_r_expr(&assoc, &indexExpr);
    mlir::Value rawIndex = visit_expr(indexExpr);
    if (!rawIndex)
      return nullptr;
    auto indexQual = mlir::cast<mlir::ada::QualType>(rawIndex.getType());
    auto intType = mlir::cast<mlir::IntegerType>(indexQual.getMlirType());
    mlir::Value first = emitIntConstant(
        llvm::APInt(intType.getWidth(), info.staticLower(0).getInt(),
                    /*isSigned=*/true),
        indexQual, location);
    // Convert the offset into a zero-based one for MemRef load/store
    // operations.
    mlir::Value offset = mlir::ada::BinOp::create(
        builder, location, mlir::ada::AdaBinaryOp::Minus, rawIndex, first,
        mlir::ada::AdaChecksAttr{});

    // Element location: memref<!ada.qual<component, @c>, strided<[], offset:
    // ?>>.
    // @todo The StridedLayout is not used at the moment but will required to
    // support slices.
    auto eltQual = mlir::ada::QualType::get(
        builder.getContext(), layout.getComponentType(), info.getComponent());

    auto strided = mlir::StridedLayoutAttr::get(
        builder.getContext(), mlir::ShapedType::kDynamic, /*strides=*/{});
    auto resultTy = mlir::MemRefType::get(/*shape=*/{}, eltQual, strided);

    return mlir::ada::IndexOp::create(builder, location, resultTy, arrayRef,
                                      offset);
  }

  /// Emit an array index result.
  mlir::Value mlirGenIndexValue(ada_node &index) {
    mlir::Value elt = indexedElementRef(index);
    if (!elt)
      return nullptr;
    return mlir::memref::LoadOp::create(
        builder, loc(index),
        mlir::cast<mlir::MemRefType>(elt.getType()).getElementType(), elt);
  }

  /// Emit a scalar attribute reference (@rm{3-5}): `'First`/`'Last` on an
  /// integer subtype prefix. The decision is per bound: a static bound is read
  /// straight from the subtype's `int_info` and emitted as a constant; a
  /// dynamic bound is read at run time from the range descriptor elaborated at
  /// the subtype declaration, via `ada.attr`. So `range 1 .. N` yields a
  /// constant `'First` but a dynamic `'Last`.
  mlir::Value mlirGenAttributeRef(ada_node &expr) {
    mlir::Location location = loc(expr);

    ada_node attrId;
    ada_attribute_ref_f_attribute(&expr, &attrId);
    std::string name = libadalang::getName(&attrId, /*canonical=*/true);
    // @todo Support the other scalar/enumeration attributes (@rm{3-5-5}):
    //       'Pos, 'Val, 'Succ, 'Pred, 'Image, 'Value.
    if (name != "first" && name != "last") {
      mlir::emitError(location, "unsupported attribute '") << name << "'";
      return nullptr;
    }

    ada_node prefix, subtype;
    ada_attribute_ref_f_prefix(&expr, &prefix);
    if (!ada_name_p_name_designated_type(&prefix, &subtype) ||
        ada_node_is_null(&subtype)) {
      mlir::emitError(location, "'") << name << "' prefix is not a subtype";
      return nullptr;
    }
    mlir::ada::QualType type = getAdaQualType(subtype, location);
    if (!type)
      return nullptr;
    mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(subtype, location);
    auto info = mlir::dyn_cast_or_null<mlir::ada::IntegerTypeInfoAttr>(
        typeOp ? typeOp.getTypeInfoAttr() : mlir::Attribute());
    if (!info) {
      mlir::emitError(location, "'")
          << name << "' is only supported on integer subtypes";
      return nullptr;
    }

    // Static bound: read it straight from the subtype's `int_info`.
    bool wantLow = name == "first";
    if (mlir::IntegerAttr bound =
            wantLow ? info.staticLower() : info.staticUpper())
      return emitIntConstant(bound.getValue(), type, location);

    // Dynamic bound: read from the range descriptor elaborated at the subtype
    // declaration.
    mlir::Value range = findDynamicRange(
        builder.getInsertionBlock()->getParentOp(), type.getAdaType());
    if (!range) {
      mlir::emitError(location, "no range descriptor in scope for '")
          << name << "'";
      return nullptr;
    }
    return mlir::ada::AttrOp::create(builder, location, type,
                                     builder.getStringAttr(name), range);
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
            mlir::Location exprLoc = loc(expr);
            mlir::ada::QualType tgtType = resolveLiteralQualType(expr, exprLoc);
            if (!tgtType)
              return nullptr;
            return coerce(value, tgtType, exprLoc);
          }
          mlir::emitError(loc(expr), "failed to evaluate named number");
          return nullptr;
        }
      }
      // Enum literal: emit as its integer representation. (Checked before the
      // call test below: enum literals are parameterless functions that
      // p_is_call also reports as calls.)
      std::optional<ada_node> ref_decl = libadalang::referencedDecl(expr);
      if (ref_decl && ada_node_kind(&ref_decl.value()) == ada_enum_literal_decl)
        return mlirGenEnumLit(ref_decl.value(), expr);
      // A parameterless function call written without parentheses (e.g.
      // `F : Float := G`) is an identifier that p_is_call reports as a call
      // (@rm{6-4}); lower it as a call rather than rejecting it as a value.
      ada_bool isCall = false;
      if (ada_name_p_is_call(&expr, &isCall) && isCall)
        return mlirGenCallExprValue(expr);
      return mlirGenVariable(expr);
    }
    case ada_int_literal:
      return mlirGenIntLiteral(expr);
    case ada_real_literal:
      return mlirGenRealLiteral(expr);
    case ada_char_literal: {
      // A character literal denotes an enumeration literal of a character type
      // (predefined Standard.Character, @rm{3-5-2}); emit it as the
      // corresponding enum value, like any other enum literal.
      std::optional<ada_node> ref_decl = libadalang::referencedDecl(expr);
      if (ref_decl && ada_node_kind(&ref_decl.value()) == ada_enum_literal_decl)
        return mlirGenEnumLit(ref_decl.value(), expr);
      mlir::emitError(loc(expr), "failed to resolve character literal");
      return nullptr;
    }
    case ada_bin_op:
    // RelationOp (the relational operators '=', '/=', '<', ...) derives from
    // BinOp, so the ada_bin_op_f_* accessors apply; mlirGenBinOp routes the
    // relational kinds to ada.cmp.
    case ada_relation_op:
      return mlirGenBinOp(expr);
    case ada_un_op:
      return mlirGenUnOp(expr);
    case ada_call_expr: {
      ada_call_expr_kind kind;
      if (!ada_call_expr_p_kind(&expr, &kind)) {
        mlir::emitError(loc(expr), "can't get kind of call expr");
        return nullptr;
      }

      switch (kind) {
      case ADA_CALL_EXPR_KIND_ARRAY_INDEX:
        return mlirGenIndexValue(expr);
      default:
        return mlirGenCallExprValue(expr);
      }
    }
    case ada_attribute_ref:
      return mlirGenAttributeRef(expr);
    case ada_if_expr:
      return mlirGenIfExpr(expr);
    case ada_paren_expr: {
      // A parenthesized expression (@rm{4-4}) has the value of its operand; the
      // parentheses only group syntactically. Visit the inner expression.
      ada_node inner;
      ada_paren_expr_f_expr(&expr, &inner);
      return visit_expr(inner);
    }
    default:
      mlir::emitError(loc(expr), "unsupported expression: ")
          << libadalang::image(&expr);
    }

    return nullptr;
  }

  /// Emit a named number declaration (@rm{3-3-2}).
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
  /// `numberDecls` with an `ada.constant` SSA value at universal type (null
  /// only when Libadalang fails to evaluate the expression, diagnosed at the
  /// use site). Use-site resolution emits `ada.coerce` from the universal
  /// constant to the concrete type required by context.
  mlir::LogicalResult mlirGenNumberDecl(ada_node &number_decl) {
    ada_node expr;
    ada_number_decl_f_expr(&number_decl, &expr);

    // Determine the universal type of the static expression.
    ada_node exprType;
    bool hasConstantValue = ada_expr_p_expression_type(&expr, &exprType) &&
                            !ada_node_is_null(&exprType);

    enum class UniversalKind { Unknown, Int, Real };
    UniversalKind kind = UniversalKind::Unknown;
    std::string unknownTypeName;
    if (hasConstantValue) {
      ada_node typeNameNode;
      ada_base_type_decl_f_name(&exprType, &typeNameNode);
      if (!ada_node_is_null(&typeNameNode)) {
        std::string typeName = libadalang::getName(&typeNameNode);
        kind =
            llvm::StringSwitch<UniversalKind>(typeName)
                .Case(libadalang::kUniversalIntTypeName, UniversalKind::Int)
                .Case(libadalang::kUniversalRealTypeName, UniversalKind::Real)
                .Default(UniversalKind::Unknown);
        if (kind == UniversalKind::Unknown)
          unknownTypeName = std::move(typeName);
      }
    }

    // Evaluate to a universal-typed constant; each use site coerces it to the
    // concrete type its context requires.
    mlir::TypedAttr constAttr;
    mlir::ada::TypeOp typeOp;
    switch (kind) {
    case UniversalKind::Int: {
      if (auto value = libadalang::evalExprAsInt(expr)) {
        typeOp = lookupOrEmitTypeOp(exprType, loc(number_decl));
        if (typeOp) {
          auto intType = mlir::cast<mlir::IntegerType>(typeOp.getMlirType());
          constAttr = mlir::IntegerAttr::get(
              intType, value->sextOrTrunc(intType.getWidth()));
        }
      }
      break;
    }
    case UniversalKind::Real: {
      // Parentheses only group syntactically (@rm{4-4}): strip them to reach
      // the literal node, as `visit_expr` does when emitting. This cannot
      // reuse `visit_expr` itself, which resolves a concrete type from the
      // use context; here the value must stay at universal type.
      ada_node literal = expr;
      while (ada_node_kind(&literal) == ada_paren_expr) {
        ada_node inner;
        if (ada_paren_expr_f_expr(&literal, &inner) == 0 ||
            ada_node_is_null(&inner))
          break;
        literal = inner;
      }
      if (ada_node_kind(&literal) == ada_real_literal) {
        auto value = evalRealLiteral(literal);
        if (value) {
          typeOp = lookupOrEmitTypeOp(exprType, loc(number_decl));
          if (typeOp)
            constAttr = mlir::FloatAttr::get(
                mlir::cast<mlir::FloatType>(typeOp.getMlirType()), *value);
        }
      }
      break;
    }
    case UniversalKind::Unknown: {
      auto diag = mlir::emitError(loc(number_decl),
                                  "named number has unsupported type");
      if (!unknownTypeName.empty())
        diag << " '" << unknownTypeName << "'";
      return mlir::failure();
    }
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
        auto nameAttr = getNameAttr(id);
        auto typedType = qualTypeFor(typeOp, constAttr.getType());
        auto constOp = mlir::ada::ConstantOp::create(builder, loc(id),
                                                     typedType, constAttr);
        setAdaNameLoc(mlir::Value(constOp), nameAttr);
        constValue = constOp.getResult();
      }
      numberDecls[id] = constValue;
    }
    return mlir::success();
  }

  /// True if `type_decl` is a character type (@rm{3-5-2}): an enumeration whose
  /// literals are character literals. Callers use this once per type to choose
  /// between `enumLiteralRep` and `charLiteralRep` without re-checking each
  /// literal.
  static bool isCharacterType(ada_node type_decl) {
    ada_bool result = false;
    return ada_base_type_decl_p_is_char_type(
               &type_decl, &libadalang::kNullOrigin, &result) &&
           result;
  }

  /// Representation value of an ordinary enum literal: its `p_enum_rep`
  /// (honoring @rm{13-4} representation clauses). Returns nullopt and emits a
  /// diagnostic on failure. For a character type use `charLiteralRep`.
  ///
  /// @todo Carry the rep as an `llvm::APInt`, not `int64_t`: support the full
  ///       `System.Min_Int .. System.Max_Int` range (@rm{13-4}), not just i64.
  ///       `EnumTypeInfoAttr` would then store APInt-valued codes, like the
  ///       universal-int work.
  /// @todo @rm{13-4} requires the codes to be distinct and to satisfy the
  ///       type's predefined ordering (each code exceeds its predecessor's).
  ///       A value is its code, so ordering compares codes; once negative
  ///       codes exist (above), that must be a signed comparison.
  ///       `isUnsignedOrderQualType` (`LowerToLLVM`) treats enums as unsigned,
  ///       correct only for the default non-negative positional codes.
  std::optional<int64_t> enumLiteralRep(ada_node &lit,
                                        mlir::Location location) {
    ada_big_integer bigint;
    if (!ada_enum_literal_decl_p_enum_rep(&lit, &bigint)) {
      mlir::emitError(location, "failed to get enum literal rep value");
      return std::nullopt;
    }
    auto val = libadalang::bigIntToAPInt(bigint);
    // Enum rep values are stored as int64 in EnumTypeInfoAttr, which covers any
    // normally-sized enum (i8..i64); reject the rare value that would not fit.
    if (!val || val->getSignificantBits() > 64) {
      mlir::emitError(location, "enum rep value out of range");
      return std::nullopt;
    }
    return val->getSExtValue();
  }

  /// Representation value of a character enum literal (@rm{3-5-2}): the Latin-1
  /// code point from the wrapped `CharLiteral`'s `p_denoted_value`.
  /// `p_enum_rep` is unusable here because Libadalang materializes only the
  /// referenced Character literals, so it would give a wrong position. Returns
  /// nullopt and emits a diagnostic on failure.
  std::optional<int64_t> charLiteralRep(ada_node &lit,
                                        mlir::Location location) {
    ada_node defName, charLit;
    uint32_t codePoint;
    if (!ada_enum_literal_decl_f_name(&lit, &defName) ||
        ada_node_is_null(&defName) ||
        !ada_defining_name_f_name(&defName, &charLit) ||
        ada_node_is_null(&charLit) ||
        ada_node_kind(&charLit) != ada_char_literal ||
        !ada_char_literal_p_denoted_value(&charLit, &codePoint)) {
      mlir::emitError(location, "failed to evaluate character literal");
      return std::nullopt;
    }
    return codePoint;
  }

  /// Whether mlirGenTypeDecl handles this declaration: numeric, universal,
  /// enumeration, or array types, or subtypes thereof (by canonical type). The
  /// declarative-part walk skips the rest; they fail only when referenced.
  bool isSupportedTypeDecl(ada_node &decl) {
    ada_node canon = libadalang::canonicalType(decl);
    if (ada_node_kind(&decl) == ada_subtype_decl && canon.node == decl.node)
      return false;
    return libadalang::isUniversalTypeDecl(canon) ||
           libadalang::isNumericTypeDecl(canon) ||
           libadalang::isEnumTypeDecl(canon) ||
           libadalang::isArrayTypeDecl(canon);
  }

  /// Build a type declaration's unique dialect symbol (see declareSymbol).
  /// Emits a diagnostic and fails when the declaration has no name.
  llvm::FailureOr<std::string> resolveTypeDeclName(ada_node &type_decl,
                                                   bool external) {
    ada_node nameNode;
    if (!ada_base_type_decl_f_name(&type_decl, &nameNode) ||
        ada_node_is_null(&nameNode)) {
      mlir::emitError(loc(type_decl), "failed to get type name");
      return mlir::failure();
    }
    return declareSymbol(canonicalDefName(nameNode),
                         libadalang::getName(&nameNode, /*canonical=*/true),
                         external);
  }

  /// Emit the ada.type for a subtype declaration (@rm{3-2-2}): representation
  /// and `base` link come from the canonical type; type_info records only the
  /// declaration's own constraint. Enum bounds resolve via the base metadata,
  /// never eval_as_int (which yields positions; rep order equals position
  /// order, @rm{13-4}); an unresolvable bound is dynamic (`?`).
  /// @todo Record float subtype constraints once float_info models bounds.
  mlir::LogicalResult mlirGenSubtypeDecl(ada_node &type_decl, bool external) {
    auto location = loc(type_decl);
    ada_node canon = libadalang::canonicalType(type_decl);
    if (canon.node == type_decl.node)
      return mlir::emitError(location,
                             "failed to resolve the subtype's base type");
    mlir::ada::TypeOp baseOp = lookupOrEmitTypeOp(canon, location);
    if (!baseOp)
      return mlir::failure();

    auto typeName = resolveTypeDeclName(type_decl, external);
    if (mlir::failed(typeName))
      return mlir::failure();

    mlir::Attribute typeInfo;
    ada_node indication{}, constraint{};
    ada_internal_discrete_range range{};
    bool constrained =
        ada_subtype_decl_f_subtype(&type_decl, &indication) &&
        !ada_node_is_null(&indication) &&
        ada_subtype_indication_f_constraint(&indication, &constraint) &&
        !ada_node_is_null(&constraint) &&
        ada_base_type_decl_p_discrete_range(&type_decl, &range) &&
        !ada_node_is_null(&range.low_bound) &&
        !ada_node_is_null(&range.high_bound);
    mlir::Attribute baseInfo =
        constrained ? baseOp.getTypeInfoAttr() : mlir::Attribute();
    if (mlir::isa_and_nonnull<mlir::ada::IntegerTypeInfoAttr>(baseInfo)) {
      typeInfo = mlir::ada::IntegerTypeInfoAttr::get(
          builder.getContext(), /*modulus=*/mlir::IntegerAttr(),
          rangeBoundAttr(range.low_bound), rangeBoundAttr(range.high_bound));
    } else if (auto enumInfo =
                   mlir::dyn_cast_or_null<mlir::ada::EnumTypeInfoAttr>(
                       baseInfo)) {
      auto repOf = [&](ada_node &bound) -> std::optional<int64_t> {
        std::optional<ada_node> lit = libadalang::referencedDecl(bound);
        if (!lit || ada_node_kind(&lit.value()) != ada_enum_literal_decl)
          return std::nullopt;
        ada_node litName{};
        if (!ada_enum_literal_decl_f_name(&lit.value(), &litName) ||
            ada_node_is_null(&litName))
          return std::nullopt;
        return enumInfo.enumRep(libadalang::getName(&litName));
      };
      auto boundAttr = [&](ada_node &bound) -> mlir::Attribute {
        if (std::optional<int64_t> rep = repOf(bound))
          return minimalWidthIntAttr(llvm::APInt(64, *rep, /*isSigned=*/true));
        return mlir::UnitAttr::get(builder.getContext());
      };
      typeInfo = mlir::ada::EnumTypeInfoAttr::get(
          builder.getContext(), builder.getArrayAttr({}), /*values=*/{},
          boundAttr(range.low_bound), boundAttr(range.high_bound));
    }

    auto typeOp = mlir::ada::TypeOp::create(
        builder, location, *typeName, baseOp.getMlirType(), typeInfo,
        mlir::FlatSymbolRefAttr::get(baseOp.getSymNameAttr()));
    typeDecls[type_decl.node] = typeOp;

    // Elaborate a dynamic subtype's range once (@rm{3-2-2}): evaluate its
    // bounds here and emit the `ada.range`, which `findDynamicRange` recovers
    // at each check site. The bound values go in the body's entry block (before
    // this `ada.decls`) so they dominate the checks, while the `ada.type`
    // symbol stays in `ada.decls`. Evaluating the bounds here also runs their
    // side effects exactly once, even if the subtype is never referenced.
    auto intInfo =
        mlir::dyn_cast_or_null<mlir::ada::IntegerTypeInfoAttr>(typeInfo);
    if (intInfo && intInfo.hasRange() &&
        (!intInfo.staticLower() || !intInfo.staticUpper())) {
      mlir::Operation *declsOp = builder.getInsertionBlock()->getParentOp();
      if (mlir::isa<mlir::ada::DeclsOp>(declsOp)) {
        mlir::OpBuilder::InsertionGuard guard(builder);
        builder.setInsertionPoint(declsOp);
        // Bounds have the subtype's base type (RM 3.5). A static bound is an
        // `ada.constant` of it; a dynamic one is its evaluated expression
        // coerced to it (so both bounds share the base `!ada.qual`).
        auto baseQual = mlir::ada::QualType::get(
            builder.getContext(), baseOp.getMlirType(),
            mlir::FlatSymbolRefAttr::get(baseOp.getSymNameAttr()));
        auto boundValue = [&](ada_node &expr,
                              mlir::IntegerAttr stat) -> mlir::Value {
          if (stat)
            return emitIntConstant(stat.getValue(), baseQual, location);
          mlir::Value v = visit_expr(expr);
          return v ? coerce(v, baseQual, loc(expr)) : mlir::Value();
        };
        mlir::Value lo = boundValue(range.low_bound, intInfo.staticLower());
        mlir::Value hi = boundValue(range.high_bound, intInfo.staticUpper());
        if (lo && hi)
          emitRange(lo, hi,
                    mlir::FlatSymbolRefAttr::get(typeOp.getSymNameAttr()),
                    location);
      }
    }
    return mlir::success();
  }

  /// Find the `ada.range` elaborated for the subtype `sym` (by
  /// mlirGenSubtypeDecl) in the entry block of an enclosing subprogram, or
  /// null. Outer subprograms are searched too: a nested subprogram may
  /// reference an outer dynamic subtype, and the descriptor is then an up-level
  /// reference that ClosureConversion lifts into a parameter.
  mlir::Value findDynamicRange(mlir::Operation *from,
                               mlir::FlatSymbolRefAttr sym) {
    // Start at the nearest enclosing subprogram, including `from` itself when
    // it is one (an attribute use anchored on its `ada.subp` must see a range
    // declared in that same subprogram).
    mlir::ada::SubpOp start = mlir::dyn_cast<mlir::ada::SubpOp>(from);
    if (!start)
      start = from->getParentOfType<mlir::ada::SubpOp>();
    for (auto subp = start; subp;
         subp = subp->getParentOfType<mlir::ada::SubpOp>()) {
      if (subp.getBody().empty())
        continue;
      for (mlir::Operation &op : subp.getBody().front())
        if (auto rangeOp = mlir::dyn_cast<mlir::ada::RangeOp>(&op))
          if (mlir::cast<mlir::ada::RangeType>(rangeOp.getType())
                  .getConstrainedType() == sym)
            return rangeOp.getResult();
    }
    return {};
  }

  /// Emit an ada.type op for an Ada type declaration (@rm{3-1}): enumeration
  /// (@rm{3-5-1}), numeric and universal types, and subtype declarations
  /// (@rm{3-2-2}, see mlirGenSubtypeDecl). Other kinds are silently skipped
  /// and will be added as support for each kind is implemented.
  ///
  /// `external` is set by lookupOrEmitTypeOp's lazy path (predefined/Standard
  /// types, emitted at module level): the symbol name uses the canonical fully
  /// qualified Ada name (e.g. `standard.boolean`). In-place declarations get a
  /// local qualified symbol built from the enclosing scope (see declareSymbol).
  ///
  /// @todo FixedTypeInfoAttr, RecordTypeInfoAttr, etc.
  mlir::LogicalResult mlirGenTypeDecl(ada_node &type_decl,
                                      bool external = false) {
    if (ada_node_kind(&type_decl) == ada_subtype_decl)
      return mlirGenSubtypeDecl(type_decl, external);
    // Universal types (@rm{3-4-1}) and numeric types
    // (@rm{3-5-4}, @rm{3-5-6}, @rm{3-5-7}).
    if (libadalang::isUniversalTypeDecl(type_decl) ||
        libadalang::isNumericTypeDecl(type_decl))
      return mlirGenNumericTypeDecl(type_decl, external);
    // Array types (@rm{3-6}).
    if (libadalang::isArrayTypeDecl(type_decl))
      return mlirGenArrayTypeDecl(type_decl, external);
    return mlirGenEnumTypeDecl(type_decl, external);
  }

  /// Emit a universal or numeric type declaration as an `ada.type` carrying
  /// integer/float `type_info` (and, for a subtype, a link to its base).
  mlir::LogicalResult mlirGenNumericTypeDecl(ada_node &type_decl,
                                             bool external) {
    auto location = loc(type_decl);
    auto typeName = resolveTypeDeclName(type_decl, external);
    if (mlir::failed(typeName))
      return mlir::failure();

    mlir::Type mlirType;
    mlir::IntegerAttr modulus;
    if (auto type_def =
            libadalang::typeDefOfKind(type_decl, ada_mod_int_type_def)) {
      ada_node expr;
      ada_mod_int_type_def_f_expr(&type_def.value(), &expr);
      if (!libadalang::isStaticExpr(expr))
        return mlir::emitError(
            location, "modular type modulus is not a static expression");
      auto modulusAP = libadalang::evalExprAsInt(expr);
      if (!modulusAP)
        return mlir::emitError(location, "invalid modular type modulus");
      modulus = minimalWidthIntAttr(*modulusAP);
      // Width holds 0 .. modulus-1, byte-rounded to a power of two (up to
      // i128: GNAT's System.Max_Binary_Modulus is 2**128 with 128-bit ints).
      unsigned need = modulusAP->ceilLogBase2();
      unsigned width = need <= 8    ? 8
                       : need <= 16 ? 16
                       : need <= 32 ? 32
                       : need <= 64 ? 64
                                    : 128;
      mlirType = builder.getIntegerType(width);
    } else {
      mlirType = getMLIRTypeFromDecl(type_decl, location);
      if (!mlirType)
        return mlir::failure();
    }

    mlir::Attribute typeInfo;
    if (mlir::isa<mlir::FloatType>(mlirType)) {
      std::optional<uint32_t> digits = evalFloatDigits(type_decl, location);
      if (!digits)
        return mlir::failure();
      typeInfo =
          mlir::ada::FloatTypeInfoAttr::get(builder.getContext(), *digits);
    } else {
      // Record the declared range as metadata: static bounds as values,
      // dynamic bounds as UnitAttr (printed `?`). Universal integer is
      // unconstrained and keeps the bare attribute: its placeholder range
      // in LAL's Standard (-1 .. 1 standing for an infinite range) must
      // not be recorded.
      mlir::Attribute lower, upper;
      if (!libadalang::isUniversalTypeDecl(type_decl)) {
        ada_internal_discrete_range range;
        if (ada_base_type_decl_p_discrete_range(&type_decl, &range) &&
            !ada_node_is_null(&range.low_bound) &&
            !ada_node_is_null(&range.high_bound)) {
          lower = rangeBoundAttr(range.low_bound);
          upper = rangeBoundAttr(range.high_bound);
        }
      }
      typeInfo = mlir::ada::IntegerTypeInfoAttr::get(builder.getContext(),
                                                     modulus, lower, upper);
    }

    // Link the canonical base type when this declaration is not its own
    // (subtypes, @rm{3-2-2}). Looking the base up emits it first, so the
    // reference always resolves.
    mlir::FlatSymbolRefAttr base;
    ada_node canon_type = libadalang::canonicalType(type_decl);
    if (canon_type.node != type_decl.node) {
      if (mlir::ada::TypeOp baseOp = lookupOrEmitTypeOp(canon_type, location))
        base = mlir::FlatSymbolRefAttr::get(baseOp.getSymNameAttr());
    }

    auto typeOp = mlir::ada::TypeOp::create(builder, location, *typeName,
                                            mlirType, typeInfo, base);
    typeDecls[type_decl.node] = typeOp;
    return mlir::success();
  }

  /// Emit a statically-constrained 1-D array type declaration (@rm{3-6}) as an
  /// `ada.type` whose `mlir_type` is the `!ada.array` layout and whose metadata
  /// is `array_info`.
  mlir::LogicalResult mlirGenArrayTypeDecl(ada_node &type_decl, bool external) {
    auto location = loc(type_decl);
    auto typeName = resolveTypeDeclName(type_decl, external);
    if (mlir::failed(typeName))
      return mlir::failure();

    // @todo Add support to multi-dimensional arrays.
    if (libadalang::arrayNdims(type_decl) != 1)
      return mlir::emitError(location,
                             "multi-dimensional array types are not supported");

    auto type_def = libadalang::typeDefOfKind(type_decl, ada_array_type_def);
    if (!type_def)
      return mlir::emitError(location, "expected an array type definition");

    // Constrainedness is given by the `f_indices` node kind.
    // @todo Add support to unconstrained arrays.
    ada_node indices = {};
    ada_array_type_def_f_indices(&type_def.value(), &indices);
    if (ada_node_kind(&indices) != ada_constrained_array_indices)
      return mlir::emitError(location,
                             "unconstrained array types are not supported");

    ada_node comp_type = {};
    if (!ada_base_type_decl_p_comp_type(&type_decl, /*is_subscript*/ false,
                                        &libadalang::kNullOrigin, &comp_type) ||
        ada_node_is_null(&comp_type))
      return mlir::failure();

    // @todo Add support to non-scalar component arrays.
    if (!libadalang::isNumericTypeDecl(comp_type) &&
        !libadalang::isEnumTypeDecl(comp_type))
      return mlir::emitError(location, "array component type must be scalar");

    // Component type.
    mlir::Type componentType = getMLIRTypeFromDecl(comp_type, location);
    if (!componentType)
      return mlir::failure();
    mlir::ada::TypeOp compOp = lookupOrEmitTypeOp(comp_type, location);
    if (!compOp)
      return mlir::failure();
    auto componentSym = mlir::FlatSymbolRefAttr::get(compOp.getSymNameAttr());

    // Index type (dim 0).
    ada_node index_type = {};
    if (!ada_base_type_decl_p_index_type(
            &type_decl, /*dim=*/0, &libadalang::kNullOrigin, &index_type) ||
        ada_node_is_null(&index_type))
      return mlir::failure();

    mlir::Type indexType = getMLIRTypeFromDecl(index_type, location);
    if (!indexType)
      return mlir::failure();
    mlir::ada::TypeOp indexOp = lookupOrEmitTypeOp(index_type, location);
    if (!indexOp)
      return mlir::failure();
    auto indexSym = mlir::FlatSymbolRefAttr::get(indexOp.getSymNameAttr());

    // The index bounds live in the ContraintList, it's a BinOp (LO .. HI).
    ada_node constraint_list = {};
    ada_constrained_array_indices_f_list(&indices, &constraint_list);
    ada_node dim0 = {};
    if (ada_node_child(&constraint_list, 0, &dim0) == 0 ||
        ada_node_kind(&dim0) != ada_bin_op)
      return mlir::emitError(location,
                             "array index constraint must be a .. binary op");

    ada_node lo_node = {}, hi_node = {};
    ada_bin_op_f_left(&dim0, &lo_node);
    ada_bin_op_f_right(&dim0, &hi_node);
    if (!libadalang::isStaticExpr(lo_node) ||
        !libadalang::isStaticExpr(hi_node))
      return mlir::emitError(location,
                             "array index constraint bounds must be static");
    auto lo = libadalang::evalExprAsInt(lo_node);
    auto hi = libadalang::evalExprAsInt(hi_node);
    if (!lo || !hi)
      return mlir::failure();

    // Extent is the element count: `hi - lo + 1`.
    int64_t extent = hi->getSExtValue() - lo->getSExtValue() + 1;
    auto *ctx = builder.getContext();

    // Build the !ada.array machine type
    auto arrayType =
        mlir::ada::ArrayType::get(ctx, componentType, {indexType}, {extent});
    // Build the array_info attribute
    auto typeInfo = mlir::ada::ArrayTypeInfoAttr::get(
        ctx, componentSym, {indexSym}, {minimalWidthIntAttr(*lo)},
        {minimalWidthIntAttr(*hi)});

    auto typeOp = mlir::ada::TypeOp::create(builder, location, *typeName,
                                            arrayType, typeInfo,
                                            /*base=*/mlir::FlatSymbolRefAttr{});
    typeDecls[type_decl.node] = typeOp;
    return mlir::success();
  }

  /// Emit an enumeration type declaration (@rm{3-5-1}) as an `ada.type`
  /// carrying `enum_info` (enumerator names and representation values). Fails
  /// when the declaration is not a supported enum kind.
  mlir::LogicalResult mlirGenEnumTypeDecl(ada_node &type_decl, bool external) {
    auto location = loc(type_decl);
    auto type_def = libadalang::typeDefOfKind(type_decl, ada_enum_type_def);
    if (!type_def)
      return mlir::failure();

    // Get the MLIR integer type for this enum.
    mlir::Type mlirType = getMLIRTypeFromDecl(type_decl, location);
    if (!mlirType)
      return mlir::failure();

    auto typeName = resolveTypeDeclName(type_decl, external);
    if (mlir::failed(typeName))
      return mlir::failure();

    // Collect enumerator names (canonical) and representation values.
    ada_node literals;
    ada_enum_type_def_f_enum_literals(&type_def.value(), &literals);
    unsigned litCount = ada_node_children_count(&literals);
    bool isChar = isCharacterType(type_decl);

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

      std::optional<int64_t> rep = isChar ? charLiteralRep(lit, location)
                                          : enumLiteralRep(lit, location);
      if (!rep)
        return mlir::failure();
      values.push_back(*rep);
    }

    auto typeInfo = mlir::ada::EnumTypeInfoAttr::get(
        builder.getContext(),
        mlir::ArrayAttr::get(builder.getContext(), nameAttrs), values,
        /*lower=*/{}, /*upper=*/{});
    auto typeOp = mlir::ada::TypeOp::create(builder, location, *typeName,
                                            mlirType, typeInfo);
    typeDecls[type_decl.node] = typeOp;
    return mlir::success();
  }

  /// If `typeDecl`'s scalar type has a `Default_Value` aspect (@rm{3-5}), set
  /// `out` to its expression and return true. `p_get_aspect` resolves the
  /// effective value through subtypes and derived-type overrides.
  bool defaultValueExpr(ada_node &typeDecl, ada_node &out) {
    if (ada_node_is_null(&typeDecl))
      return false;
    ada_analysis_context ctx = ada_unit_context(ada_node_unit(&typeDecl));
    llvm::StringRef aspectName = "default_value";
    ada_text name;
    ada_text_from_utf8(aspectName.data(), aspectName.size(), &name);
    ada_symbol_type sym;
    bool ok = ada_context_symbol(ctx, &name, &sym);
    ada_destroy_text(&name);
    if (!ok)
      return false;
    ada_internal_aspect aspect;
    if (!ada_basic_decl_p_get_aspect(&typeDecl, &sym, /*previous_parts_only=*/0,
                                     /*imprecise_fallback=*/0, &aspect) ||
        !aspect.exists || ada_node_is_null(&aspect.value))
      return false;
    out = aspect.value;
    return true;
  }

  /// Emit an object declaration (@rm{3-3-1}).
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
  mlir::LogicalResult mlirGenObjectDecl(ada_node &object_decl) {
    auto declLoc = loc(object_decl);

    ada_node type_expr;
    ada_object_decl_f_type_expr(&object_decl, &type_expr);
    mlir::MemRefType memrefType = getMLIRMemRefType(type_expr);
    if (!memrefType)
      return mlir::failure();

    ada_node typeDecl{};
    ada_type_expr_p_designated_type_decl(&type_expr, &typeDecl);
    if (!lookupOrEmitTypeOp(typeDecl, declLoc))
      return mlir::failure();

    ada_bool isConstant = 0;
    ada_basic_decl_p_is_constant_object(&object_decl, &isConstant);

    ada_node default_expr;
    ada_object_decl_f_default_expr(&object_decl, &default_expr);

    // Emit an error if the initialization expression is an array object.
    auto elemQual =
        mlir::cast<mlir::ada::QualType>(memrefType.getElementType());
    if (mlir::isa<mlir::ada::ArrayType>(elemQual.getMlirType()) &&
        !ada_node_is_null(&default_expr))
      return mlir::emitError(loc(default_expr),
                             "array object initialization is not supported");

    ada_node ids;
    ada_object_decl_f_ids(&object_decl, &ids);
    unsigned count = ada_node_children_count(&ids);
    for (unsigned i = 0; i < count; ++i) {
      ada_node id;
      if (ada_node_child(&ids, i, &id) == 0) {
        mlir::emitError(declLoc, "failed to get declared identifier");
        return mlir::failure();
      }
      auto nameAttr = getNameAttr(id);

      auto elemType =
          mlir::cast<mlir::ada::QualType>(memrefType.getElementType());

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
        init = coerce(init, elemType, loc(id));
        setAdaNameLoc(init, nameAttr, loc(id));
        declare(id, init);
      } else {
        // No explicit initializer: default-initialize from the type's
        // Default_Value aspect (@rm{3-5}) if it has one. (A constant object
        // instead requires an initializer, handled above.)
        if (!init)
          if (ada_node dv; defaultValueExpr(typeDecl, dv)) {
            init = visit_expr(dv);
            if (!init)
              return mlir::failure();
          }
        auto allocaOp =
            mlir::ada::AllocaOp::create(builder, loc(id), memrefType);
        setAdaNameLoc(mlir::Value(allocaOp), nameAttr);
        mlir::Value ptr = allocaOp;
        if (init) {
          init = coerce(init, elemType, loc(id));
          auto storeOp =
              mlir::memref::StoreOp::create(builder, loc(id), init, ptr);
          setAdaNameLoc(storeOp, nameAttr);
        }
        declare(id, ptr);
      }
    }
    return mlir::success();
  }

  /// Emit declarations from a declarative part.
  /// Supported: ObjectDecl (initialized only), SubpBody (nested subprograms),
  ///            NumberDecl (expression stashed for lazy use-site emission),
  ///            ConcreteTypeDecl.
  /// Silently skipped: SubpDecl (forward declarations), and everything else.
  /// The AST structure is: DeclarativePart -> AdaNodeList -> decl...
  ///
  /// The `ada.subp`/`ada.type` symbols are routed into an interior `ada.decls`
  /// (a SymbolTable), since neither the enclosing `ada.subp` nor a dissolved
  /// block is one; locals (objects, numbers) stay inline in the region.
  /// Declarations are emitted in source order, with the `ada.decls` placed
  /// *after* the locals: each local lands in the region just before the
  /// `ada.decls`, each symbol inside it. So a local dominates the `ada.decls`
  /// (required once a nested subprogram captures it) and is bound before a
  /// later nested subprogram is emitted, while a symbol is still available by
  /// name to the locals around it.
  mlir::LogicalResult mlirGenDeclarativePart(ada_node &decls) {
    // Visit every declaration in source order, invoking `fn`.
    auto forEachDecl =
        [&](llvm::function_ref<mlir::LogicalResult(ada_node &)> fn)
        -> mlir::LogicalResult {
      unsigned listCount = ada_node_children_count(&decls);
      for (unsigned i = 0; i < listCount; ++i) {
        ada_node list;
        if (ada_node_child(&decls, i, &list) == 0)
          return mlir::emitError(loc(decls), "failed to get declarative list");
        unsigned count = ada_node_children_count(&list);
        for (unsigned j = 0; j < count; ++j) {
          ada_node decl;
          if (ada_node_child(&list, j, &decl) == 0)
            return mlir::emitError(loc(decls), "failed to get declaration");
          if (mlir::failed(fn(decl)))
            return mlir::failure();
        }
      }
      return mlir::success();
    };

    // Emit one declaration at the current insertion point; no-op for kinds we
    // don't handle.
    auto emitDecl = [&](ada_node &decl) -> mlir::LogicalResult {
      if (mlir::failed(checkResolution(decl)))
        return mlir::failure();
      switch (ada_node_kind(&decl)) {
      case ada_number_decl:
        return mlirGenNumberDecl(decl);
      case ada_object_decl:
        return mlirGenObjectDecl(decl);
      case ada_concrete_type_decl:
      case ada_subtype_decl:
        if (isSupportedTypeDecl(decl))
          return mlirGenTypeDecl(decl);
        return mlir::success();
      case ada_subp_body:
        return mlirGenSubpBody(decl) ? mlir::success() : mlir::failure();
      default:
        return mlir::success();
      }
    };
    // ada.subp/ada.type are symbols routed into the interior ada.decls; locals
    // (objects, numbers) stay inline in the region.
    auto isSymbol = [&](ada_node &decl) {
      auto kind = ada_node_kind(&decl);
      return ((kind == ada_concrete_type_decl || kind == ada_subtype_decl) &&
              isSupportedTypeDecl(decl)) ||
             kind == ada_subp_body;
    };

    bool hasSymbols = false;
    if (mlir::failed(forEachDecl([&](ada_node &decl) {
          hasSymbols |= isSymbol(decl);
          return mlir::success();
        })))
      return mlir::failure();

    // No symbols: emit the locals inline in source order, no ada.decls needed.
    if (!hasSymbols)
      return forEachDecl(emitDecl);

    // Otherwise route each declaration in source order: locals just before the
    // ada.decls, symbols into it. mlirGenSubpBody moves the insertion point
    // into the nested subprogram, but the next iteration resets it here.
    auto declsOp = mlir::ada::DeclsOp::create(builder, loc(decls));
    mlir::Block *declsBlock = builder.createBlock(&declsOp.getBody());
    if (mlir::failed(forEachDecl([&](ada_node &decl) {
          if (isSymbol(decl))
            builder.setInsertionPointToEnd(declsBlock);
          else
            builder.setInsertionPoint(declsOp);
          return emitDecl(decl);
        })))
      return mlir::failure();
    builder.setInsertionPointAfter(declsOp);
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

    // Guard to function returning array: this is not supported yet.
    if (!isProc) {
      ada_node ret_type_decl = {};
      if (ada_type_expr_p_designated_type_decl(&ret_type_expr,
                                               &ret_type_decl) &&
          !ada_node_is_null(&ret_type_decl) &&
          libadalang::isArrayTypeDecl(ret_type_decl)) {
        mlir::emitError(loc(ret_type_expr),
                        "returning an array is not supported");
        return nullptr;
      }
    }

    mlir::Operation *op = mlirGenSubpSpec(ada_subp_spec, isProc);
    if (!op)
      return nullptr;
    // Enter the subprogram's naming scope for the declarations in its body.
    scopeStack.push_back(mlir::SymbolTable::getSymbolName(op).str());
    auto scopeGuard = llvm::scope_exit([&] { scopeStack.pop_back(); });

    // Neither a loop nor a goto label is reachable across a subprogram boundary
    // (@rm{5-7}, @rm{5-8}), so both are subprogram-local: clear them for this
    // body (a nested subp may be emitted inside an enclosing loop) and restore
    // on exit. This differs from `scopeStack`, which stays cumulative for
    // nested qualified names.
    auto savedLoops = std::move(loopStack);
    auto savedLabels = std::move(labelBlocks);
    auto savedLabelMarkers = std::move(pendingLabels);
    loopStack.clear();
    labelBlocks.clear();
    pendingLabels.clear();
    auto frameGuard = llvm::scope_exit([&] {
      loopStack = std::move(savedLoops);
      labelBlocks = std::move(savedLabels);
      pendingLabels = std::move(savedLabelMarkers);
    });
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
      auto nameAttr = getNameAttr(entry.id);
      mlir::Location srcLoc = loc(entry.id);
      // Ensure the ada.type op is emitted so AdaDebugInfoPass can look it up.
      lookupOrEmitTypeOp(entry.typeDecl, srcLoc);
      arg.setLoc(mlir::NameLoc::get(nameAttr, srcLoc));
      // Record `out` / `in out` mode as an `ada.mode` arg attr (`in` is the
      // implicit default); the uninitialized-read check and later passes read
      // it back to tell the two by-reference modes apart.
      if (entry.mode == ada_mode_out || entry.mode == ada_mode_in_out)
        mlir::cast<mlir::ada::SubpOp>(op).setArgAttr(
            arg.getArgNumber(), "ada.mode",
            mlir::ada::AdaParamModeAttr::get(
                builder.getContext(), entry.mode == ada_mode_out
                                          ? mlir::ada::AdaParamMode::Out
                                          : mlir::ada::AdaParamMode::InOut));
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

    // Procedures have no explicit return statement; add an implicit one,
    // unless the body already ended in a terminator (e.g. an explicit return,
    // possibly inside a block that dissolved into this region).
    if (isProc) {
      if (!currentBlockTerminated())
        mlir::ada::ReturnOp::create(builder,
                                    mlir::UnknownLoc::get(builder.getContext()),
                                    mlir::Value{});
    }

    // Fuse each recorded label's `DILabelRef` marker onto its anchor op,
    // keeping the op's own loc as the fused inner.
    mlir::MLIRContext *ctx = builder.getContext();
    for (auto &[blk, name, labelLoc] : pendingLabels) {
      // Find a lowering-surviving anchor for the label's low_pc: skip ops later
      // passes erase (`ada.null`; a `memref.load`/`store` of a mem2reg-promoted
      // local) and chase a branch-only block to its target (consecutive labels
      // form such blocks, which lowering elides). Consecutive labels may thus
      // share an anchor, each fusing its own marker.
      mlir::Operation *anchor = nullptr;
      mlir::Block *b = blk;
      llvm::SmallPtrSet<mlir::Block *, 4> seen;
      while (b && seen.insert(b).second) {
        mlir::Operation *first = nullptr;
        for (mlir::Operation &o : *b)
          if (!mlir::isa<mlir::ada::NullOp, mlir::memref::LoadOp,
                         mlir::memref::StoreOp>(o)) {
            first = &o;
            break;
          }
        if (auto br = mlir::dyn_cast_or_null<mlir::cf::BranchOp>(first)) {
          b = br.getDest();
          continue;
        }
        anchor = first;
        break;
      }
      if (!anchor)
        continue;
      anchor->setLoc(mlir::FusedLoc::get(
          ctx, {anchor->getLoc()},
          mlir::ada::DILabelRefAttr::get(ctx, name, labelLoc)));
    }

    return op;
  }

  /// Lower an Ada block statement (ada_begin_block or ada_decl_block). The
  /// block dissolves into the enclosing region: its locals and statements are
  /// emitted inline and its nested symbols into an in-place `ada.decls`. A
  /// unique scope segment is pushed so the block's declarations get a qualified
  /// name that reflects the block scope, which no longer exists as an op.
  mlir::LogicalResult mlirGenBlock(ada_node &blockNode, llvm::StringRef name) {
    bool isDecl = ada_node_kind(&blockNode) == ada_decl_block;

    std::string seg = name.empty() ? std::string("b") : name.str();
    std::string prefix = scopePrefixForInsertion();
    scopeStack.push_back(makeUnique(prefix.empty() ? seg : prefix + "." + seg));
    auto scopeGuard = llvm::scope_exit([&] { scopeStack.pop_back(); });

    // Codegen the declarative part (ada_decl_block only).
    if (isDecl) {
      ada_node decls;
      ada_decl_block_f_decls(&blockNode, &decls);
      if (!ada_node_is_null(&decls))
        if (mlir::failed(mlirGenDeclarativePart(decls)))
          return mlir::failure();
    }

    // Codegen the statement sequence inline into the enclosing region.
    ada_node stmts;
    if (isDecl)
      ada_decl_block_f_stmts(&blockNode, &stmts);
    else
      ada_begin_block_f_stmts(&blockNode, &stmts);
    return visit(stmts);
  }

  /// Unwrap an Ada-qualified value (`!ada.qual<T, @...>`) to its underlying
  /// builtin MLIR type `T`, e.g. to feed a `cf`/`arith` op that needs the raw
  /// type. Emits a diagnostic and returns null if `value` is not qualified.
  ///
  /// Unwrapping discards the Ada type identity, so the op's location is a
  /// `FusedLoc` carrying the type reference (the same `DITypeRefAttr`
  /// LowerToLLVM threads through value locations), keeping the type
  /// discoverable for debug info.
  mlir::Value unwrap(mlir::Value value, mlir::Location location) {
    auto qual = mlir::dyn_cast<mlir::ada::QualType>(value.getType());
    if (!qual) {
      mlir::emitError(location, "cannot unwrap non-qualified value of type ")
          << value.getType();
      return nullptr;
    }
    auto *ctx = builder.getContext();
    mlir::Location fused = mlir::FusedLoc::get(
        ctx, {location}, mlir::ada::DITypeRefAttr::get(ctx, qual.getAdaType()));
    return mlir::ada::UnwrapOp::create(builder, fused, qual.getMlirType(),
                                       value);
  }

  /// True if the current insertion block ends in a terminator (e.g. a
  /// `return`). False for an empty or absent block.
  bool currentBlockTerminated() {
    mlir::Block *blk = builder.getInsertionBlock();
    return blk && !blk->empty() &&
           blk->back().hasTrait<mlir::OpTrait::IsTerminator>();
  }

  /// Append a branch to `mergeBlock` if the current insertion block is not
  /// already terminated (e.g. by a `return`).
  void branchToMergeIfOpen(mlir::Block *mergeBlock, mlir::Location location) {
    if (!currentBlockTerminated())
      mlir::cf::BranchOp::create(builder, location, mergeBlock);
  }

  /// The CFG block a goto label marks (@rm{5-8}), created on first reference
  /// and appended to the current subprogram region, so a forward `goto` and its
  /// later `<<label>>` agree on one block. Leaves the insertion point
  /// unchanged.
  mlir::Block *getOrCreateLabelBlock(ada_node defName) {
    ada_base_node key = canonicalDefName(defName).node;
    mlir::Block *&blk = labelBlocks[key];
    if (!blk) {
      blk = new mlir::Block();
      builder.getInsertionBlock()->getParent()->push_back(blk);
    }
    return blk;
  }

  /// Emit a `goto L` (@rm{5-8}): branch to L's block. The current block is now
  /// terminated; statements up to the next label are unreachable.
  mlir::LogicalResult mlirGenGoto(ada_node &goto_stmt) {
    ada_node nameNode, defName;
    ada_goto_stmt_f_label_name(&goto_stmt, &nameNode);
    if (!ada_name_p_referenced_defining_name(
            &nameNode, /*imprecise_fallback=*/0, &defName) ||
        ada_node_is_null(&defName))
      return mlir::emitError(loc(goto_stmt), "unresolved goto label");
    // A label in an enclosing body name-resolves from a nested subprogram
    // (implicit declaration, @rm{5-1}), but @rm{5-8} forbids the transfer;
    // reject it here or a dangling label block fails verification.
    ada_node gotoBody = libadalang::enclosingSubpBody(&goto_stmt);
    ada_node labelBody = libadalang::enclosingSubpBody(&defName);
    if (ada_node_is_null(&gotoBody) || ada_node_is_null(&labelBody) ||
        !ada_node_is_equivalent(&gotoBody, &labelBody))
      return mlir::emitError(loc(goto_stmt),
                             "goto label outside the current subprogram");
    mlir::cf::BranchOp::create(builder, loc(goto_stmt),
                               getOrCreateLabelBlock(defName));
    return mlir::success();
  }

  /// Emit a `<<L>>` label (@rm{5-8}): fall through into L's block (when the
  /// prior statement left the flow open) and resume emission there.
  mlir::LogicalResult mlirGenLabel(ada_node &label) {
    ada_node decl, nameNode;
    ada_label_f_decl(&label, &decl);
    ada_label_decl_f_name(&decl, &nameNode);
    mlir::Block *blk = getOrCreateLabelBlock(nameNode);
    branchToMergeIfOpen(blk, loc(label));
    builder.setInsertionPointToEnd(blk);
    // A DWARF label needs no op of its own: record (block, name, loc) now and,
    // after the body, fuse a `DILabelRef` marker onto the block's next
    // lowering-surviving op (the label's low_pc anchor).
    pendingLabels.push_back(
        {blk, builder.getStringAttr(libadalang::getName(&nameNode)),
         loc(label)});
    return mlir::success();
  }

  /// Emit an if statement (@rm{5-3}) as an unstructured CFG. Each guard (the
  /// `if` condition and every `elsif`) branches to its then-block or to the
  /// next test; the last guard's false edge goes to the else-block, or to the
  /// merge block when there is no else. Elsif conditions are evaluated in their
  /// own blocks, so a condition is tested only when all earlier ones were false
  /// (@rm{5-3}). A branch that does not already terminate (e.g. via `return`)
  /// falls through to the merge block. If every path terminates, the merge
  /// block is unreachable and is erased.
  mlir::LogicalResult mlirGenIf(ada_node &if_stmt) {
    // Collect the guards: the `if` plus each `elsif`, as (condition, stmts).
    llvm::SmallVector<std::pair<ada_node, ada_node>, 4> guards;
    ada_node cond, thenStmts;
    ada_if_stmt_f_cond_expr(&if_stmt, &cond);
    ada_if_stmt_f_then_stmts(&if_stmt, &thenStmts);
    guards.push_back({cond, thenStmts});

    ada_node alternatives;
    ada_if_stmt_f_alternatives(&if_stmt, &alternatives);
    unsigned nalt = ada_node_children_count(&alternatives);
    for (unsigned i = 0; i < nalt; ++i) {
      ada_node part;
      if (ada_node_child(&alternatives, i, &part) == 0 ||
          ada_node_is_null(&part)) {
        mlir::emitError(loc(if_stmt), "failed to get elsif part");
        return mlir::failure();
      }
      ada_node ec, es;
      ada_elsif_stmt_part_f_cond_expr(&part, &ec);
      ada_elsif_stmt_part_f_stmts(&part, &es);
      guards.push_back({ec, es});
    }

    ada_node elsePart;
    ada_if_stmt_f_else_part(&if_stmt, &elsePart);
    bool hasElse = !ada_node_is_null(&elsePart);

    // Split off the merge/continuation block (empty: we are at block end).
    mlir::Block *condBlock = builder.getInsertionBlock();
    mlir::Block *mergeBlock =
        condBlock->splitBlock(builder.getInsertionPoint());

    mlir::Block *curTest = condBlock;
    for (unsigned i = 0; i < guards.size(); ++i) {
      bool last = (i + 1 == guards.size());

      builder.setInsertionPointToEnd(curTest);
      mlir::Value c = visit_expr(guards[i].first);
      if (!c)
        return mlir::failure();
      // The condition must be Boolean (@rm{5-3}, enforced by libadalang).
      // Unwrap to the underlying type; cf.cond_br's verifier requires i1.
      mlir::Value condI1 = unwrap(c, loc(guards[i].first));
      if (!condI1)
        return mlir::failure();

      // The then-block and the block taken when this condition is false.
      mlir::Block *thenBlock = builder.createBlock(mergeBlock);
      mlir::Block *falseBlock;
      if (!last || hasElse)
        falseBlock =
            builder.createBlock(mergeBlock); // next elsif test, or else
      else
        falseBlock = mergeBlock;

      builder.setInsertionPointToEnd(curTest);
      mlir::cf::CondBranchOp::create(builder, loc(guards[i].first), condI1,
                                     thenBlock, falseBlock);

      // Fill the then-block; fall through to merge if it did not terminate.
      builder.setInsertionPointToEnd(thenBlock);
      if (mlir::failed(visit(guards[i].second)))
        return mlir::failure();
      branchToMergeIfOpen(mergeBlock, loc(if_stmt));

      curTest = falseBlock; // the next elsif test, or the else block
    }

    // Else part: after the loop curTest is the else block (when hasElse).
    if (hasElse) {
      builder.setInsertionPointToEnd(curTest);
      ada_node elseStmts;
      ada_else_part_f_stmts(&elsePart, &elseStmts);
      if (mlir::failed(visit(elseStmts)))
        return mlir::failure();
      branchToMergeIfOpen(mergeBlock, loc(if_stmt));
    }

    if (mergeBlock->hasNoPredecessors()) {
      // Every path terminated (e.g. all branches return): the merge is dead.
      // Leave the insertion point in a terminated block so the enclosing
      // statement loop reports any following statements as unreachable.
      mergeBlock->erase();
      builder.setInsertionPointToEnd(condBlock);
    } else {
      builder.setInsertionPointToEnd(mergeBlock);
    }
    return mlir::success();
  }

  /// Emit a loop statement (@rm{5-5}) as an unstructured CFG. `while C` gets a
  /// header that tests C and branches to the body or the merge; a bare `loop`
  /// has none, its body branching to itself so `exit` is the only way out. The
  /// merge block and source name are pushed on `loopStack` for `exit`. A `for`
  /// loop is delegated to `mlirGenForLoop`.
  mlir::LogicalResult mlirGenLoop(ada_node &loopNode, llvm::StringRef name) {
    ada_node spec;
    ada_base_loop_stmt_f_spec(&loopNode, &spec);
    if (!ada_node_is_null(&spec) && ada_node_kind(&spec) == ada_for_loop_spec)
      return mlirGenForLoop(loopNode, spec, name);
    bool isWhile =
        !ada_node_is_null(&spec) && ada_node_kind(&spec) == ada_while_loop_spec;
    if (!ada_node_is_null(&spec) && !isWhile) {
      mlir::emitError(loc(loopNode), "unsupported loop specification");
      return mlir::failure();
    }

    ada_node body;
    ada_base_loop_stmt_f_stmts(&loopNode, &body);

    // Split off the (empty) merge block, then insert the loop blocks before it:
    // entry -> [header ->] body -> merge.
    mlir::Block *entryBlock = builder.getInsertionBlock();
    mlir::Block *mergeBlock =
        entryBlock->splitBlock(builder.getInsertionPoint());
    mlir::Block *bodyBlock = builder.createBlock(mergeBlock);

    // `while` tests the condition in a header each iteration; a bare loop has
    // none and re-enters the body directly.
    mlir::Block *headerBlock = bodyBlock;
    if (isWhile) {
      headerBlock = builder.createBlock(bodyBlock);
      ada_node cond;
      ada_while_loop_spec_f_expr(&spec, &cond);
      builder.setInsertionPointToEnd(headerBlock);
      mlir::Value c = visit_expr(cond);
      if (!c)
        return mlir::failure();
      mlir::Value condI1 = unwrap(c, loc(cond));
      if (!condI1)
        return mlir::failure();
      mlir::cf::CondBranchOp::create(builder, loc(cond), condI1, bodyBlock,
                                     mergeBlock);
    }

    builder.setInsertionPointToEnd(entryBlock);
    mlir::cf::BranchOp::create(builder, loc(loopNode), headerBlock);

    // Body, with the loop on `loopStack` for `exit`; close with the back-edge
    // to the header unless the body already terminated.
    builder.setInsertionPointToEnd(bodyBlock);
    loopStack.push_back({mergeBlock, name.str()});
    auto loopGuard = llvm::scope_exit([&] { loopStack.pop_back(); });
    if (mlir::failed(visit(body)))
      return mlir::failure();
    branchToMergeIfOpen(headerBlock, loc(loopNode));

    if (mergeBlock->hasNoPredecessors()) {
      // Bare loop with no reachable `exit`: the merge is dead. Leave a
      // terminated insertion point so following code reads as unreachable
      // (mirrors `mlirGenIf`).
      mergeBlock->erase();
      builder.setInsertionPointToEnd(bodyBlock);
    } else {
      builder.setInsertionPointToEnd(mergeBlock);
    }
    return mlir::success();
  }

  /// Emit a `for` loop over an explicit discrete range (@rm{5-5}), forward or
  /// reverse. The loop parameter is a read-only local (an `alloca` bound in
  /// `declValues`, promoted to a phi by mem2reg). The CFG tests for the last
  /// iteration before stepping, so the induction step never crosses the bound
  /// and needs no `Overflow_Check`; an empty range runs zero iterations.
  /// `exit` uses `loopStack`, as for the other loop forms.
  ///
  /// @todo Support the currently diagnosed forms: container iteration, iterator
  ///       filters, a range given by a type or subtype name, and non-integer
  ///       (enumeration or real) ranges.
  mlir::LogicalResult mlirGenForLoop(ada_node &loopNode, ada_node &spec,
                                     llvm::StringRef name) {
    // `for ... of` container iteration is deferred.
    ada_node loopType;
    ada_for_loop_spec_f_loop_type(&spec, &loopType);
    if (ada_node_kind(&loopType) != ada_iter_type_in) {
      mlir::emitError(loc(loopNode),
                      "`for ... of` loops are not yet supported");
      return mlir::failure();
    }

    // Iterator filters (`when`) are deferred.
    ada_node filter;
    ada_for_loop_spec_f_iter_filter(&spec, &filter);
    if (!ada_node_is_null(&filter)) {
      mlir::emitError(loc(loopNode),
                      "`for` loop iterator filters are not yet supported");
      return mlir::failure();
    }

    // Only an explicit `L .. H` range is supported; a range given by a type or
    // subtype name or a Range attribute (needing First and Last) is diagnosed.
    ada_node iterExpr;
    ada_for_loop_spec_f_iter_expr(&spec, &iterExpr);
    ada_node rangeOp{};
    if (ada_node_kind(&iterExpr) == ada_bin_op) {
      ada_node op;
      ada_bin_op_f_op(&iterExpr, &op);
      if (ada_node_kind(&op) == ada_op_double_dot)
        rangeOp = iterExpr;
    }
    if (ada_node_is_null(&rangeOp)) {
      mlir::emitError(
          loc(loopNode),
          "`for` loop over a type or subtype name is not yet supported");
      return mlir::failure();
    }

    ada_node reverseNode;
    ada_for_loop_spec_f_has_reverse(&spec, &reverseNode);
    bool isReverse = ada_node_kind(&reverseNode) == ada_reverse_present;

    // Loop parameter and its subtype (@rm{5-5}(6)): from an explicit type mark
    // when present, else the range's type.
    ada_node varDecl;
    ada_for_loop_spec_f_var_decl(&spec, &varDecl);
    ada_node id;
    ada_for_loop_var_decl_f_id(&varDecl, &id);
    ada_node idType;
    ada_for_loop_var_decl_f_id_type(&varDecl, &idType);
    ada_node typeDecl{};
    if (!ada_node_is_null(&idType))
      ada_type_expr_p_designated_type_decl(&idType, &typeDecl);
    else
      ada_expr_p_expression_type(&iterExpr, &typeDecl);
    if (ada_node_is_null(&typeDecl)) {
      mlir::emitError(loc(loopNode),
                      "failed to resolve `for` loop parameter type");
      return mlir::failure();
    }
    mlir::ada::QualType paramType = getAdaQualType(typeDecl, loc(loopNode));
    if (!paramType)
      return mlir::failure();

    // Reject non-integer ranges: an enumeration or real range would step and
    // compare on the representation (wrong for a non-contiguous enumeration).
    // Integer and modular types carry an `IntegerTypeInfoAttr`; others do not.
    if (mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(typeDecl, loc(loopNode)))
      if (!mlir::isa_and_nonnull<mlir::ada::IntegerTypeInfoAttr>(
              typeOp.getTypeInfoAttr())) {
        mlir::emitError(loc(loopNode),
                        "`for` loop over a non-integer range is not yet "
                        "supported");
        return mlir::failure();
      }

    // Boolean result type for the range guard and the pre-step bound test.
    ada_node boolDecl = resolveBooleanTypeDecl(loopNode, loc(loopNode));
    if (ada_node_is_null(&boolDecl))
      return mlir::failure();
    mlir::ada::QualType boolType = getAdaQualType(boolDecl, loc(loopNode));
    if (!boolType)
      return mlir::failure();

    // Compare two induction values, unwrapped to the `i1` a `cf` branch needs.
    auto cmpI1 = [&](mlir::ada::AdaRelationalOp kind, mlir::Value a,
                     mlir::Value b) -> mlir::Value {
      return unwrap(mlir::ada::CmpOp::create(builder, loc(loopNode), boolType,
                                             kind, a, b),
                    loc(loopNode));
    };

    // Blocks in source order: entry -> init -> body -> step -> merge.
    mlir::Block *entryBlock = builder.getInsertionBlock();
    mlir::Block *mergeBlock =
        entryBlock->splitBlock(builder.getInsertionPoint());
    mlir::Block *stepBlock = builder.createBlock(mergeBlock);
    mlir::Block *bodyBlock = builder.createBlock(stepBlock);
    mlir::Block *initBlock = builder.createBlock(bodyBlock);

    // Entry: evaluate the bounds, allocate the loop parameter, and guard the
    // empty range.
    builder.setInsertionPointToEnd(entryBlock);
    ada_node lowNode, highNode;
    ada_bin_op_f_left(&rangeOp, &lowNode);
    ada_bin_op_f_right(&rangeOp, &highNode);

    // A statically null range (`lo > hi`) provably runs zero iterations; warn.
    if (libadalang::isStaticExpr(lowNode) && libadalang::isStaticExpr(highNode))
      if (auto staticLo = libadalang::evalExprAsInt(lowNode))
        if (auto staticHi = libadalang::evalExprAsInt(highNode)) {
          unsigned width =
              std::max(staticLo->getBitWidth(), staticHi->getBitWidth()) + 1;
          if (staticLo->sext(width).sgt(staticHi->sext(width)))
            mlir::emitWarning(loc(loopNode),
                              "loop range is null, loop will not execute");
        }

    mlir::Value lo = visit_expr(lowNode);
    if (!lo)
      return mlir::failure();
    mlir::Value hi = visit_expr(highNode);
    if (!hi)
      return mlir::failure();
    lo = coerce(lo, paramType, loc(lowNode));
    hi = coerce(hi, paramType, loc(highNode));

    auto nameAttr = getNameAttr(id);
    mlir::Value paramPtr = mlir::ada::AllocaOp::create(
        builder, loc(id), mlir::MemRefType::get({}, paramType));
    setAdaNameLoc(paramPtr, nameAttr);
    declare(id, paramPtr);

    mlir::Value guardI1 = cmpI1(mlir::ada::AdaRelationalOp::Lte, lo, hi);
    if (!guardI1)
      return mlir::failure();
    mlir::cf::CondBranchOp::create(builder, loc(loopNode), guardI1, initBlock,
                                   mergeBlock);

    // Init: set the parameter to `hi` (reverse) or `lo`, then enter the body.
    builder.setInsertionPointToEnd(initBlock);
    auto initStore = mlir::memref::StoreOp::create(
        builder, loc(id), isReverse ? hi : lo, paramPtr);
    setAdaNameLoc(initStore, nameAttr);
    mlir::cf::BranchOp::create(builder, loc(loopNode), bodyBlock);

    // Body, with the loop on `loopStack` for `exit`. Its fall-through runs the
    // pre-step bound test: exit at the far bound, else step.
    builder.setInsertionPointToEnd(bodyBlock);
    ada_node body;
    ada_base_loop_stmt_f_stmts(&loopNode, &body);
    loopStack.push_back({mergeBlock, name.str()});
    auto loopGuard = llvm::scope_exit([&] { loopStack.pop_back(); });
    if (mlir::failed(visit(body)))
      return mlir::failure();

    if (!currentBlockTerminated()) {
      mlir::Value paramVal = mlir::memref::LoadOp::create(
          builder, loc(loopNode), paramType, paramPtr);
      mlir::Value atEndI1 =
          cmpI1(mlir::ada::AdaRelationalOp::Eq, paramVal, isReverse ? lo : hi);
      if (!atEndI1)
        return mlir::failure();
      mlir::cf::CondBranchOp::create(builder, loc(loopNode), atEndI1,
                                     mergeBlock, stepBlock);

      // Step: add or subtract 1 (unchecked; the bound test above rules out
      // overflow), then back to the body. `paramVal` dominates the step block
      // (its only predecessor is the bound test), so no reload is needed.
      builder.setInsertionPointToEnd(stepBlock);
      auto intType = mlir::cast<mlir::IntegerType>(paramType.getMlirType());
      mlir::Value one = emitIntConstant(llvm::APInt(intType.getWidth(), 1),
                                        paramType, loc(loopNode));
      mlir::Value next =
          mlir::ada::BinOp::create(builder, loc(loopNode),
                                   isReverse ? mlir::ada::AdaBinaryOp::Minus
                                             : mlir::ada::AdaBinaryOp::Plus,
                                   paramVal, one, mlir::ada::AdaChecksAttr{});
      auto stepStore =
          mlir::memref::StoreOp::create(builder, loc(loopNode), next, paramPtr);
      setAdaNameLoc(stepStore, nameAttr);
      mlir::cf::BranchOp::create(builder, loc(loopNode), bodyBlock);
    } else {
      // The body never falls through (e.g. it always returns): no step or
      // back-edge is reachable.
      stepBlock->erase();
    }

    builder.setInsertionPointToEnd(mergeBlock);
    return mlir::success();
  }

  /// Emit an `exit` statement (@rm{5-7}): branch to the target loop's merge,
  /// found on `loopStack` (innermost, or by name for `exit Loop_Name`). `exit
  /// when C` branches to the merge when C holds, else continues the body.
  mlir::LogicalResult mlirGenExit(ada_node &exit_stmt) {
    if (loopStack.empty()) {
      mlir::emitError(loc(exit_stmt), "exit outside of a loop");
      return mlir::failure();
    }

    mlir::Block *target = nullptr;
    ada_node nameNode;
    ada_exit_stmt_f_loop_name(&exit_stmt, &nameNode);
    if (ada_node_is_null(&nameNode)) {
      target = loopStack.back().first; // innermost
    } else {
      std::string name = libadalang::getName(&nameNode);
      for (auto &entry : llvm::reverse(loopStack))
        if (llvm::StringRef(entry.second).equals_insensitive(name)) {
          target = entry.first;
          break;
        }
      if (!target) {
        mlir::emitError(loc(exit_stmt), "no enclosing loop named '")
            << name << "'";
        return mlir::failure();
      }
    }

    ada_node cond;
    ada_exit_stmt_f_cond_expr(&exit_stmt, &cond);
    if (ada_node_is_null(&cond)) {
      mlir::cf::BranchOp::create(builder, loc(exit_stmt), target);
      return mlir::success();
    }

    // `exit when C`: continue the body in a fresh block taken when C is false.
    mlir::Block *current = builder.getInsertionBlock();
    mlir::Block *contBlock = current->splitBlock(builder.getInsertionPoint());
    builder.setInsertionPointToEnd(current);
    mlir::Value c = visit_expr(cond);
    if (!c)
      return mlir::failure();
    mlir::Value condI1 = unwrap(c, loc(cond));
    if (!condI1)
      return mlir::failure();
    mlir::cf::CondBranchOp::create(builder, loc(exit_stmt), condI1, target,
                                   contBlock);
    builder.setInsertionPointToEnd(contBlock);
    return mlir::success();
  }

  /// Resolve the Boolean type declaration in scope of `node` via `p_bool_type`
  /// (the carrier for synthesized Booleans and comparison results). Emits a
  /// diagnostic and returns a null node on failure.
  ada_node resolveBooleanTypeDecl(ada_node &node, mlir::Location location) {
    ada_node bool_decl;
    if (!ada_ada_node_p_bool_type(&node, &bool_decl) ||
        ada_node_is_null(&bool_decl)) {
      mlir::emitError(location, "failed to resolve Boolean type");
      return {};
    }
    return bool_decl;
  }

  /// Synthesize an `ada.constant` for Boolean `True`. The Boolean type is
  /// resolved with `p_bool_type`; the literal's value is read from the
  /// `enum_info` metadata on the resolved `ada.type` (`enumRep`), keeping the
  /// rep mapping single-sourced rather than hardcoding 1.
  ///
  /// @param context_node Any node, used to reach the analysis unit for the
  ///                     static `p_bool_type` query.
  /// @param location     MLIR location for the constant and diagnostics.
  mlir::Value synthesizeBooleanTrue(ada_node &context_node,
                                    mlir::Location location) {
    ada_node bool_decl = resolveBooleanTypeDecl(context_node, location);
    if (ada_node_is_null(&bool_decl))
      return nullptr;
    mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(bool_decl, location);
    if (!typeOp)
      return nullptr;
    auto enumInfo = mlir::dyn_cast_or_null<mlir::ada::EnumTypeInfoAttr>(
        typeOp.getTypeInfoAttr());
    if (!enumInfo) {
      mlir::emitError(location, "Boolean type missing enum metadata");
      return nullptr;
    }
    std::optional<int64_t> rep = enumInfo.enumRep("true");
    if (!rep) {
      mlir::emitError(location, "Boolean type has no 'true' enum literal");
      return nullptr;
    }
    mlir::ada::QualType boolType = getAdaQualType(bool_decl, location);
    if (!boolType)
      return nullptr;
    auto intType = mlir::cast<mlir::IntegerType>(boolType.getMlirType());
    return mlir::ada::ConstantOp::create(builder, location, boolType,
                                         mlir::IntegerAttr::get(intType, *rep));
  }

  /// Emit an if expression (@rm{4-5-7}) as a (possibly nested) `scf.if` that
  /// yields the expression's value. The expected type T is applied to every
  /// dependent_expression (@rm{4-5-7}(8/3)), so each branch value is coerced to
  /// T before being yielded. `elsif` parts nest as an `scf.if` in the else
  /// region; conditions are thus tested in order, the first True winning
  /// (@rm{4-5-7}(20/3)). When the `else` part is absent the if expression is of
  /// a boolean type and the missing else yields `True` (@rm{4-5-7}(18/3,
  /// 20/3)).
  mlir::Value mlirGenIfExpr(ada_node &if_expr) {
    mlir::Location location = loc(if_expr);

    // Result type, shared by every dependent_expression.
    ada_node type_decl{};
    if (!ada_expr_p_expression_type(&if_expr, &type_decl) ||
        ada_node_is_null(&type_decl)) {
      mlir::emitError(location, "failed to resolve type of if expression");
      return nullptr;
    }
    mlir::ada::QualType resultType = getAdaQualType(type_decl, location);
    if (!resultType)
      return nullptr;

    // Collect the guards: the `if` plus each `elsif`, as (condition, then).
    llvm::SmallVector<std::pair<ada_node, ada_node>, 4> guards;
    ada_node cond, thenExpr;
    ada_if_expr_f_cond_expr(&if_expr, &cond);
    ada_if_expr_f_then_expr(&if_expr, &thenExpr);
    guards.push_back({cond, thenExpr});

    ada_node alternatives;
    ada_if_expr_f_alternatives(&if_expr, &alternatives);
    unsigned nalt = ada_node_children_count(&alternatives);
    for (unsigned i = 0; i < nalt; ++i) {
      ada_node part;
      if (ada_node_child(&alternatives, i, &part) == 0 ||
          ada_node_is_null(&part)) {
        mlir::emitError(location, "failed to get elsif expression part");
        return nullptr;
      }
      ada_node ec, et;
      ada_elsif_expr_part_f_cond_expr(&part, &ec);
      ada_elsif_expr_part_f_then_expr(&part, &et);
      guards.push_back({ec, et});
    }

    ada_node elseExpr;
    ada_if_expr_f_else_expr(&if_expr, &elseExpr);
    bool hasElse = !ada_node_is_null(&elseExpr);

    return emitIfExprChain(guards, /*idx=*/0, elseExpr, hasElse, resultType,
                           location);
  }

  /// Recursively emit the `scf.if` for guard `idx`; the else region holds the
  /// next guard's `scf.if`, the explicit else value, or a synthesized `True`
  /// for a missing else (boolean if expression). See `mlirGenIfExpr`.
  mlir::Value
  emitIfExprChain(llvm::ArrayRef<std::pair<ada_node, ada_node>> guards,
                  unsigned idx, ada_node elseExpr, bool hasElse,
                  mlir::ada::QualType resultType, mlir::Location location) {
    ada_node condNode = guards[idx].first;
    ada_node thenNode = guards[idx].second;

    mlir::Value cond = visit_expr(condNode);
    if (!cond)
      return nullptr;
    // The condition is Boolean (@rm{4-5-7}, enforced by libadalang); unwrap to
    // the underlying i1 that scf.if's verifier requires.
    mlir::Value condI1 = unwrap(cond, loc(condNode));
    if (!condI1)
      return nullptr;

    auto ifOp =
        mlir::scf::IfOp::create(builder, location, mlir::TypeRange{resultType},
                                condI1, /*withElseRegion=*/true);

    // Then region: the dependent_expression for this guard.
    builder.setInsertionPointToStart(ifOp.thenBlock());
    mlir::Value thenVal = visit_expr(thenNode);
    if (!thenVal)
      return nullptr;
    thenVal = coerce(thenVal, resultType, loc(thenNode));
    mlir::scf::YieldOp::create(builder, loc(thenNode), thenVal);

    // Else region: the next guard, the explicit else, or a synthesized True.
    builder.setInsertionPointToStart(ifOp.elseBlock());
    mlir::Value elseVal;
    if (idx + 1 < guards.size()) {
      elseVal = emitIfExprChain(guards, idx + 1, elseExpr, hasElse, resultType,
                                location);
    } else if (hasElse) {
      elseVal = visit_expr(elseExpr);
      if (elseVal)
        elseVal = coerce(elseVal, resultType, loc(elseExpr));
    } else {
      // No else: the if expression is of a boolean type and the absent else
      // yields True (@rm{4-5-7}(18/3, 20/3)).
      elseVal = synthesizeBooleanTrue(condNode, location);
      if (elseVal)
        elseVal = coerce(elseVal, resultType, location);
    }
    if (!elseVal)
      return nullptr;
    mlir::scf::YieldOp::create(builder, location, elseVal);

    builder.setInsertionPointAfter(ifOp);
    return ifOp.getResult(0);
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
      mlir::ada::QualType paramType = getAdaQualType(type_expr);
      if (!paramType) {
        ada_node_array_dec_ref(params);
        return nullptr;
      }
      ada_param_spec_f_ids(&params->items[i], &ids);

      ada_node mode_node;
      ada_param_spec_f_mode(&params->items[i], &mode_node);
      ada_node_kind_enum mode = ada_node_kind(&mode_node);
      bool writable = (mode == ada_mode_in_out || mode == ada_mode_out);
      mlir::Type argType =
          writable ? mlir::Type(mlir::MemRefType::get({}, paramType))
                   : mlir::Type(paramType);

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
      mlir::ada::QualType retType = getAdaQualType(ret_type_expr);
      if (!retType)
        return nullptr;
      retTypes.push_back(retType);
    }
    auto funcType = builder.getFunctionType(argTypes, retTypes);
    ada_node canon = canonicalDefName(name);
    std::string symName =
        declareSymbol(canon, libadalang::getName(&name), /*useFqn=*/false);
    auto subpOp =
        mlir::ada::SubpOp::create(builder, location, symName, funcType);
    // Nested subprograms are not externally visible: mark them private so they
    // skip the GNAT `_ada_` prefix (`getMangledName`). Library-level
    // subprograms (emitted directly under the module) keep the default public
    // visibility.
    if (!mlir::isa<mlir::ModuleOp>(subpOp->getParentOp()))
      subpOp.setPrivate();
    subpDecls[canon.node] = subpOp;

    return subpOp;
  }

  /// Emit a procedure call statement (@rm{6-4}).
  /// @todo Support entry calls.
  /// @todo Check that the returned value of mlirGenCallExpr is not a function.
  mlir::LogicalResult mlirGenCallStmt(ada_node &call_stmt) {
    ada_node call;
    ada_call_stmt_f_call(&call_stmt, &call);
    return mlirGenCallExpr(call) ? mlir::success() : mlir::failure();
  }

  mlir::LogicalResult mlirGenAssign(ada_node &assign_stmt) {
    ada_node dest_node, expr_node;
    ada_assign_stmt_f_dest(&assign_stmt, &dest_node);
    ada_assign_stmt_f_expr(&assign_stmt, &expr_node);

    mlir::Value rhs = visit_expr(expr_node);
    if (!rhs)
      return mlir::failure();

    ada_node_kind_enum kind = ada_node_kind(&dest_node);
    mlir::Value ptr;

    switch (kind) {
    case ada_identifier: {
      ptr = findVarValue(dest_node);
      if (!ptr) {
        mlir::emitError(loc(dest_node), "unknown variable '")
            << libadalang::getName(&dest_node, false) << "'";
        return mlir::failure();
      }
      break;
    }
    case ada_call_expr: {
      ada_call_expr_kind call_kind = {};
      if (ada_call_expr_p_kind(&dest_node, &call_kind) &&
          call_kind == ADA_CALL_EXPR_KIND_ARRAY_INDEX) {
        ptr = indexedElementRef(dest_node);
        if (!ptr)
          return mlir::failure(); // error already emitted
        break;
      }
    }
      [[fallthrough]];
    default: {
      mlir::emitError(loc(assign_stmt), "unsupported assignment destination");
      return mlir::failure();
    }
    }

    // A loop parameter is a constant view (@rm{5-5}(6)): reject assignment to
    // it, even though it is backed by a mutable `alloca`.
    std::optional<ada_node> ref_decl = libadalang::referencedDecl(dest_node);
    if (ref_decl && ada_node_kind(&ref_decl.value()) == ada_for_loop_var_decl) {
      mlir::emitError(loc(dest_node),
                      "assignment to loop parameter not allowed");
      return mlir::failure();
    }

    if (!mlir::isa<mlir::MemRefType>(ptr.getType())) {
      mlir::emitError(loc(dest_node), "cannot assign to '")
          << libadalang::getName(&dest_node, false) << "'";
      return mlir::failure();
    }

    // Coerce to the declared type of the variable (handles subtypes and
    // implicit conversions).
    auto memrefTy = mlir::cast<mlir::MemRefType>(ptr.getType());
    if (auto elemType =
            mlir::dyn_cast<mlir::ada::QualType>(memrefTy.getElementType()))
      rhs = coerce(rhs, elemType, loc(assign_stmt));

    mlir::memref::StoreOp::create(
        builder, propagateAdaNameLoc(ptr, loc(dest_node)), rhs, ptr);
    return mlir::success();
  }

  /// Emit a return operation. This will return failure if any generation fails.
  mlir::LogicalResult mlirGenReturn(ada_node &return_stmt) {
    auto location = loc(return_stmt);

    ada_node return_expr;
    ada_return_stmt_f_return_expr(&return_stmt, &return_expr);

    // In Ada, a procedure return carries no value; only function returns do.
    mlir::Value expr = nullptr;
    if (!ada_node_is_null(&return_expr)) {
      expr = visit_expr(return_expr);
      if (!expr)
        return mlir::failure();
      // Coerce to the subprogram's declared return type (handles subtypes and
      // implicit conversions).
      mlir::Operation *enclosing =
          builder.getInsertionBlock()->getParent()->getParentOp();
      if (auto subp = mlir::dyn_cast<mlir::ada::SubpOp>(enclosing)) {
        auto results = subp.getFunctionType().getResults();
        if (!results.empty())
          if (auto retType =
                  mlir::dyn_cast<mlir::ada::QualType>(results.front()))
            expr = coerce(expr, retType, location);
      }
    }

    mlir::ada::ReturnOp::create(builder, location, expr);
    return mlir::success();
  }

  /// Minimal-signed-width IntegerAttr, the canonical bound form shared with
  /// the info attr parsers.
  mlir::IntegerAttr minimalWidthIntAttr(const llvm::APInt &value) {
    unsigned bits = value.getSignificantBits();
    return mlir::IntegerAttr::get(builder.getIntegerType(bits),
                                  value.sextOrTrunc(bits));
  }

  /// One range bound as type info metadata: minimalWidthIntAttr of its
  /// static value, or UnitAttr (`?`) when it is missing or not static.
  mlir::Attribute rangeBoundAttr(ada_node &bound) {
    if (!ada_node_is_null(&bound))
      if (auto value = libadalang::evalExprAsInt(bound))
        return minimalWidthIntAttr(*value);
    return mlir::UnitAttr::get(builder.getContext());
  }

  /// Evaluate the decimal digits of a floating-point type declaration
  /// (@rm{3-5-7}). The C API exposes no semantic digits property, so the
  /// floating_point_def's num_digits expression is evaluated (static by
  /// definition). Returns 0 when the declaration has no floating_point_def
  /// (universal_real); nullopt, after emitting a diagnostic, on evaluation
  /// failure or when the value exceeds 18 (System.Max_Digits on x86-64).
  std::optional<uint32_t> evalFloatDigits(ada_node &type_decl,
                                          mlir::Location location) {
    auto float_def =
        libadalang::typeDefOfKind(type_decl, ada_floating_point_def);
    if (!float_def)
      return 0;
    ada_node digits_expr;
    ada_big_integer bigint;
    if (!ada_floating_point_def_f_num_digits(&float_def.value(),
                                             &digits_expr) ||
        ada_node_is_null(&digits_expr) ||
        !ada_expr_p_eval_as_int(&digits_expr, &bigint)) {
      mlir::emitError(location,
                      "failed to evaluate floating-point type digits");
      return std::nullopt;
    }
    auto value = libadalang::bigIntToAPInt(bigint);
    // 18 is System.Max_Digits for x86-64 (the 80-bit extended type); GNAT
    // emits the same error, located at the digits expression.
    // @todo Retrieve the maximum from the System package (System.Max_Digits)
    //       through Libadalang instead of hardcoding the x86-64 value.
    if (!value || value->ugt(18)) {
      mlir::emitError(loc(digits_expr),
                      "digits value out of range, maximum is 18");
      return std::nullopt;
    }
    return static_cast<uint32_t>(value->getZExtValue());
  }

  /// Map a type declaration node to an MLIR type. Follows the subtype chain
  /// to the canonical base type, derives integer widths from its range and
  /// float widths from its digits, and matches the remaining (universal)
  /// types by name.
  /// diagLoc is the caller's diagnostic site (typically the use location) for
  /// any error the mapping emits.
  mlir::Type getMLIRTypeFromDecl(ada_node &type_decl, mlir::Location diagLoc) {
    // Follow the subtype chain to the canonical (base) type so that subtypes
    // of Integer map to the same MLIR type as Integer itself.
    ada_node canon_type = libadalang::canonicalType(type_decl);

    // Each helper returns nullopt when its kind does not apply, so the type
    // falls through to the next mapping and finally to the universal-by-name
    // carrier. A present null Type means an error was diagnosed.
    if (auto t = getModularMLIRType(canon_type, diagLoc))
      return *t;
    if (auto t = getEnumMLIRType(canon_type, diagLoc))
      return *t;
    if (auto t = getIntegerMLIRType(canon_type, diagLoc))
      return *t;
    if (auto t = getFloatMLIRType(canon_type, diagLoc))
      return *t;
    if (auto t = getArrayMLIRType(canon_type, diagLoc))
      return *t;
    return getUniversalMLIRType(canon_type, diagLoc);
  }

  /// Modular type (@rm{3-5-4}): reuse the width chosen at declaration and
  /// stored on the `TypeOp`. Nullopt if not modular.
  std::optional<mlir::Type> getModularMLIRType(ada_node &canon_type,
                                               mlir::Location diagLoc) {
    if (!libadalang::typeDefOfKind(canon_type, ada_mod_int_type_def))
      return std::nullopt;

    auto it = typeDecls.find(canon_type.node);
    if (it == typeDecls.end()) {
      mlir::emitError(diagLoc, "modular type used before its declaration");
      return mlir::Type{};
    }
    return it->second.getMlirType();
  }

  /// Enumeration type (@rm{3-5-1}). Nullopt if not an enum.
  std::optional<mlir::Type> getEnumMLIRType(ada_node &canon_type,
                                            mlir::Location diagLoc) {
    auto type_def = libadalang::typeDefOfKind(canon_type, ada_enum_type_def);
    if (!type_def)
      return std::nullopt;

    // Reuse the stored type once emitted, rather than rescanning the literals.
    if (auto it = typeDecls.find(canon_type.node); it != typeDecls.end())
      return it->second.getMlirType();

    // Width covers the range of representation values, not the literal count:
    // character types (@rm{3-5-2}) materialize only their referenced literals,
    // and representation clauses (@rm{13-4}) can assign values beyond the
    // count.
    ada_node literals;
    ada_enum_type_def_f_enum_literals(&type_def.value(), &literals);
    unsigned count = ada_node_children_count(&literals);
    bool isChar = isCharacterType(canon_type);
    int64_t minRep = 0, maxRep = 0;
    for (unsigned i = 0; i < count; ++i) {
      ada_node lit;
      if (ada_node_child(&literals, i, &lit) == 0)
        continue;
      std::optional<int64_t> rep =
          isChar ? charLiteralRep(lit, diagLoc) : enumLiteralRep(lit, diagLoc);
      if (!rep)
        return mlir::Type{};
      minRep = std::min(minRep, *rep);
      maxRep = std::max(maxRep, *rep);
    }
    // Character types size to their kind, from the max *referenced* code point
    // (see charLiteralRep), not the type's true upper bound; reps are
    // non-negative.
    // @todo i16/i32 are unreachable: no Libadalang route yields a code point
    //       above Latin-1 (literal resolution, bracket `p_denoted_value`, and
    //       UTF-8 lexing all fail). Referenced-literal sizing also diverges
    //       from GNAT's fixed 16/32-bit wide characters.
    if (isChar) {
      if (maxRep <= 255)
        return builder.getIntegerType(8);
      if (maxRep <= 65535)
        return builder.getIntegerType(16);
      return builder.getIntegerType(32);
    }
    // Ordinary enum: smallest *signed* width holding minRep .. maxRep (MLIR
    // extends signless integers as signed). Boolean and single-literal enums
    // stay i1.
    if (minRep >= 0 && maxRep <= 1)
      return builder.getIntegerType(1);
    unsigned bits = std::max(
        llvm::APInt(64, minRep, /*isSigned=*/true).getSignificantBits(),
        llvm::APInt(64, maxRep, /*isSigned=*/true).getSignificantBits());
    return builder.getIntegerType(bits);
  }

  /// Array type (@rm{3-6}): reuse the `!ada.array` layout built at
  /// declaration. Nullopt if not an array.
  std::optional<mlir::Type> getArrayMLIRType(ada_node &canon_type,
                                             mlir::Location diagLoc) {
    if (!libadalang::isArrayTypeDecl(canon_type))
      return std::nullopt;

    // Search for the stored type.
    if (auto it = typeDecls.find(canon_type.node); it != typeDecls.end())
      return it->second.getMlirType();

    mlir::emitError(diagLoc, "array type used before its declaration");
    return mlir::Type{};
  }

  /// Signed integer type (@rm{3-5-4}): width from the base type's (static)
  /// range rather than the predefined name, rounded up to a power-of-two byte
  /// width (i8 .. i128; only enums get tight widths). Universal types skip this
  /// and fall to the name carrier: Libadalang's synthetic Standard gives
  /// universal_int_type_ a placeholder -1 .. 1 range (standing for an infinite
  /// range, only for name resolution). Nullopt when `canon_type` is not a
  /// non-universal integer type.
  std::optional<mlir::Type> getIntegerMLIRType(ada_node &canon_type,
                                               mlir::Location diagLoc) {
    ada_bool is_int = false;
    if (libadalang::isUniversalTypeDecl(canon_type) ||
        !ada_base_type_decl_p_is_int_type(&canon_type, &libadalang::kNullOrigin,
                                          &is_int) ||
        !is_int)
      return std::nullopt;
    ada_internal_discrete_range range;
    if (!ada_base_type_decl_p_discrete_range(&canon_type, &range) ||
        ada_node_is_null(&range.low_bound) ||
        ada_node_is_null(&range.high_bound))
      return std::nullopt;
    std::optional<llvm::APInt> lo = libadalang::evalExprAsInt(range.low_bound);
    std::optional<llvm::APInt> hi = libadalang::evalExprAsInt(range.high_bound);
    if (!lo || !hi) {
      mlir::emitError(diagLoc, "failed to evaluate integer type bounds");
      return mlir::Type{};
    }
    unsigned bits =
        std::max(lo->getSignificantBits(), hi->getSignificantBits());
    if (bits > 128) {
      mlir::emitError(diagLoc, "unsupported integer type wider than 128 bits");
      return mlir::Type{};
    }
    return builder.getIntegerType(llvm::bit_ceil(std::max(bits, 8u)));
  }

  /// Floating-point type (@rm{3-5-7}): width from the declared digits rather
  /// than the predefined name (<= 6 f32, <= 15 f64, <= 18 f80 x86-extended,
  /// else f128, which is unreachable while evalFloatDigits caps digits at 18
  /// but kept for a larger System.Max_Digits). Universal real falls to the name
  /// carrier.
  /// Nullopt when `canon_type` is not a non-universal float type.
  std::optional<mlir::Type> getFloatMLIRType(ada_node &canon_type,
                                             mlir::Location diagLoc) {
    ada_bool is_float = false;
    if (libadalang::isUniversalTypeDecl(canon_type) ||
        !ada_base_type_decl_p_is_float_type(
            &canon_type, &libadalang::kNullOrigin, &is_float) ||
        !is_float)
      return std::nullopt;
    std::optional<uint32_t> digits = evalFloatDigits(canon_type, diagLoc);
    if (!digits)
      return mlir::Type{};
    if (*digits == 0) {
      mlir::emitError(diagLoc, "floating-point type without digits");
      return mlir::Type{};
    }
    if (*digits <= 6)
      return builder.getF32Type();
    if (*digits <= 15)
      return builder.getF64Type();
    if (*digits <= 18)
      return mlir::Float80Type::get(builder.getContext());
    return mlir::Float128Type::get(builder.getContext());
  }

  /// Terminal fallback: map the type by its (lower-cased) defining name. Only
  /// the universal types resolve here, to deliberately wide carriers
  /// (unbounded/exact conceptually, and not reading like machine types). Values
  /// are still bounded by the literal path (int64; reals text-parsed to
  /// double); revisit with the APInt literal work. Any other name is
  /// unsupported (a diagnosed error).
  mlir::Type getUniversalMLIRType(ada_node &canon_type,
                                  mlir::Location diagLoc) {
    ada_node type_name;
    if (!ada_base_type_decl_f_name(&canon_type, &type_name) ||
        ada_node_is_null(&type_name)) {
      // Only anonymous types (an inline `array (...) of ...`, an anonymous
      // access type, ...) are nameless. Report the unsupported construct, not
      // the internal name-lookup failure.
      const char *what = "anonymous types";
      ada_node_kind_enum kind = ada_node_kind(&canon_type);
      if (kind == ada_concrete_type_decl || kind == ada_anonymous_type_decl) {
        if (libadalang::typeDefOfKind(canon_type, ada_array_type_def))
          what = "array types";
      }
      mlir::emitError(diagLoc, what) << " are not yet supported";
      return {};
    }

    std::string name = libadalang::getName(&type_name);
    if (name == libadalang::kUniversalIntTypeName)
      return builder.getIntegerType(512);
    if (name == libadalang::kUniversalRealTypeName)
      return mlir::Float128Type::get(builder.getContext());

    mlir::emitError(diagLoc, "unsupported Ada type '") << name << "'";
    return {};
  }

  mlir::MemRefType getMLIRMemRefType(ada_node &type_expr) {
    mlir::ada::QualType elementType = getAdaQualType(type_expr);
    if (!elementType)
      return {};
    return mlir::MemRefType::get({}, elementType);
  }
};

} // namespace

namespace lalvm {

mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &compilationUnit) {
  return MLIRGenImpl(context).mlirGen(compilationUnit);
}

} // namespace lalvm
