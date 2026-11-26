// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2024-2026 The LALVM Project

#ifndef LALVM_MLIR_MLIRGENIMPL_H
#define LALVM_MLIR_MLIRGENIMPL_H

#include "ada/Dialect.h"
#include "frontend/AST.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace lalvm::mlirgen {

/// DenseMapInfo for ada_node: identity is the raw node pointer; entity info
/// (rebindings) is ignored for map purposes.
struct AdaNodeDenseMapInfo {
  static unsigned getHashValue(const ada_node &n) {
    return llvm::DenseMapInfo<ada_base_node>::getHashValue(n.node);
  }
  static bool isEqual(const ada_node &a, const ada_node &b) {
    return a.node == b.node;
  }
};

/// Shared implementation of Ada IR generation.
class MLIRGenImpl {
public:
  MLIRGenImpl(mlir::MLIRContext &context) : builder(&context) {}

  /// Public API: convert an Ada compilation unit to an MLIR module.
  mlir::ModuleOp mlirGen(ada_node &compilationUnit);

private:
  /// The MLIR module being built. In MLIR parlance, a module is the top-level
  /// container for operations; it maps to an Ada compilation unit.
  mlir::ModuleOp adaModule;

  /// Insertion-point cursor: tracks where the next MLIR operation is emitted.
  mlir::OpBuilder builder;

  mlir::StringAttr getNameAttr(ada_node &node);

  /// Attach an Ada source name to `op`. The Ada type symbol is carried in the
  /// op's ada.qual result type and will be re-encoded into the loc by
  /// LowerToLLVM patterns so AdaDebugInfoPass can recover it post-conversion.
  void setAdaNameLoc(mlir::Operation *op, mlir::StringAttr name);

  /// Overload for values: handles block arguments (which have no defining op)
  /// as well as op results.
  void setAdaNameLoc(mlir::Value val, mlir::StringAttr name);

  /// Overload with explicit source location: replaces the value's existing
  /// location entirely rather than wrapping it.
  void setAdaNameLoc(mlir::Value val, mlir::StringAttr name,
                     mlir::Location srcLoc);

  /// Return the location to use for a load on `ptr`, with `srcLoc` as the
  /// inner source location. Propagates the variable name from `ptr.getLoc()`
  /// if it is a NameLoc; otherwise returns `srcLoc` unchanged.
  mlir::Location propagateAdaNameLoc(mlir::Value ptr, mlir::Location srcLoc);

  /// Verify that the file name matches the Ada unit name and emit a warning
  /// if it does not.
  ///
  /// @todo Does not support child units (e.g. `Parent.Child`): only the
  ///       last element of the FQN is checked against the file stem.
  void verifyUnitFileName(ada_node &node);

  // Maps each DefiningName node to its SSA Value, keyed by ada_base_node
  // pointer (Libadalang's unique node identity). Node identity rather than name
  // strings ensures correct resolution under name shadowing.
  //
  // Mutable objects and writable formals map to semantic references. Constants,
  // input formals, and loop parameters map to values. Libadalang rejects names
  // outside their scope before MLIRGen runs.
  llvm::DenseMap<ada_base_node, mlir::Value> declValues;

  // Named numbers (@rm{3-3-2}): store exact integer values and representable
  // constants for debug info. Uses convert the constant or materialize the
  // exact value in their contextual type. Evaluation failures are diagnosed at
  // use.
  struct NamedNumber {
    std::optional<llvm::APSInt> integer;
    mlir::Value value;
  };
  llvm::DenseMap<ada_node, NamedNumber, AdaNodeDenseMapInfo> numberDecls;

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

  // The `ada.decls` block of the declarative part being emitted, or null when
  // it has none: where emitAnonymousSubtype puts an object's subtype.
  mlir::Block *currentDeclsBlock = nullptr;

  // The `ada.range` elaborated for each dynamic subtype, by symbol (see
  // elaborateDynamicRange). Ada visibility guarantees it dominates every use.
  llvm::DenseMap<mlir::Attribute, mlir::Value> dynamicRanges;

  // Fallback type for a universal operand of an explicit conversion.
  ada_node universalOperandType{};

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
  llvm::DenseMap<mlir::Operation *, llvm::SmallVector<ada_node>> subpFormals;

  // Share each attribute's mapping across uses of the same base enumeration.
  llvm::DenseMap<mlir::Operation *, llvm::StringMap<mlir::ada::SubpOp>>
      enumAttributeFunctions;

  /// Return the canonical defining name for a defining-name node, collapsing
  /// spec/body and partial/full views. Falls back to the input on failure.
  ada_node canonicalDefName(ada_node defName);

  /// Return `candidate` if unused, else `candidate__2`, `__3`, ... until
  /// unique. Reserves the chosen name.
  std::string makeUnique(std::string candidate);

