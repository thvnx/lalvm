// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2024-2026 The LALVM Project

#include "MLIRGenImpl.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Diagnostics.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/SaveAndRestore.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace libadalang = frontend::libadalang;

namespace lalvm::mlirgen {

mlir::Value MLIRGenImpl::findVarValue(ada_node &expr) {
  ada_node def_name;
  if (!ada_name_p_referenced_defining_name(&expr, /*imprecise_fallback=*/0,
                                           &def_name) ||
      ada_node_is_null(&def_name))
    return {};
  auto it = declValues.find(def_name.node);
  return it != declValues.end() ? it->second : mlir::Value{};
}

mlir::Value MLIRGenImpl::resolveVarPtr(ada_node &expr) {
  mlir::Value val;
  if (ada_node_kind(&expr) == ada_call_expr) {
    ada_call_expr_kind kind{};
    if (ada_call_expr_p_kind(&expr, &kind) &&
        kind == ADA_CALL_EXPR_KIND_ARRAY_INDEX)
      val = indexedElementRef(expr);
  } else {
    val = findVarValue(expr);
  }
  if (!val || !mlir::isa<mlir::ada::RefType>(val.getType())) {
    mlir::emitError(loc(expr),
                    "actual for `in out`/`out` parameter must be a variable");
    return nullptr;
  }
  auto decl = libadalang::referencedDecl(expr);
  if (decl && ada_node_kind(&*decl) == ada_for_loop_var_decl) {
    mlir::emitError(loc(expr), "loop parameter is not a writable actual");
    return nullptr;
  }
  return val;
}

bool MLIRGenImpl::isReadBeforeFirstWrite(mlir::Value val) {
  if (auto object = val.getDefiningOp<mlir::ada::ObjectOp>())
    if (object.getInitial())
      return false;
  if (auto arg = mlir::dyn_cast<mlir::BlockArgument>(val)) {
    auto subp =
        mlir::dyn_cast<mlir::ada::SubpOp>(arg.getOwner()->getParentOp());
    if (!subp || subp.formalNeedsInput(arg.getArgNumber()))
      return false;
  }
  return llvm::none_of(val.getUsers(), [](mlir::Operation *op) {
    return mlir::isa<mlir::ada::WriteOp, mlir::ada::CallOp>(op);
  });
}

mlir::Value MLIRGenImpl::readObject(mlir::Value reference,
                                    mlir::Location location, bool baseOnly) {
  auto qual = mlir::ada::objectValueType(reference.getType());
  bool initialized = false;
  if (auto object = reference.getDefiningOp<mlir::ada::ObjectOp>())
    initialized = object.getInitial() && object.getInitial().getType() == qual;
  if (auto arg = mlir::dyn_cast<mlir::BlockArgument>(reference)) {
    auto subp =
        mlir::dyn_cast<mlir::ada::SubpOp>(arg.getOwner()->getParentOp());
    initialized = subp && subp.getFormalMode(arg.getArgNumber()) ==
                              mlir::ada::AdaParamMode::InOut;
  }
  for (mlir::Operation *user : reference.getUsers())
    if (user->getBlock() == builder.getInsertionBlock() &&
        mlir::isa<mlir::ada::WriteOp, mlir::ada::CallOp>(user))
      initialized = true;
  if (initialized && !baseOnly)
    return mlir::ada::ReadOp::create(builder, location, qual, reference);
  auto type = mlir::ada::baseOfAdaType(
      builder.getInsertionBlock()->getParentOp(), qual);
  if (!type) {
    mlir::emitError(location, "unresolved object base type");
    return {};
  }
  return mlir::ada::ReadOp::create(builder, location, type, reference);
}

mlir::Value MLIRGenImpl::mlirGenVariable(ada_node &expr) {
  if (mlir::Value val = findVarValue(expr)) {
    if (mlir::isa<mlir::ada::RefType>(val.getType())) {
      if (isReadBeforeFirstWrite(val))
        mlir::emitWarning(loc(expr), "variable '")
            << libadalang::getName(&expr, false)
            << "' is read before first assignment";
      return readObject(val, propagateAdaNameLoc(val, loc(expr)));
    }
    return val;
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

mlir::Value MLIRGenImpl::emitBinOp(ada_node &binop, ada_node &op,
                                   mlir::Value lhs, mlir::Value rhs,
                                   mlir::Type resultType, bool isInteger,
                                   bool modular) {
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
  case ada_op_mod:
  case ada_op_rem:
    kind = ada_node_kind(&op) == ada_op_div   ? mlir::ada::AdaBinaryOp::Div
           : ada_node_kind(&op) == ada_op_mod ? mlir::ada::AdaBinaryOp::Mod
                                              : mlir::ada::AdaBinaryOp::Rem;
    if (isInteger) {
      checks = mlir::ada::AdaChecks::Division;
      // Resolve checks for constant divisors, including converted values. A
      // zero divisor is illegal in a static expression (@rm{4-9}(34)).
      // Otherwise warn and retain the runtime check (@rm{4-5-5}(22)).
      mlir::Value divisor = rhs;
      if (auto convert = divisor.getDefiningOp<mlir::ada::ConvertOp>())
        divisor = convert.getInput();
      if (auto constant = divisor.getDefiningOp<mlir::ada::ConstantOp>())
        if (auto value = constant.getIntValue()) {
          if (value.value().isZero()) {
            ada_node right;
            ada_bin_op_f_right(&binop, &right);
            auto divisorLoc = loc(right);
            if (libadalang::isStaticExpr(binop)) {
              mlir::emitError(divisorLoc, "division by zero");
              mlir::emitError(divisorLoc,
                              "static expression fails Constraint_Check");
              return nullptr;
            }
            mlir::emitWarning(divisorLoc, "division by zero");
            mlir::emitWarning(divisorLoc,
                              "Constraint_Error will be raised at run time");
          } else if (modular || kind != mlir::ada::AdaBinaryOp::Div ||
                     !value.value().isAllOnes()) {
            // Only -1 needs an overflow check for signed division.
            checks = mlir::ada::AdaChecks{};
          }
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
  // Omit the attribute when no runtime check is needed.
  auto checksAttr =
      checks != mlir::ada::AdaChecks{}
          ? mlir::ada::AdaChecksAttr::get(builder.getContext(), checks)
          : mlir::ada::AdaChecksAttr{};
  return mlir::ada::BinOp::create(builder, callerLoc, resultType, kind, lhs,
                                  rhs, checksAttr);
}

mlir::Value MLIRGenImpl::emitCmpOp(ada_node &op, mlir::Value lhs,
                                   mlir::Value rhs,
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

std::optional<mlir::Value> MLIRGenImpl::tryEmitStaticIntExpr(ada_node &expr) {
  if (!libadalang::isStaticExpr(expr))
    return std::nullopt;
  // @todo Libadalang 26 confuses enumeration positions and representation
  // codes. Keep executable expressions unfolded until its evaluator is fixed.
  // Named numbers and type bounds still use that evaluator.
  if (libadalang::containsAttribute(expr, {"pos", "val", "succ", "pred"}))
    return std::nullopt;
  mlir::Location location = loc(expr);
  ada_node type_decl = resolveLiteralType(expr, location);
  if (ada_node_is_null(&type_decl))
    return std::nullopt;
  // A predefined operator of a type yields that type, free of any subtype
  // constraint (@rm{4-5}): its root type stands for it.
  type_decl = libadalang::canonicalType(type_decl).value_or(type_decl);
  mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(type_decl, location);
  if (!typeOp)
    return std::nullopt;
  if (!mlir::isa_and_nonnull<mlir::ada::IntegerTypeInfoAttr>(
          typeOp.getTypeInfoAttr())) {
    if (ada_node_kind(&expr) != ada_relation_op)
      return std::nullopt;
    ada_node left{}, right{};
    ada_bin_op_f_left(&expr, &left);
    ada_bin_op_f_right(&expr, &right);
    // Evaluate static universal and root integer comparisons exactly before
    // materialization (@rm{4-9}). Libadalang may resolve universal arithmetic
    // to root_integer.
    for (ada_node operand : {left, right}) {
      auto type = libadalang::expressionType(operand);
      if (!type || (!libadalang::isRootIntegerTypeDecl(type.value()) &&
                    (!libadalang::isUniversalTypeDecl(type.value()) ||
                     libadalang::isRealTypeDecl(type.value()))))
        return std::nullopt;
    }
  }
  // @todo Libadalang 27 evaluates modular arithmetic without wrapping
  //       (@rm{3-5-4}(19)). Keep it unfolded until the evaluator is fixed.
  if (isModularTypeOp(typeOp))
    return std::nullopt;
  // @todo Diagnose all failing static checks (@rm{4-9}). Libadalang 27 cannot
  //       evaluate `mod`, `rem` or division by zero, so emit these normally.
  auto value = libadalang::evalExprAsInt(expr);
  if (!value) {
    if (involvesDivisionOp(expr))
      return std::nullopt;
    mlir::emitError(location, "failed to evaluate static integer expression");
    return mlir::Value(nullptr);
  }
  return mlir::Value(emitCheckedIntConstant(*value, type_decl, location));
}

bool MLIRGenImpl::involvesDivisionOp(ada_node &expr) {
  ada_node_kind_enum k = ada_node_kind(&expr);
  if (k == ada_op_div || k == ada_op_mod || k == ada_op_rem)
    return true;
  if (k == ada_identifier) {
    auto decl = libadalang::referencedDecl(expr);
    ada_node init{};
    if (decl &&
        ((ada_node_kind(&decl.value()) == ada_object_decl &&
          ada_object_decl_f_default_expr(&decl.value(), &init)) ||
         (ada_node_kind(&decl.value()) == ada_number_decl &&
          ada_number_decl_f_expr(&decl.value(), &init))) &&
        !ada_node_is_null(&init) && involvesDivisionOp(init))
      return true;
  }
  unsigned count = ada_node_children_count(&expr);
  for (unsigned i = 0; i < count; ++i) {
    ada_node child{};
    if (ada_node_child(&expr, i, &child) && !ada_node_is_null(&child) &&
        involvesDivisionOp(child))
      return true;
  }
  return false;
}

mlir::Value MLIRGenImpl::mlirGenBinOp(ada_node &binop) {
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
  ada_node type_decl = resolveLiteralType(binop, loc(binop));
  mlir::ada::QualType resultType;
  if (!ada_node_is_null(&type_decl))
    resultType = getAdaQualType(type_decl, loc(binop));

  // Relational operators (@rm{4-5-2}) yield Boolean. Their operands are
  // values of one type, whatever their subtypes: no conversion.
  switch (ada_node_kind(&op)) {
  case ada_op_eq:
  case ada_op_neq:
  case ada_op_lt:
  case ada_op_lte:
  case ada_op_gt:
  case ada_op_gte:
    return emitCmpOp(op, lhs, rhs, resultType);
  default:
    break;
  }

  // Arithmetic (@rm{4-5-3}..@rm{4-5-5}) and Boolean logical (@rm{4-5-1})
  // operators yield the operator's type. Their operands are values of that
  // type, whatever their subtypes: no conversion.
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
  return emitBinOp(binop, op, lhs, rhs,
                   resultType ? mlir::Type(resultType) : lhs.getType(),
                   isInteger, modular);
}

mlir::Value MLIRGenImpl::mlirGenUnOp(ada_node &unop) {
  if (auto folded = tryEmitStaticIntExpr(unop))
    return *folded;

  ada_node operandNode;
  ada_un_op_f_expr(&unop, &operandNode);
  mlir::Value operand = visit_expr(operandNode);
  if (!operand)
    return nullptr;

  ada_node op;
  ada_un_op_f_op(&unop, &op);

  mlir::ada::AdaUnaryOp kind;
  switch (ada_node_kind(&op)) {
  case ada_op_abs:
    kind = mlir::ada::AdaUnaryOp::Abs;
    break;
  case ada_op_minus:
    kind = mlir::ada::AdaUnaryOp::Minus;
    break;
  case ada_op_not:
    kind = mlir::ada::AdaUnaryOp::Not;
    break;
  case ada_op_plus:
    kind = mlir::ada::AdaUnaryOp::Plus;
    break;
  default:
    mlir::emitError(loc(unop), "invalid unary operator '")
        << libadalang::image(&op) << "'";
    return nullptr;
  }

  ada_node type_decl = resolveLiteralType(unop, loc(unop));
  if (ada_node_is_null(&type_decl)) {
    mlir::emitError(loc(unop), "failed to resolve type of unary operation");
    return nullptr;
  }
  // Yield the operand's base type, not its subtype (@rm{4-5-4}, @rm{4-5-6}).
  if (std::optional<ada_node> canon = libadalang::canonicalType(type_decl))
    type_decl = *canon;
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
  if (kind == mlir::ada::AdaUnaryOp::Not && !boolOrModular) {
    mlir::emitError(loc(unop),
                    "operator \"not\" requires a Boolean or modular operand");
    return nullptr;
  }

  // `abs` and `-` on a signed integer overflow on T'Base'First (@rm{4-5}(11),
  // @rm{3-5-4}(9)). @todo Omit the overflow check when the operand is
  // statically known not to be T'Base'First.
  bool isInteger = resultType.getMlirType().isInteger();
  mlir::ada::AdaChecksAttr checks;
  if ((kind == mlir::ada::AdaUnaryOp::Abs ||
       kind == mlir::ada::AdaUnaryOp::Minus) &&
      isInteger && !boolOrModular)
    checks = mlir::ada::AdaChecksAttr::get(builder.getContext(),
                                           mlir::ada::AdaChecks::Overflow);

  // The operand is a value of the type, whatever its subtype: no conversion.
  return mlir::ada::UnOp::create(builder, loc(unop), resultType, kind, operand,
                                 checks);
}

ada_node MLIRGenImpl::resolveLiteralType(ada_node &node,
                                         mlir::Location location) {
  ada_node type_decl = libadalang::expressionType(node).value_or(ada_node{});
  if (ada_node_is_null(&type_decl)) {
    mlir::emitError(location, "failed to resolve type of literal");
    return {};
  }
  if (libadalang::isUniversalTypeDecl(type_decl)) {
    ada_node expected{};
    if (ada_expr_p_expected_expression_type(&node, &expected) &&
        !ada_node_is_null(&expected) &&
        !libadalang::isUniversalTypeDecl(expected))
      return expected;
    if (!ada_node_is_null(&universalOperandType))
      return universalOperandType;
  }
  return type_decl;
}

mlir::ada::QualType
MLIRGenImpl::resolveLiteralQualType(ada_node &node, mlir::Location location) {
  ada_node type_decl = resolveLiteralType(node, location);
  if (ada_node_is_null(&type_decl))
    return {};
  return getAdaQualType(type_decl, location);
}

mlir::Value MLIRGenImpl::emitIntConstant(const llvm::APInt &value,
                                         mlir::ada::QualType type,
                                         mlir::Location location) {
  return mlir::ada::createIntConstant(builder, location, type, value);
}

mlir::Value MLIRGenImpl::emitRealConstant(double value,
                                          mlir::ada::QualType type,
                                          mlir::Location location) {
  auto floatType = mlir::cast<mlir::FloatType>(type.getMlirType());
  if (floatType.getWidth() == 32 && std::isinf(static_cast<float>(value))) {
    mlir::emitError(location, "real constant ")
        << value << " out of range for f32";
    return nullptr;
  }
  return mlir::ada::ConstantOp::create(builder, location, type,
                                       mlir::FloatAttr::get(floatType, value));
}

mlir::Value MLIRGenImpl::emitRealConstant(const llvm::APSInt &value,
                                          mlir::ada::QualType type,
                                          mlir::Location location) {
  auto floatType = mlir::cast<mlir::FloatType>(type.getMlirType());
  llvm::APFloat f(floatType.getFloatSemantics());
  auto status = f.convertFromAPInt(value, value.isSigned(),
                                   llvm::APFloat::rmNearestTiesToEven);
  if (status & llvm::APFloat::opOverflow) {
    mlir::emitError(location, "integer constant out of range for ")
        << floatType;
    return nullptr;
  }
  return mlir::ada::ConstantOp::create(builder, location, type,
                                       mlir::FloatAttr::get(floatType, f));
}

std::optional<llvm::APSInt> MLIRGenImpl::evalIntLiteral(ada_node &node) {
  ada_big_integer bigint;
  if (!ada_int_literal_p_denoted_value(&node, &bigint)) {
    mlir::emitError(loc(node), "failed to evaluate integer literal");
    return std::nullopt;
  }
  auto value = libadalang::bigIntToAPSInt(bigint);
  if (!value) {
    ada_text text;
    ada_node_text(&node, &text);
    mlir::emitError(loc(node), "invalid integer literal '")
        << libadalang::textToString(text) << "'";
  }
  return value;
}

bool MLIRGenImpl::diagnoseOutOfBaseRange(const llvm::APSInt &value,
                                         const llvm::APSInt &lo,
                                         const llvm::APSInt &hi,
                                         llvm::StringRef typeName,
                                         mlir::Location location) {
  if (mlir::ada::inStaticRange(value, lo, hi))
    return false;
  mlir::emitError(location, "value not in range of type \"")
      << typeName << "\"";
  mlir::emitError(location, "static expression fails Constraint_Check");
  return true;
}

bool MLIRGenImpl::warnOutOfSubtypeRange(const llvm::APSInt &value,
                                        const llvm::APSInt &lo,
                                        const llvm::APSInt &hi,
                                        llvm::StringRef typeName,
                                        mlir::Location location) {
  if (mlir::ada::inStaticRange(value, lo, hi))
    return false;
  mlir::emitWarning(location, "value not in range of type \"")
      << typeName << "\"";
  mlir::emitWarning(location, "Constraint_Error will be raised at run time");
  return true;
}

ada_node MLIRGenImpl::checkedIntegerBase(const llvm::APSInt &value,
                                         ada_node type_decl,
                                         mlir::Location location) {
  std::optional<ada_node> canon = libadalang::canonicalType(type_decl);
  ada_node base = canon ? canon.value() : type_decl;
  if (libadalang::isUniversalTypeDecl(base) &&
      !libadalang::isRealTypeDecl(base)) {
    auto [lo, hi] = getRuntimeIntegerBounds();
    if (diagnoseOutOfBaseRange(value, lo, hi, "root_integer", location))
      return {};
  } else if (auto b = subtypeStaticBounds(base, location)) {
    ada_node defName;
    std::string name = ada_basic_decl_p_defining_name(&base, &defName)
                           ? canonicalFqn(defName)
                           : libadalang::getName(&base);
    if (diagnoseOutOfBaseRange(value, b->first, b->second, name, location))
      return {};
  }
  return base;
}

mlir::Value MLIRGenImpl::emitCheckedIntConstant(const llvm::APSInt &value,
                                                ada_node type_decl,
                                                mlir::Location location) {
  ada_node base = checkedIntegerBase(value, type_decl, location);
  if (ada_node_is_null(&base))
    return nullptr;
  mlir::ada::QualType type = getAdaQualType(type_decl, location);
  mlir::ada::QualType baseType = getAdaQualType(base, location);
  if (!type || !baseType)
    return nullptr;
  mlir::Value constant;
  if (mlir::isa<mlir::FloatType>(baseType.getMlirType()))
    constant = emitRealConstant(value, baseType, location);
  else
    constant = emitIntConstant(value, baseType, location);
  if (!constant)
    return nullptr;
  return convertImplicitly(constant, type, location);
}

mlir::Value MLIRGenImpl::mlirGenIntLiteral(ada_node &node) {
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

std::optional<double> MLIRGenImpl::evalRealLiteral(ada_node &node) {
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

mlir::Value MLIRGenImpl::mlirGenRealLiteral(ada_node &node) {
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

mlir::Value MLIRGenImpl::emitConversion(mlir::Value val,
                                        mlir::ada::QualType target,
                                        mlir::ada::AdaConversionKind kind,
                                        mlir::Location location) {
  mlir::Value range;
  mlir::ada::TypeOp typeOp = mlir::ada::lookupAdaType(
      builder.getInsertionBlock()->getParentOp(), target.getAdaType());
  auto info = mlir::ada::intTypeInfoOfAdaType(
      builder.getInsertionBlock()->getParentOp(), target.getAdaType());
  auto bounds = info ? info.staticBounds() : std::nullopt;
  if (info && info.hasRange() && !bounds) {
    range = findDynamicRange(target.getAdaType());
    if (!range) {
      mlir::emitError(location, "no range descriptor in scope for '")
          << typeOp.getSymName() << "'";
      return nullptr;
    }
  } else if (bounds && typeOp.isSubtype()) {
    if (auto cst = val.getDefiningOp<mlir::ada::ConstantOp>())
      if (std::optional<llvm::APSInt> value = cst.getIntValue())
        warnOutOfSubtypeRange(*value, bounds->first, bounds->second,
                              typeOp.getSymName(), location);
  }
  return mlir::ada::ConvertOp::create(builder, location, target, kind, val,
                                      range);
}

mlir::Value MLIRGenImpl::convertImplicitly(mlir::Value val,
                                           mlir::ada::QualType target,
                                           mlir::Location location) {
  if (val.getType() == target)
    return val;
  return emitConversion(val, target, mlir::ada::AdaConversionKind::Implicit,
                        location);
}

mlir::Value MLIRGenImpl::mlirGenEnumLit(ada_node &enumLitDecl,
                                        ada_node &useExpr) {
  auto location = loc(useExpr);

  // Get the literal's canonical name to look up its representation value.
  ada_node lit_name_node;
  if (!ada_enum_literal_decl_f_name(&enumLitDecl, &lit_name_node) ||
      ada_node_is_null(&lit_name_node)) {
    mlir::emitError(location, "failed to get enum literal name");
    return nullptr;
  }
  std::string litName = libadalang::getName(&lit_name_node, /*canonical=*/true);

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
        << litName << "' not found in ada.type '" << typeOp.getSymName() << "'";
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

mlir::Value MLIRGenImpl::mlirGenCallExprValue(ada_node &call) {
  auto callOp = mlirGenCallExpr(call);
  if (!callOp)
    return nullptr;
  if (callOp.getNumResults() == 0) {
    mlir::emitError(loc(call), "procedure called in expression context");
    return nullptr;
  }
  return callOp->getResult(0);
}

mlir::Value MLIRGenImpl::mlirGenTypeConversion(ada_node &conversion) {
  mlir::Location location = loc(conversion);
  ada_node name = {}, suffix = {}, assoc = {}, operand = {};
  ada_call_expr_f_name(&conversion, &name);
  ada_call_expr_f_suffix(&conversion, &suffix);
  std::optional<ada_node> typeDecl = libadalang::referencedDecl(name);
  if (!typeDecl || ada_node_children_count(&suffix) != 1) {
    mlir::emitError(location, "unsupported type conversion");
    return nullptr;
  }
  mlir::ada::QualType target = getAdaQualType(*typeDecl, location);
  if (!target)
    return nullptr;
  ada_node_child(&suffix, 0, &assoc);
  ada_param_assoc_f_r_expr(&assoc, &operand);
  mlir::Operation *scope = builder.getInsertionBlock()->getParentOp();
  bool toInt = mlir::isa<mlir::IntegerType>(target.getMlirType());
  // A static universal operand has no expected type (@rm{4-6}(27)).
  auto operandType = libadalang::expressionType(operand);
  bool universal = operandType &&
                   libadalang::isUniversalTypeDecl(operandType.value()) &&
                   libadalang::isStaticExpr(operand);
  bool realOperand =
      universal && libadalang::isRealTypeDecl(operandType.value());
  if (universal && !realOperand &&
      !libadalang::containsAttribute(operand, {"pos", "val", "succ", "pred"})) {
    // Evaluate universal integers exactly before converting (@rm{4-9}(35)).
    std::optional<llvm::APSInt> staticValue =
        libadalang::evalExprAsInt(operand);
    if (!staticValue) {
      mlir::emitError(location, "failed to evaluate static integer expression");
      return nullptr;
    }
    if (toInt)
      return emitCheckedIntConstant(staticValue.value(), typeDecl.value(),
                                    location);
    auto floatType = mlir::dyn_cast<mlir::FloatType>(target.getMlirType());
    auto baseType = mlir::ada::baseOfAdaType(scope, target);
    if (!floatType || !baseType) {
      mlir::emitError(location, "unsupported type conversion");
      return nullptr;
    }
    mlir::Value constant =
        emitRealConstant(staticValue.value(), baseType, location);
    if (!constant)
      return nullptr;
    if (constant.getType() == target)
      return constant;
    return emitConversion(constant, target,
                          mlir::ada::AdaConversionKind::Explicit, location);
  }
  // Fold a static conversion to an integer type, then convert it to the
  // target subtype, in case the fold took its base type.
  if (toInt)
    if (std::optional<mlir::Value> folded = tryEmitStaticIntExpr(conversion)) {
      if (!folded.value() || folded.value().getType() == target)
        return folded.value();
      return emitConversion(folded.value(), target,
                            mlir::ada::AdaConversionKind::Explicit, location);
    }
  // @todo A static real operand (`0.1 + 0.2 - 0.3`) is computed in a float
  //       type at run time, rounded at each step, not exactly as
  //       @rm{4-9}(33) requires. Even a literal is rounded through double,
  //       so `Long_Long_Integer (123456789012345678.0)` is wrong. Libadalang
  //       evaluates no real expression: fold it with exact rationals, then
  //       round once.
  llvm::SaveAndRestore<ada_node> savedType(
      universalOperandType, realOperand ? operandType.value() : ada_node{});
  mlir::Value value = visit_expr(operand);
  if (!value)
    return nullptr;
  auto source = mlir::dyn_cast<mlir::ada::QualType>(value.getType());
  auto isEnum = [&](mlir::ada::QualType type) {
    mlir::ada::TypeOp root = mlir::ada::rootOfAdaType(scope, type.getAdaType());
    return root && mlir::isa_and_nonnull<mlir::ada::EnumTypeInfoAttr>(
                       root.getTypeInfoAttr());
  };
  mlir::Type from = source ? source.getMlirType() : mlir::Type();
  mlir::Type to = target.getMlirType();
  // @todo Conversions of enumeration types (by position, @rm{4-6}).
  if (!from || !mlir::ada::isNumericMachineType(from) ||
      !mlir::ada::isNumericMachineType(to) || isEnum(source) ||
      isEnum(target)) {
    mlir::emitError(location, "unsupported type conversion");
    return nullptr;
  }
  if (value.getType() == target)
    return value;
  return emitConversion(value, target, mlir::ada::AdaConversionKind::Explicit,
                        location);
}

mlir::Value MLIRGenImpl::indexedElementRef(ada_node &index) {
  auto location = loc(index);

  ada_node name = {}, suffix = {};
  ada_call_expr_f_name(&index, &name);
  ada_call_expr_f_suffix(&index, &suffix);

  // One dimensional array support limitation.
  if (ada_node_children_count(&suffix) != 1) {
    mlir::emitError(location, "unsupported array index");
    return nullptr;
  }

  mlir::Value arrayRef = findVarValue(name);
  auto arrayQual = arrayRef ? mlir::ada::objectValueType(arrayRef.getType())
                            : mlir::ada::QualType{};
  auto layout =
      arrayQual ? mlir::dyn_cast<mlir::ada::ArrayType>(arrayQual.getMlirType())
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
  ada_node typeExpr = {};
  ada_object_decl_f_type_expr(&objDecl.value(), &typeExpr);
  std::optional<ada_node> arrayTypeDecl =
      libadalang::designatedTypeDecl(typeExpr);
  if (!arrayTypeDecl) {
    mlir::emitError(location, "failed to resolve type expression");
    return nullptr;
  }
  auto typeOp = typeDecls.lookup(arrayTypeDecl.value().node);
  auto info =
      mlir::cast<mlir::ada::ArrayTypeInfoAttr>(typeOp.getTypeInfoAttr());

  ada_node assoc = {}, indexExpr = {};
  ada_node_child(&suffix, 0, &assoc);
  ada_param_assoc_f_r_expr(&assoc, &indexExpr);
  mlir::Value rawIndex = visit_expr(indexExpr);
  if (!rawIndex)
    return nullptr;

  // Diagnose static indices. Lowering emits runtime checks.
  if (auto cst = rawIndex.getDefiningOp<mlir::ada::ConstantOp>())
    if (auto value = cst.getIntValue())
      warnOutOfSubtypeRange(*value, mlir::ada::boundValue(info.staticLower(0)),
                            mlir::ada::boundValue(info.staticUpper(0)),
                            info.getIndexTypes()[0].getValue(), location);

  // Keep the selected component's identity through call lowering.
  auto eltQual = mlir::ada::QualType::get(
      builder.getContext(), layout.getComponentType(), info.getComponent());

  auto resultTy = mlir::ada::RefType::get(builder.getContext(), eltQual);

  return mlir::ada::IndexOp::create(builder, location, resultTy, arrayRef,
                                    rawIndex, mlir::Value{});
}

mlir::Value MLIRGenImpl::mlirGenIndexValue(ada_node &index) {
  mlir::Value elt = indexedElementRef(index);
  return elt ? readObject(elt, loc(index)) : mlir::Value{};
}

mlir::Value MLIRGenImpl::visit_expr(ada_node &expr) {
  switch (ada_node_kind(&expr)) {
  case ada_identifier: {
    ada_node def_name;
    if (ada_name_p_referenced_defining_name(&expr, /*imprecise_fallback=*/0,
                                            &def_name) &&
        !ada_node_is_null(&def_name)) {
      auto it = numberDecls.find(def_name);
      if (it != numberDecls.end()) {
        auto &number = it->second;
        mlir::Location exprLoc = loc(expr);
        ada_node type = resolveLiteralType(expr, exprLoc);
        if (ada_node_is_null(&type))
          return nullptr;
        if (number.integer) {
          if (!number.value)
            return emitCheckedIntConstant(number.integer.value(), type,
                                          exprLoc);
          auto base = checkedIntegerBase(number.integer.value(), type, exprLoc);
          if (ada_node_is_null(&base))
            return nullptr;
        }
        if (number.value) {
          auto target = getAdaQualType(type, exprLoc);
          return target ? convertImplicitly(number.value, target, exprLoc)
                        : nullptr;
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
    ada_node name{};
    ada_call_expr_f_name(&expr, &name);
    if (ada_node_kind(&name) == ada_attribute_ref)
      return mlirGenAttributeRef(expr);
    ada_call_expr_kind kind;
    if (!ada_call_expr_p_kind(&expr, &kind)) {
      mlir::emitError(loc(expr), "can't get kind of call expr");
      return nullptr;
    }

    switch (kind) {
    case ADA_CALL_EXPR_KIND_ARRAY_INDEX:
      return mlirGenIndexValue(expr);
    case ADA_CALL_EXPR_KIND_TYPE_CONVERSION:
      return mlirGenTypeConversion(expr);
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

mlir::Value MLIRGenImpl::synthesizeBooleanTrue(ada_node &context_node,
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

mlir::Value MLIRGenImpl::mlirGenIfExpr(ada_node &if_expr) {
  mlir::Location location = loc(if_expr);

  // Result type, shared by every dependent_expression.
  auto type_decl = libadalang::expressionType(if_expr);
  if (!type_decl) {
    mlir::emitError(location, "failed to resolve type of if expression");
    return nullptr;
  }
  mlir::ada::QualType resultType = getAdaQualType(type_decl.value(), location);
  if (!resultType)
    return nullptr;

  auto guards = collectIfGuards(if_expr);
  if (!guards)
    return nullptr;

  ada_node elseExpr;
  ada_if_expr_f_else_expr(&if_expr, &elseExpr);
  bool hasElse = !ada_node_is_null(&elseExpr);

  return emitIfExprChain(*guards, /*idx=*/0, elseExpr, hasElse, resultType,
                         location);
}

mlir::Value MLIRGenImpl::emitIfExprChain(llvm::ArrayRef<IfGuard> guards,
                                         unsigned idx, ada_node elseExpr,
                                         bool hasElse,
                                         mlir::ada::QualType resultType,
                                         mlir::Location location) {
  ada_node condNode = guards[idx].condition;
  ada_node thenNode = guards[idx].thenPart;

  mlir::Value condI1 = mlirGenCondition(condNode);
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
  // Each dependent expression is converted to the expression's type
  // (@rm{4-5-7}).
  thenVal = convertImplicitly(thenVal, resultType, loc(thenNode));
  if (!thenVal)
    return nullptr;
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
      elseVal = convertImplicitly(elseVal, resultType, loc(elseExpr));
  } else {
    // No else: the if expression is of a boolean type and the absent else
    // yields True (@rm{4-5-7}(18/3, 20/3)).
    elseVal = synthesizeBooleanTrue(condNode, location);
    if (elseVal)
      elseVal = convertImplicitly(elseVal, resultType, location);
  }
  if (!elseVal)
    return nullptr;
  mlir::scf::YieldOp::create(builder, location, elseVal);

  builder.setInsertionPointAfter(ifOp);
  return ifOp.getResult(0);
}

} // namespace lalvm::mlirgen
