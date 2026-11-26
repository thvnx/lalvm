// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2024-2026 The LALVM Project

#include "MLIRGenImpl.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"
#include "mlir/IR/Diagnostics.h"
#include "llvm/ADT/STLExtras.h"

namespace libadalang = frontend::libadalang;

namespace lalvm::mlirgen {

mlir::Value MLIRGenImpl::mapEnumAttribute(llvm::StringRef name,
                                          mlir::Value value,
                                          mlir::ada::TypeOp root,
                                          mlir::ada::EnumTypeInfoAttr info,
                                          mlir::ada::QualType resultType,
                                          mlir::Location location) {
  auto values = info.getValues();
  auto mappedValue = [&](size_t i) -> std::optional<int64_t> {
    if (name == "pos")
      return i;
    if (name == "succ") {
      if (i + 1 < values.size())
        return values[i + 1];
    } else if (i > 0) {
      return values[i - 1];
    }
    return std::nullopt;
  };
  if (auto constant = value.getDefiningOp<mlir::ada::ConstantOp>()) {
    auto code = constant.getIntValue().value().getExtValue();
    auto it = llvm::find(values, code);
    if (it != values.end())
      if (auto result = mappedValue(it - values.begin()))
        return emitIntConstant(llvm::APInt(64, *result), resultType, location);
  }

  auto enumType = qualTypeFor(root, root.getMlirType());
  auto &helper = enumAttributeFunctions[root][name];
  if (!helper) {
    mlir::OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointAfter(root);
    auto symbol = makeUnique((root.getSymName() + "." + name).str());
    auto signature = builder.getFunctionType({enumType}, {resultType});
    helper =
        mlir::ada::SubpOp::create(builder, root.getLoc(), symbol, signature);
    helper.setPrivate();
    auto loc = root.getLoc();
    auto &body = helper.getBody();
    auto &entry = body.front();
    auto *invalid = builder.createBlock(&body);
    mlir::ada::ConstraintErrorOp::create(
        builder, loc, mlir::ada::AdaConstraintErrorKind::InvalidData);
    llvm::SmallVector<llvm::APInt> codes;
    llvm::SmallVector<mlir::Block *> cases;
    for (auto [position, code] : llvm::enumerate(values)) {
      codes.push_back(
          builder.getIntegerAttr(root.getMlirType(), code).getValue());
      cases.push_back(builder.createBlock(&body));
      if (auto output = mappedValue(position)) {
        auto result =
            emitIntConstant(llvm::APInt(64, *output), resultType, loc);
        mlir::ada::ReturnOp::create(builder, loc, result);
      } else {
        mlir::ada::ConstraintErrorOp::create(
            builder, loc, mlir::ada::AdaConstraintErrorKind::Overflow);
      }
    }
    builder.setInsertionPointToStart(&entry);
    auto raw = unwrap(entry.getArgument(0), loc);
    llvm::SmallVector<mlir::ValueRange> operands(cases.size());
    mlir::cf::SwitchOp::create(builder, loc, raw, invalid, mlir::ValueRange{},
                               codes, cases, operands);
  }
  auto arg = mlir::ada::requalify(builder, location, value, enumType);
  auto callee = mlir::FlatSymbolRefAttr::get(helper.getSymNameAttr());
  return mlir::ada::CallOp::create(builder, location, callee, resultType, arg)
      .getResult();
}

mlir::Value MLIRGenImpl::stepEnumeration(ada_node &expr, mlir::Value value,
                                         mlir::ada::QualType type,
                                         mlir::ada::EnumTypeInfoAttr info,
                                         bool character, bool successor) {
  auto location = loc(expr);
  auto machineType = mlir::cast<mlir::IntegerType>(type.getMlirType());
  auto codes = info.getValues();
  unsigned width = machineType.getWidth();
  auto endpoint =
      character
          ? builder.getIntegerAttr(machineType,
                                   successor ? llvm::APInt::getMaxValue(width)
                                             : llvm::APInt::getZero(width))
          : builder.getIntegerAttr(machineType,
                                   successor ? codes.back() : codes.front());
  auto bound = mlir::ada::ConstantOp::create(builder, location, type, endpoint);
  value = mlir::ada::requalify(builder, location, value, type);
  auto checked = mlir::ada::OverflowCheckOp::create(
      builder, location, type, mlir::ada::AdaRelationalOp::Eq, value, bound);
  if (!character && codes.size() == 1)
    return checked;
  auto one = emitIntConstant(llvm::APInt(width, 1), type, location);
  return mlir::ada::BinOp::create(builder, location, type,
                                  successor ? mlir::ada::AdaBinaryOp::Plus
                                            : mlir::ada::AdaBinaryOp::Minus,
                                  checked, one, mlir::ada::AdaChecksAttr{});
}

mlir::ada::QualType MLIRGenImpl::getPositionType(ada_node &expr,
                                                 mlir::Location location) {
  ada_node declaration{};
  if (!ada_ada_node_p_universal_int_type(&expr, &declaration) ||
      ada_node_is_null(&declaration)) {
    mlir::emitError(location, "failed to resolve position type");
    return {};
  }
  return getAdaQualType(declaration, location);
}

mlir::Value MLIRGenImpl::mlirGenDiscreteAttribute(ada_node &expr,
                                                  ada_node &subtype,
                                                  mlir::ada::TypeOp typeOp,
                                                  ada_node &args,
                                                  llvm::StringRef name) {
  auto location = loc(expr);
  ada_node assoc{}, argument{};
  if (ada_node_is_null(&args) || ada_node_children_count(&args) != 1 ||
      !ada_node_child(&args, 0, &assoc) ||
      !ada_param_assoc_f_r_expr(&assoc, &argument)) {
    mlir::emitError(location, "'") << name << "' requires one argument";
    return nullptr;
  }
  auto root = mlir::ada::rootOfAdaType(typeOp);
  auto baseType = qualTypeFor(root, root.getMlirType());
  if (!mlir::isa<mlir::IntegerType>(baseType.getMlirType())) {
    // @todo Lower floating point 'Succ and 'Pred as adjacent machine values.
    mlir::emitError(location, "'")
        << name << "' is only supported on discrete types";
    return nullptr;
  }
  auto info = mlir::dyn_cast_or_null<mlir::ada::EnumTypeInfoAttr>(
      root.getTypeInfoAttr());
  bool character = libadalang::isPredefinedCharacterType(subtype);
  bool identity = !info || character || info.hasPositionalRepresentation();
  mlir::Value value = visit_expr(argument);
  if (!value)
    return nullptr;
  mlir::ada::QualType resultType;
  if (name == "pos") {
    auto resultDecl = libadalang::expressionType(expr);
    if (!resultDecl)
      return nullptr;
    ada_node expected{};
    if (!ada_expr_p_expected_expression_type(&expr, &expected) ||
        ada_node_is_null(&expected))
      expected = resultDecl.value();
    expected = libadalang::canonicalType(expected).value_or(expected);
    resultType = getAdaQualType(expected, location);
    if (!resultType)
      return nullptr;
    if (identity)
      return convertImplicitly(value, resultType, location);
  }

  unsigned width = baseType.getMlirType().getIntOrFloatBitWidth();
  bool unsignedBase = mlir::ada::isUnsignedOrderQualType(root, baseType);
  llvm::APSInt lo = llvm::APSInt::getMinValue(width, unsignedBase);
  llvm::APSInt hi = llvm::APSInt::getMaxValue(width, unsignedBase);
  bool stepping = name == "succ" || name == "pred";
  if (stepping) {
    if (!identity)
      return mapEnumAttribute(name, value, root, info, baseType, location);
    if (info)
      return stepEnumeration(expr, value, baseType, info, character,
                             name == "succ");
    value = convertImplicitly(value, baseType, location);
    if (!value)
      return nullptr;
    auto checks =
        unsignedBase
            ? mlir::ada::AdaChecksAttr{}
            : mlir::ada::AdaChecksAttr::get(builder.getContext(),
                                            mlir::ada::AdaChecks::Overflow);
    auto one = emitIntConstant(llvm::APInt(width, 1), baseType, location);
    return mlir::ada::BinOp::create(builder, location, baseType,
                                    name == "succ"
                                        ? mlir::ada::AdaBinaryOp::Plus
                                        : mlir::ada::AdaBinaryOp::Minus,
                                    value, one, checks);
  }
  if (name == "val" && info && !character)
    return convertImplicitly(value, baseType, location);
  // A conversion checks the modular base range or a full signed range.
  if (name == "val" && !info) {
    auto integer =
        mlir::cast<mlir::ada::IntegerTypeInfoAttr>(root.getTypeInfoAttr());
    if (integer.getModulus())
      return convertImplicitly(value, baseType, location);
    if (auto bounds = integer.staticBounds();
        bounds && llvm::APSInt::isSameValue(bounds->first, lo) &&
        llvm::APSInt::isSameValue(bounds->second, hi))
      return convertImplicitly(value, baseType, location);
  }

  auto positionType = getPositionType(expr, location);
  if (!positionType)
    return nullptr;
  // Work in position space, using the base range rather than prefix bounds.
  mlir::Value position =
      !identity
          ? mapEnumAttribute(name, value, root, info, positionType, location)
          : convertImplicitly(value, positionType, location);
  if (!position)
    return nullptr;
  if (name == "pos")
    return convertImplicitly(position, resultType, location);
  unsigned positionWidth = positionType.getMlirType().getIntOrFloatBitWidth();
  lo = lo.extOrTrunc(positionWidth);
  hi = hi.extOrTrunc(positionWidth);
  auto range = mlir::ada::getOrCreateStaticRange(
      builder, location, positionType.getAdaType(), positionType, lo, hi);
  position = mlir::ada::RangeCheckOp::create(builder, location, positionType,
                                             position, range);
  auto raw = mlir::ada::UnwrapOp::create(builder, location,
                                         positionType.getMlirType(), position);
  mlir::Value result = raw;
  if (width < positionType.getMlirType().getIntOrFloatBitWidth())
    result = mlir::arith::TruncIOp::create(builder, location,
                                           baseType.getMlirType(), raw);
  return mlir::ada::WrapOp::create(builder, location, baseType, result);
}

mlir::Value MLIRGenImpl::mlirGenAttributeRef(ada_node &expr) {
  mlir::Location location = loc(expr);

  ada_node attribute = expr, args{};
  if (ada_node_kind(&expr) == ada_call_expr) {
    ada_call_expr_f_name(&expr, &attribute);
    ada_call_expr_f_suffix(&expr, &args);
  } else {
    ada_attribute_ref_f_args(&attribute, &args);
  }
  ada_node attrId;
  ada_attribute_ref_f_attribute(&attribute, &attrId);
  std::string name = libadalang::getName(&attrId, /*canonical=*/true);
  // @todo Support the other scalar/enumeration attributes (@rm{3-5-5}):
  //       'Image, 'Value.
  if (name != "first" && name != "last" && name != "pos" && name != "val" &&
      name != "succ" && name != "pred") {
    mlir::emitError(location, "unsupported attribute '") << name << "'";
    return nullptr;
  }

  ada_node prefix, subtype;
  ada_attribute_ref_f_prefix(&attribute, &prefix);
  if (!ada_name_p_name_designated_type(&prefix, &subtype) ||
      ada_node_is_null(&subtype)) {
    mlir::emitError(location, "'") << name << "' prefix is not a subtype";
    return nullptr;
  }
  mlir::ada::QualType type = getAdaQualType(subtype, location);
  if (!type)
    return nullptr;
  mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(subtype, location);
  if (name == "pos" || name == "val" || name == "succ" || name == "pred")
    return mlirGenDiscreteAttribute(expr, subtype, typeOp, args, name);
  auto info = mlir::ada::intTypeInfoOfAdaType(
      builder.getInsertionBlock()->getParentOp(), type.getAdaType());
  if (!info) {
    mlir::emitError(location, "'")
        << name << "' is only supported on integer subtypes";
    return nullptr;
  }

  // `S'First` and `S'Last` are of the type of S, not of the subtype S
  // (@rm{3-5}(12, 13)): the root type stands for it.
  ada_node base = libadalang::canonicalType(subtype).value_or(subtype);
  mlir::ada::QualType baseType = getAdaQualType(base, location);
  if (!baseType)
    return nullptr;

  // Static bound: read it straight from the subtype's `int_info`.
  bool wantLow = name == "first";
  if (mlir::IntegerAttr bound =
          wantLow ? info.staticLower() : info.staticUpper())
    return emitIntConstant(bound.getValue(), baseType, location);

  // Dynamic bound: read from the range descriptor elaborated at the subtype
  // declaration.
  mlir::Value range = findDynamicRange(type.getAdaType());
  if (!range) {
    mlir::emitError(location, "no range descriptor in scope for '")
        << name << "'";
    return nullptr;
  }
  mlir::Value attr = mlir::ada::AttrOp::create(
      builder, location, type, builder.getStringAttr(name), range);
  return emitConversion(attr, baseType, mlir::ada::AdaConversionKind::Implicit,
                        location);
}

} // namespace lalvm::mlirgen