  /// The qualified name of the current enclosing scope, or "" at module level.
  /// Tracked by the generator rather than read from the IR because a block
  /// leaves no op to recover its scope from.
  std::string scopePrefixForInsertion();

  /// Push `scope` on `scopeStack` until the returned guard is destroyed.
  [[nodiscard]] auto pushScope(std::string scope) {
    scopeStack.push_back(std::move(scope));
    return llvm::scope_exit([this] { scopeStack.pop_back(); });
  }

  /// `simple` qualified by the current enclosing scope, made unique with
  /// `__N` (see makeUnique).
  std::string makeUniqueInScope(llvm::StringRef simple);

  /// Canonical fully-qualified name of a defining name (e.g.
  /// `standard.integer`), falling back to its simple name on failure.
  std::string canonicalFqn(ada_node defName);

  /// Assign (or recall) the unique dialect symbol for a declaration's canonical
  /// defining name. Local declarations get `prefix.simple`, made unique with
  /// `__N`. When `useFqn` is set (predefined/external types emitted via the
  /// lazy lookupOrEmitTypeOp path), the canonical fully-qualified name is used
  /// instead (e.g. `standard.integer`). Keyed on the canonical node so a
  /// subprogram spec and body (or a private type's partial and full view)
  /// share one symbol.
  std::string declareSymbol(ada_node canonDef, llvm::StringRef simple,
                            bool useFqn);

  /// Resolve a reference to its qualified dialect symbol by canonical defining
  /// name. Falls back to the canonical fully-qualified name when the referenced
  /// declaration was never emitted by us (i.e. lives in another unit).
  std::string resolveSymbol(ada_node refDefName);

  /// Helper conversion for a Libadalang AST location to an MLIR location.
  mlir::Location loc(const ada_node &node);

  /// Bind a DefiningName node to a Value in declValues.
  void declare(ada_node &def_name, mlir::Value value);

  /// If `node` is an xref entry point that fails name resolution, emit
  /// libadalang's diagnostics and return failure. The statement walker
  /// (`visit`) and the declaration walker (`emitDecl`) both route through this,
  /// so a resolution error is reported once, at the entry point, before codegen
  /// descends into the unresolved subtree.
  mlir::LogicalResult checkResolution(ada_node &node);

  /// Recursive AST walker. Handles the node kinds we know how to codegen;
  /// everything else is ignored at this level and its children are visited.
  /// Returning early (without visiting children) stops descent into a subtree
  /// (a handler that already walked it, e.g. mlirGenSubpBody visits stmts).
  mlir::LogicalResult visit(ada_node &node);

  /// Resolve a name expression to its bound Value via Libadalang
  /// cross-reference + declValues lookup. Returns a null Value (without
  /// emitting any diagnostic) if the name cannot be resolved or is not bound.
  mlir::Value findVarValue(ada_node &expr);

  /// Resolve a writable actual once, retaining its object identity.
  mlir::Value resolveVarPtr(ada_node &expr);

  /// Whether reading an object here precedes its first initialization or write.
  /// IR is built in source order, so only prior uses are present.
  bool isReadBeforeFirstWrite(mlir::Value val);

  /// Read the nominal type when initialization is established, otherwise its
  /// base.
  mlir::Value readObject(mlir::Value reference, mlir::Location location,
                         bool baseOnly = false);

  /// Resolve a variable, reading its value when it denotes an object.
  mlir::Value mlirGenVariable(ada_node &expr);

  /// Emit the arithmetic or Boolean logical operation of `binop` on its
  /// precomputed operands, of type `resultType`. `op` is its ada_op node
  /// (ada_op_plus, ada_op_and, etc.).
  mlir::Value emitBinOp(ada_node &binop, ada_node &op, mlir::Value lhs,
                        mlir::Value rhs, mlir::Type resultType,
                        bool isInteger = false, bool modular = false);

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
                        mlir::ada::QualType resultType);

  /// Fold a static integer expression to a checked constant (@rm{4-9}). Return
  /// nullopt when it is not handled, or a null value after a diagnostic.
  /// Enumeration values are emitted separately.
  std::optional<mlir::Value> tryEmitStaticIntExpr(ada_node &expr);

  /// Whether an expression or referenced constant uses `/`, `mod` or `rem`.
  static bool involvesDivisionOp(ada_node &expr);

  /// Emit a binary operation.
  mlir::Value mlirGenBinOp(ada_node &binop);

  /// Codegen a unary operation (@rm{4-5-6}) as `ada.unop`. The result has the
  /// operator's type; the operand keeps its subtype.
  mlir::Value mlirGenUnOp(ada_node &unop);

  /// Resolve the type of a literal expression. For universal types
  /// (universal_int_type_ / universal_real_type_), falls back to the expected
  /// type from the surrounding context. Returns a null node on failure.
  ada_node resolveLiteralType(ada_node &node, mlir::Location location);

  /// Resolve a literal/static expression's concrete `ada.qual` type:
  /// `resolveLiteralType` (expression type, falling back to the expected type
  /// for universal types) wrapped via `getAdaQualType`. Returns a null QualType
  /// (diagnostic already emitted) on failure.
  mlir::ada::QualType resolveLiteralQualType(ada_node &node,
                                             mlir::Location location);

  /// Emit an `ada.constant` for an integer value already range-checked by the
  /// caller against its target type. The value is fit to the target width
  /// (`sextOrTrunc`); the bounds check guarantees the value is representable.
  mlir::Value emitIntConstant(const llvm::APInt &value,
                              mlir::ada::QualType type,
                              mlir::Location location);

  /// Emit an ada.constant for a floating-point value. Checks for f32 overflow;
  /// emits a diagnostic and returns nullptr on failure.
  mlir::Value emitRealConstant(double value, mlir::ada::QualType type,
                               mlir::Location location);

  /// Round an exact integer once to a floating point type.
  mlir::Value emitRealConstant(const llvm::APSInt &value,
                               mlir::ada::QualType type,
                               mlir::Location location);

  /// Extract the value of an ada_int_literal node via `p_denoted_value`, as an
  /// APSInt (exact at any width). Returns nullopt and emits a diagnostic on
  /// failure.
  std::optional<llvm::APSInt> evalIntLiteral(ada_node &node);

  /// Static integer bounds of a subtype, including inherited bounds, or
  /// nullopt when unavailable. APSInts preserve the bounds at any width.
  std::optional<std::pair<llvm::APSInt, llvm::APSInt>>
  subtypeStaticBounds(ada_node &type_decl, mlir::Location location);

  /// A static value outside the base range of its type is illegal
  /// (@rm{4-9}(35)): emit the two errors and return true.
  bool diagnoseOutOfBaseRange(const llvm::APSInt &value, const llvm::APSInt &lo,
                              const llvm::APSInt &hi, llvm::StringRef typeName,
                              mlir::Location location);

  /// A static value inside the base range but outside the static range of a
  /// subtype is legal, but its range check fails (@rm{11-5}): warn, as GNAT
  /// does, and return true so the caller keeps the run-time check.
  bool warnOutOfSubtypeRange(const llvm::APSInt &value, const llvm::APSInt &lo,
                             const llvm::APSInt &hi, llvm::StringRef typeName,
                             mlir::Location location);

  /// Check a static integer against its resolved base range (@rm{4-9}).
  ada_node checkedIntegerBase(const llvm::APSInt &value, ada_node type_decl,
                              mlir::Location location);

  /// Emit an integer in its contextual type. Reject values outside the base
  /// range. Literals have a type, not a subtype (@rm{8-6}(22)), so emit the
  /// base value before applying subtype checks (@rm{5-2}). Floating point
  /// contexts round once. Return null after a diagnostic.
  mlir::Value emitCheckedIntConstant(const llvm::APSInt &value,
                                     ada_node type_decl,
                                     mlir::Location location);

  mlir::Value mlirGenIntLiteral(ada_node &node);

  /// Extract the floating-point value of an ada_real_literal node via its
  /// source text. Ada allows underscores as digit separators; they are
  /// stripped before parsing. Returns nullopt and emits a diagnostic on
  /// failure.
  std::optional<double> evalRealLiteral(ada_node &node);

  mlir::Value mlirGenRealLiteral(ada_node &node);

  /// Build the `ada.qual<mlirType, @sym>` type pairing `mlirType` with the
  /// Ada type identity of `typeOp` (its symbol name).
  mlir::ada::QualType qualTypeFor(mlir::ada::TypeOp typeOp,
                                  mlir::Type mlirType);

  /// Build the `ada.qual<mlirType, @adaSym>` type for a type declaration.
  /// Returns a null type if the declaration is not supported.
  mlir::ada::QualType getAdaQualType(ada_node &type_decl,
                                     mlir::Location location);

  /// Build the `ada.qual` type from a type expression (SubtypeIndication).
  mlir::ada::QualType getAdaQualType(ada_node &type_expr);

  /// Convert the value `val` to `target` (@rm{4-6}) with an `ada.convert`,
  /// which LowerAdaConversions expands into its checks and casts. `kind`
  /// records whether the conversion is written in the source or applied by
  /// the language. A target with a dynamic bound takes the descriptor
  /// elaborated at its declaration. A constant outside a static subtype warns
  /// that its check raises (@rm{11-5}). Returns null after a diagnostic.
  ///
  /// @todo View conversions of tagged objects (@rm{4-6}(5/2)).
  mlir::Value emitConversion(mlir::Value val, mlir::ada::QualType target,
                             mlir::ada::AdaConversionKind kind,
                             mlir::Location location);

  /// Convert `val` to `target` with an implicit conversion (see
  /// emitConversion), unless it already has that type. Returns null after a
  /// diagnostic.
  mlir::Value convertImplicitly(mlir::Value val, mlir::ada::QualType target,
                                mlir::Location location);

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
                                       mlir::Location location);

  /// Whether `typeOp` is a modular type or a subtype of one (@rm{3-5-4}).
  bool isModularTypeOp(mlir::ada::TypeOp typeOp);

  /// Emit an enum literal as its integer representation (Ada @rm{13-4}).
  /// The rep value and MLIR type are read from the `ada.type` op metadata;
  /// Libadalang is only consulted for the literal name and the enclosing type.
  ///
  /// @param enumLitDecl `EnumLiteralDecl` for the literal; supplies the
  ///                    canonical name used to look up the rep value in the
  ///                    `ada.type` op metadata.
  /// @param useExpr     Use-site expression node; provides the source location.
  mlir::Value mlirGenEnumLit(ada_node &enumLitDecl, ada_node &useExpr);

  /// Emit a subprogram call from an ada_identifier (no-arg) or ada_call_expr
  /// (with args) node. Returns the CallOp on success, nullptr on error.
  /// For function calls the op has one result; for procedure calls none.
  mlir::ada::CallOp mlirGenCallExpr(ada_node &call);

  /// Emit a function call used in expression context, returning its result.
  /// Errors if the callee is a procedure (no result).
  mlir::Value mlirGenCallExprValue(ada_node &call);

  /// Emit a numeric type conversion `T (X)` (@rm{4-6}), folding static
  /// integers.
  mlir::Value mlirGenTypeConversion(ada_node &conversion);

  /// Return the component reference selected by an Ada index value.
  mlir::Value indexedElementRef(ada_node &index);

  /// Emit an array index result.
  mlir::Value mlirGenIndexValue(ada_node &index);

  /// Map a representation code to a position or a neighboring literal.
  mlir::Value mapEnumAttribute(llvm::StringRef name, mlir::Value value,
                               mlir::ada::TypeOp root,
                               mlir::ada::EnumTypeInfoAttr info,
                               mlir::ada::QualType resultType,
                               mlir::Location location);

  /// Check the endpoint before stepping in the enumeration's representation.
  mlir::Value stepEnumeration(ada_node &expr, mlir::Value value,
                              mlir::ada::QualType type,
                              mlir::ada::EnumTypeInfoAttr info, bool character,
                              bool successor);

  mlir::ada::QualType getPositionType(ada_node &expr, mlir::Location location);

  /// Emit 'Pos, 'Val, 'Succ and 'Pred for discrete types (@rm{3-5-5},
  /// @rm{3-5}).
  /// Libadalang does not provide the correct bounds for predefined character
  /// types'First and 'Last. Consequently, 'Succ and 'Pred use the 8 bit
  /// representation bounds, and Wide_Character and Wide_Wide_Character
  /// positions above 255 are unsupported.
  mlir::Value mlirGenDiscreteAttribute(ada_node &expr, ada_node &subtype,
                                       mlir::ada::TypeOp typeOp, ada_node &args,
                                       llvm::StringRef name);

  /// Emit scalar attributes (@rm{3-5}). Static bounds are constants, while
  /// dynamic bounds come from the subtype's elaborated range descriptor.
  mlir::Value mlirGenAttributeRef(ada_node &expr);

  /// Codegen an expression node. Returns the SSA Value for the result, or
  /// nullptr on failure (unsupported expression kind or codegen error).
  mlir::Value visit_expr(ada_node &expr);

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
  /// **Implementation Details**: Store exact integers and representable
  /// universal constants in `numberDecls`. Uses convert the constant or
  /// materialize the exact integer in the required type. Real values retain a
  /// universal constant. Diagnose evaluation failures at use.
  mlir::LogicalResult mlirGenNumberDecl(ada_node &number_decl);

  /// True if `type_decl` is a character type (@rm{3-5-2}): an enumeration whose
  /// literals are character literals. Callers use this once per type to choose
  /// between `enumLiteralRep` and `charLiteralRep` without re-checking each
  /// literal.
  static bool isCharacterType(ada_node type_decl);

  /// Representation value of an ordinary enum literal: its `p_enum_rep`
  /// (honoring @rm{13-4} representation clauses). Returns nullopt and emits a
  /// diagnostic on failure. For a character type use `charLiteralRep`.
  ///
  /// @todo Carry the rep as an `llvm::APInt`, not `int64_t`: support the full
  ///       `System.Min_Int .. System.Max_Int` range (@rm{13-4}), not just i64.
  ///       `EnumTypeInfoAttr` would then store APInt-valued codes, like the
  ///       universal-int work.
  /// @todo @rm{13-4} requires the codes to be distinct and to satisfy the
  ///       type's predefined ordering; not checked yet. Ordering compares
  ///       codes (signed once one is negative, see `isUnsignedOrderQualType`).
  std::optional<int64_t> enumLiteralRep(ada_node &lit, mlir::Location location);

  /// Representation value of a character enum literal (@rm{3-5-2}): the Latin-1
  /// code point from the wrapped `CharLiteral`'s `p_denoted_value`.
  /// `p_enum_rep` is unusable here because Libadalang materializes only the
  /// referenced Character literals, so it would give a wrong position. Returns
  /// nullopt and emits a diagnostic on failure.
  std::optional<int64_t> charLiteralRep(ada_node &lit, mlir::Location location);

  /// Whether mlirGenTypeDecl handles this declaration: numeric, universal,
  /// enumeration, or array types, or subtypes thereof (by canonical type). The
  /// others are skipped, not rejected, and fail only when referenced.
  bool isSupportedTypeDecl(ada_node &decl);

  /// Build a type declaration's unique dialect symbol (see declareSymbol).
  /// Emits a diagnostic and fails when the declaration has no name.
  llvm::FailureOr<std::string> resolveTypeDeclName(ada_node &type_decl,
                                                   bool external);

  /// The `type_info` of a subtype of `baseOp` constrained to `[lo, hi]`
  /// (@rm{3-2-2}), or null when unconstrained (`lo` or `hi` null). An
  /// unresolvable bound is dynamic (`?`).
  ///
  /// @todo Record float subtype constraints once float_info models bounds.
  mlir::Attribute subtypeInfo(mlir::ada::TypeOp baseOp, ada_node lo,
                              ada_node hi);

  /// Create a subtype of the immediate `parent`, constrained to
  /// `[lo, hi]` (see subtypeInfo) at the insertion point.
  mlir::ada::TypeOp createSubtypeOp(llvm::StringRef name,
                                    mlir::ada::TypeOp parent, ada_node lo,
                                    ada_node hi, mlir::Location location);

  /// Elaborate the dynamic range of `typeOp`, constrained
  /// to `[lo, hi]`, at the insertion point (@rm{3-2-2}(10)): evaluate its
  /// bounds once and emit the `ada.range` that `findDynamicRange` recovers at
  /// each check site. Evaluating the bounds here also runs their side effects
  /// exactly once, even if the subtype is never referenced.
  void elaborateDynamicRange(mlir::ada::TypeOp typeOp, ada_node lo, ada_node hi,
                             mlir::Location location);

  /// Emit the anonymous nominal subtype of object `id`, a subtype of
  /// `markDecl`'s type constrained to `[lo, hi]` (@rm{3-3-1}(8)). Returns null
  /// on failure (a diagnostic is emitted).
  mlir::ada::TypeOp emitAnonymousSubtype(ada_node &id, ada_node &markDecl,
                                         ada_node lo, ada_node hi,
                                         mlir::Location location);

  /// Emit the ada.type for a subtype declaration (@rm{3-2-2}): representation
  /// comes from the canonical type and the subtype link names the immediate
  /// subtype mark. Metadata records the declaration's own constraint.
  mlir::LogicalResult mlirGenSubtypeDecl(ada_node &type_decl, bool external);

  /// The `ada.range` elaborated for the dynamic subtype `sym` (see
  /// elaborateDynamicRange), or null. A nested subprogram may reference an
  /// outer dynamic subtype: the range is then an up-level reference that
  /// ClosureConversion lifts into a parameter.
  mlir::Value findDynamicRange(mlir::FlatSymbolRefAttr sym);

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
                                      bool external = false);

  /// Emit a universal or numeric type declaration as an `ada.type` carrying
  /// integer/float `type_info` (and, for a subtype, a link to its base).
  mlir::LogicalResult mlirGenNumericTypeDecl(ada_node &type_decl,
                                             bool external);

  /// Emit a statically-constrained 1-D array type declaration (@rm{3-6}) as an
  /// `ada.type` whose `mlir_type` is the `!ada.array` layout and whose metadata
  /// is `array_info`.
  mlir::LogicalResult mlirGenArrayTypeDecl(ada_node &type_decl, bool external);

  /// Emit an enumeration type declaration (@rm{3-5-1}) as an `ada.type`
  /// carrying `enum_info` (enumerator names and representation values). Fails
  /// when the declaration is not a supported enum kind.
  mlir::LogicalResult mlirGenEnumTypeDecl(ada_node &type_decl, bool external);

  /// Emit an object declaration (@rm{3-3-1}) with its explicit or default
  /// initialization. The subtype indication gives the object's nominal subtype
  /// (a range constraint in it gives each object its own anonymous subtype, see
  /// emitAnonymousSubtype), so its initializer and assignments are range
  /// checked.
  ///
  /// @todo Only a subtype indication is supported: no access definition, array
  /// type definition, single task or protected declaration, deferred constant
  /// or array initializer.
  mlir::LogicalResult mlirGenObjectDecl(ada_node &object_decl);

  /// Emit declarations from a declarative part.
  /// Supported: ObjectDecl (not at library level), NumberDecl, the supported
  ///            ConcreteTypeDecl and SubtypeDecl, SubpBody, NullSubpDecl and
  ///            ExprFunction (nested subprograms).
  /// Skipped: SubpDecl (forward declarations), EnumRepClause (queried when
  ///          needed), and unsupported type declarations. Anything else is an
  ///          error.
  /// The AST structure is: DeclarativePart -> AdaNodeList -> decl...
  ///
  /// At library level, the insertion block is the module body, a SymbolTable:
  /// every declaration is emitted inline there. Elsewhere, the `ada.subp` and
  /// `ada.type` symbols are routed into an interior `ada.decls` (a
  /// SymbolTable), since neither the enclosing `ada.subp` nor a dissolved block
  /// is one; locals (objects, numbers) stay inline in the region. Declarations
  /// are emitted in source order, with the `ada.decls` placed *after* the
  /// locals: each local lands in the region just before the `ada.decls`, each
  /// symbol inside it. So a local dominates the `ada.decls` (required once a
  /// nested subprogram captures it) and is bound before a later nested
  /// subprogram is emitted, while a symbol is still available by name to the
  /// locals around it.
  mlir::LogicalResult mlirGenDeclarativePart(ada_node &decls);

  /// Lower one Ada subprogram body to an `ada.subp` operation. This is the main
  /// codegen entry point for a subprogram: it creates the `ada.subp` op, binds
  /// argument SSA values in the symbol table, then walks the statement list to
  /// emit the body.
  mlir::Operation *mlirGenSubpBody(ada_node &subp_body);

  /// Fuse each label recorded in `pendingLabels` as a `DILabelRef` marker onto
  /// its anchor op, keeping the op's own loc as the fused inner.
  void anchorPendingLabels();

  /// Lower an Ada block statement (ada_begin_block or ada_decl_block). The
  /// block dissolves into the enclosing region: its locals and statements are
  /// emitted inline and its nested symbols into an in-place `ada.decls`. A
  /// unique scope segment is pushed so the block's declarations get a qualified
  /// name that reflects the block scope, which no longer exists as an op.
  mlir::LogicalResult mlirGenBlock(ada_node &blockNode, llvm::StringRef name);

  /// Lower a package declaration by visiting the public and private declaration
  /// parts.
  mlir::LogicalResult mlirGenPackageDecl(ada_node &decl);

  /// Lower a package body by visiting its declarative part (after having
  /// visited its declaration part).
  ///
  /// @todo Add support for the sequence of statements to execute at elaboration
  /// time (@rm{7-2}).
  mlir::LogicalResult mlirGenPackageBody(ada_node &body);

  /// Unwrap an Ada-qualified value (`!ada.qual<T, @...>`) to its underlying
  /// builtin MLIR type `T`, e.g. to feed a `cf`/`arith` op that needs the raw
  /// type. Emits a diagnostic and returns null if `value` is not qualified.
  ///
  /// Unwrapping discards the Ada type identity, so the op's location is a
  /// `FusedLoc` carrying the type reference (the same `DITypeRefAttr`
  /// LowerToLLVM threads through value locations), keeping the type
  /// discoverable for debug info.
  mlir::Value unwrap(mlir::Value value, mlir::Location location);

  /// Emit the Boolean condition `cond` (@rm{5-3}, @rm{4-5-7}) as an `i1`.
  /// Returns null after a diagnostic.
  mlir::Value mlirGenCondition(ada_node &cond);

  /// True if the current insertion block ends in a terminator (e.g. a
  /// `return`). False for an empty or absent block.
  bool currentBlockTerminated();

  /// Append a branch to `mergeBlock` if the current insertion block is not
  /// already terminated (e.g. by a `return`).
  void branchToMergeIfOpen(mlir::Block *mergeBlock, mlir::Location location);

  /// If the merge is unreachable, keep a terminated insertion block so the
  /// next statement is diagnosed as unreachable.
  void continueAtMerge(mlir::Block *merge, mlir::Block *fallback);

  /// The CFG block a goto label marks (@rm{5-8}), created on first reference
  /// and appended to the current subprogram region, so a forward `goto` and its
  /// later `<<label>>` agree on one block. Leaves the insertion point
  /// unchanged.
  mlir::Block *getOrCreateLabelBlock(ada_node defName);

  /// Emit a `goto L` (@rm{5-8}): branch to L's block. The current block is now
  /// terminated; statements up to the next label are unreachable.
  mlir::LogicalResult mlirGenGoto(ada_node &goto_stmt);

  /// Emit a `<<L>>` label (@rm{5-8}): fall through into L's block (when the
  /// prior statement left the flow open) and resume emission there.
  mlir::LogicalResult mlirGenLabel(ada_node &label);

  /// A guard of an if statement or expression: a condition and the
  /// statements or expression it selects.
  struct IfGuard {
    ada_node condition;
    ada_node thenPart;
  };

  /// The guards of the if statement or expression `ifNode`: the `if` plus each
  /// `elsif`, in order. Returns std::nullopt after a diagnostic.
  std::optional<llvm::SmallVector<IfGuard, 4>>
  collectIfGuards(ada_node &ifNode);

  /// Emit an if statement (@rm{5-3}) as an unstructured CFG. Each guard (the
  /// `if` condition and every `elsif`) branches to its then-block or to the
  /// next test; the last guard's false edge goes to the else-block, or to the
  /// merge block when there is no else. Elsif conditions are evaluated in their
  /// own blocks, so a condition is tested only when all earlier ones were false
  /// (@rm{5-3}). A branch that does not already terminate (e.g. via `return`)
  /// falls through to the merge block. If every path terminates, the merge
  /// block is unreachable and is erased.
  mlir::LogicalResult mlirGenIf(ada_node &if_stmt);

  /// Emit a loop statement (@rm{5-5}) as an unstructured CFG. `while C` gets a
  /// header that tests C and branches to the body or the merge; a bare `loop`
  /// has none, its body branching to itself so `exit` is the only way out. The
  /// merge block and source name are pushed on `loopStack` for `exit`. A `for`
  /// loop is delegated to `mlirGenForLoop`.
  mlir::LogicalResult mlirGenLoop(ada_node &loopNode, llvm::StringRef name);

  /// Emit a `for` loop over an explicit discrete range (@rm{5-5}), forward or
  /// reverse. Its readonly parameter is an SSA block argument. The final value
  /// is tested before stepping, so the step cannot overflow. Empty ranges
  /// execute no iterations.
  ///
  /// @todo Support the currently diagnosed forms: container iteration, iterator
  ///       filters, a range given by a type or subtype name, and non-integer
  ///       (enumeration or real) ranges.
  mlir::LogicalResult mlirGenForLoop(ada_node &loopNode, ada_node &spec,
                                     llvm::StringRef name);

  /// Emit an `exit` statement (@rm{5-7}): branch to the target loop's merge,
  /// found on `loopStack` (innermost, or by name for `exit Loop_Name`). `exit
  /// when C` branches to the merge when C holds, else continues the body.
  mlir::LogicalResult mlirGenExit(ada_node &exit_stmt);

  /// Resolve the Boolean type declaration in scope of `node` via `p_bool_type`
  /// (the carrier for synthesized Booleans and comparison results). Emits a
  /// diagnostic and returns a null node on failure.
  ada_node resolveBooleanTypeDecl(ada_node &node, mlir::Location location);

  /// Synthesize an `ada.constant` for Boolean `True`. The Boolean type is
  /// resolved with `p_bool_type`; the literal's value is read from the
  /// `enum_info` metadata on the resolved `ada.type` (`enumRep`), keeping the
  /// rep mapping single-sourced rather than hardcoding 1.
  ///
  /// @param context_node Any node, used to reach the analysis unit for the
  ///                     static `p_bool_type` query.
  /// @param location     MLIR location for the constant and diagnostics.
  mlir::Value synthesizeBooleanTrue(ada_node &context_node,
                                    mlir::Location location);

  /// Emit an if expression (@rm{4-5-7}) as a (possibly nested) `scf.if` that
  /// yields the expression's value. The expected type T is applied to every
  /// dependent_expression (@rm{4-5-7}(8/3)), so each branch value is converted
  /// to T before being yielded. `elsif` parts nest as an `scf.if` in the else
  /// region; conditions are thus tested in order, the first True winning
  /// (@rm{4-5-7}(20/3)). When the `else` part is absent the if expression is of
  /// a boolean type and the missing else yields `True` (@rm{4-5-7}(18/3,
  /// 20/3)).
  mlir::Value mlirGenIfExpr(ada_node &if_expr);

  /// Recursively emit the `scf.if` for guard `idx`; the else region holds the
  /// next guard's `scf.if`, the explicit else value, or a synthesized `True`
  /// for a missing else (boolean if expression). See `mlirGenIfExpr`.
  mlir::Value emitIfExprChain(llvm::ArrayRef<IfGuard> guards, unsigned idx,
                              ada_node elseExpr, bool hasElse,
                              mlir::ada::QualType resultType,
                              mlir::Location location);

  /// A subprogram parameter: its defining identifier, mode and type
  /// expression.
  struct SubpParam {
    ada_node id;
    ada_node_kind_enum mode;
    ada_node typeExpr;
  };

  /// The parameters of `subp_spec`, one per defining identifier, in order.
  /// Returns std::nullopt after a diagnostic.
  std::optional<llvm::SmallVector<SubpParam, 4>>
  subpParams(ada_node &subp_spec);

  /// Create an ada.subp with the signature derived from the Ada subprogram
  /// spec and its `params`. Returns nullptr on failure.
  mlir::ada::SubpOp mlirGenSubpSpec(ada_node &subp_spec,
                                    std::optional<ada_node> resultType,
                                    llvm::ArrayRef<SubpParam> params);

  /// Emit a procedure call statement (@rm{6-4}).
  /// @todo Support entry calls.
  /// @todo Check that the returned value of mlirGenCallExpr is not a function.
  mlir::LogicalResult mlirGenCallStmt(ada_node &call_stmt);

  mlir::LogicalResult mlirGenAssign(ada_node &assign_stmt);

  /// Emit a return statement's expression or an expression function's body,
  /// converted to the result subtype. A procedure's return has no value.
  mlir::FailureOr<mlir::Value> mlirGenReturnExpr(ada_node &node,
                                                 mlir::ada::SubpOp subp);

  /// Emit a return operation or a failure if any generation fails.
  mlir::LogicalResult mlirGenReturn(ada_node &return_stmt);

  /// `value` as a type info bound or modulus (see
  /// `mlir::ada::minimalWidthIntAttr`).
  mlir::IntegerAttr minimalWidthIntAttr(const llvm::APInt &value);

  /// One range bound as type info metadata: minimalWidthIntAttr of its
  /// static value, or UnitAttr (`?`) when it is missing or not static.
  mlir::Attribute rangeBoundAttr(ada_node &bound);

  /// Evaluate the decimal digits of a floating-point type declaration
  /// (@rm{3-5-7}). The C API exposes no semantic digits property, so the
  /// floating_point_def's num_digits expression is evaluated (static by
  /// definition). Returns 0 when the declaration has no floating_point_def
  /// (universal_real); nullopt, after emitting a diagnostic, on evaluation
  /// failure or when the value exceeds 18 (System.Max_Digits on x86-64).
  std::optional<uint32_t> evalFloatDigits(ada_node &type_decl,
                                          mlir::Location location);

  /// Map a type declaration node to an MLIR type. Follows the subtype chain
  /// to the canonical base type, derives integer widths from its range and
  /// float widths from its digits, and matches the remaining (universal)
  /// types by name.
  /// diagLoc is the caller's diagnostic site (typically the use location) for
  /// any error the mapping emits.
  mlir::Type getMLIRTypeFromDecl(ada_node &type_decl, mlir::Location diagLoc);

  /// Modular type (@rm{3-5-4}): reuse the width chosen at declaration and
  /// stored on the `TypeOp`. Nullopt if not modular.
  std::optional<mlir::Type> getModularMLIRType(ada_node &canon_type,
                                               mlir::Location diagLoc);

  /// Enumeration type (@rm{3-5-1}). Nullopt if not an enum.
  std::optional<mlir::Type> getEnumMLIRType(ada_node &canon_type,
                                            mlir::Location diagLoc);

  /// Array type (@rm{3-6}): reuse the `!ada.array` layout built at
  /// declaration. Nullopt if not an array.
  std::optional<mlir::Type> getArrayMLIRType(ada_node &canon_type,
                                             mlir::Location diagLoc);

  /// Representation of root_integer and unresolved universal integers.
  mlir::IntegerType getRuntimeIntegerType();

  /// Base range of root_integer and unresolved universal integers.
  std::pair<llvm::APSInt, llvm::APSInt> getRuntimeIntegerBounds();

  /// Signed integer type (@rm{3-5-4}): width from the base type's (static)
  /// range rather than the predefined name, rounded up to a power-of-two byte
  /// width (i8 .. i128; only enums get tight widths). Universal types skip this
  /// and fall to the name carrier: Libadalang's synthetic Standard gives
  /// universal_int_type_ a placeholder -1 .. 1 range (standing for an infinite
  /// range, only for name resolution). Nullopt when `canon_type` is not a
  /// non-universal integer type.
  std::optional<mlir::Type> getIntegerMLIRType(ada_node &canon_type,
                                               mlir::Location diagLoc);

  /// Floating-point type (@rm{3-5-7}): width from the declared digits rather
  /// than the predefined name (<= 6 f32, <= 15 f64, <= 18 f80 x86-extended,
  /// else f128, which is unreachable while evalFloatDigits caps digits at 18
  /// but kept for a larger System.Max_Digits). Universal real falls to the name
  /// carrier.
  /// Nullopt when `canon_type` is not a non-universal float type.
  std::optional<mlir::Type> getFloatMLIRType(ada_node &canon_type,
                                             mlir::Location diagLoc);

  /// Map unresolved universal types by name. Integers use the runtime
  /// representation, while reals keep a wide carrier. Static integers retain
  /// exact precision until materialization. Real literals are still parsed
  /// through double. Diagnose other types.
  mlir::Type getUniversalMLIRType(ada_node &canon_type, mlir::Location diagLoc);
};

} // namespace lalvm::mlirgen

#endif // LALVM_MLIR_MLIRGENIMPL_H
