// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2024-2026 The LALVM Project

#include "MLIRGenImpl.h"
#include "mlir/IR/Diagnostics.h"
#include "llvm/ADT/bit.h"
#include <algorithm>
#include <cstdint>

namespace libadalang = frontend::libadalang;

namespace lalvm::mlirgen {

std::optional<std::pair<llvm::APSInt, llvm::APSInt>>
MLIRGenImpl::subtypeStaticBounds(ada_node &type_decl, mlir::Location location) {
  mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(type_decl, location);
  if (!typeOp)
    return std::nullopt;
  auto info = mlir::ada::intTypeInfoOfAdaType(
      typeOp, mlir::FlatSymbolRefAttr::get(typeOp.getSymNameAttr()));
  if (!info)
    return std::nullopt;
  return info.staticBounds();
}

mlir::ada::QualType MLIRGenImpl::qualTypeFor(mlir::ada::TypeOp typeOp,
                                             mlir::Type mlirType) {
  auto *ctx = builder.getContext();
  auto typeRef = mlir::FlatSymbolRefAttr::get(ctx, typeOp.getSymName());
  return mlir::ada::QualType::get(ctx, mlirType, typeRef);
}

mlir::ada::QualType MLIRGenImpl::getAdaQualType(ada_node &type_decl,
                                                mlir::Location location) {
  mlir::Type mlirType = getMLIRTypeFromDecl(type_decl, location);
  if (!mlirType)
    return {};
  mlir::ada::TypeOp typeOp = lookupOrEmitTypeOp(type_decl, location);
  if (!typeOp) {
    mlir::emitError(location, "ada.type not emitted for this type");
    return {};
  }
  return qualTypeFor(typeOp, mlirType);
}

mlir::ada::QualType MLIRGenImpl::getAdaQualType(ada_node &type_expr) {
  std::optional<ada_node> type_decl = libadalang::designatedTypeDecl(type_expr);
  if (!type_decl) {
    mlir::emitError(loc(type_expr), "failed to resolve type expression");
    return {};
  }
  return getAdaQualType(type_decl.value(), loc(type_expr));
}

mlir::ada::TypeOp MLIRGenImpl::lookupOrEmitTypeOp(ada_node &type_decl,
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

bool MLIRGenImpl::isModularTypeOp(mlir::ada::TypeOp typeOp) {
  return typeOp && mlir::ada::modulusOfAdaType(typeOp);
}

bool MLIRGenImpl::isCharacterType(ada_node type_decl) {
  ada_bool result = false;
  return ada_base_type_decl_p_is_char_type(&type_decl, &libadalang::kNullOrigin,
                                           &result) &&
         result;
}

std::optional<int64_t> MLIRGenImpl::enumLiteralRep(ada_node &lit,
                                                   mlir::Location location) {
  ada_big_integer bigint;
  if (!ada_enum_literal_decl_p_enum_rep(&lit, &bigint)) {
    mlir::emitError(location, "failed to get enum literal rep value");
    return std::nullopt;
  }
  auto val = libadalang::bigIntToAPSInt(bigint);
  // Enum rep values are stored as int64 in EnumTypeInfoAttr, which covers any
  // normally-sized enum (i8..i64); reject the rare value that would not fit.
  if (!val || val->getSignificantBits() > 64) {
    mlir::emitError(location, "enum rep value out of range");
    return std::nullopt;
  }
  return val->getSExtValue();
}

std::optional<int64_t> MLIRGenImpl::charLiteralRep(ada_node &lit,
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

bool MLIRGenImpl::isSupportedTypeDecl(ada_node &decl) {
  std::optional<ada_node> canon = libadalang::canonicalType(decl);
  if (!canon)
    return false;
  return libadalang::isUniversalTypeDecl(canon.value()) ||
         libadalang::isNumericTypeDecl(canon.value()) ||
         libadalang::isEnumTypeDecl(canon.value()) ||
         libadalang::isArrayTypeDecl(canon.value());
}

llvm::FailureOr<std::string>
MLIRGenImpl::resolveTypeDeclName(ada_node &type_decl, bool external) {
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

mlir::Attribute MLIRGenImpl::subtypeInfo(mlir::ada::TypeOp baseOp, ada_node lo,
                                         ada_node hi) {
  if (ada_node_is_null(&lo) || ada_node_is_null(&hi))
    return {};
  mlir::Attribute baseInfo = baseOp.getTypeInfoAttr();
  if (mlir::isa_and_nonnull<mlir::ada::IntegerTypeInfoAttr>(baseInfo))
    return mlir::ada::IntegerTypeInfoAttr::get(
        builder.getContext(), /*modulus=*/mlir::IntegerAttr(),
        rangeBoundAttr(lo), rangeBoundAttr(hi));
  if (auto enumInfo =
          mlir::dyn_cast_or_null<mlir::ada::EnumTypeInfoAttr>(baseInfo)) {
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
    return mlir::ada::EnumTypeInfoAttr::get(
        builder.getContext(), builder.getArrayAttr({}), /*values=*/{},
        boundAttr(lo), boundAttr(hi));
  }
  return {};
}

mlir::ada::TypeOp MLIRGenImpl::createSubtypeOp(llvm::StringRef name,
                                               mlir::ada::TypeOp parent,
                                               ada_node lo, ada_node hi,
                                               mlir::Location location) {
  auto base = mlir::ada::rootOfAdaType(parent);
  return mlir::ada::TypeOp::create(
      builder, location, name, parent.getMlirType(), subtypeInfo(base, lo, hi),
      mlir::FlatSymbolRefAttr::get(parent.getSymNameAttr()));
}

void MLIRGenImpl::elaborateDynamicRange(mlir::ada::TypeOp typeOp, ada_node lo,
                                        ada_node hi, mlir::Location location) {
  auto intInfo = mlir::dyn_cast_or_null<mlir::ada::IntegerTypeInfoAttr>(
      typeOp.getTypeInfoAttr());
  if (!intInfo || !intInfo.hasRange() || intInfo.staticBounds())
    return;
  // Bounds have the subtype's base type (@rm{3-5}). A static bound is an
  // `ada.constant` and a dynamic one is its evaluated expression converted to
  // that base type (so both bounds share the base `!ada.qual`).
  auto base = mlir::ada::rootOfAdaType(typeOp);
  auto baseQual = qualTypeFor(base, base.getMlirType());
  auto boundValue = [&](ada_node &expr, mlir::IntegerAttr stat) -> mlir::Value {
    if (stat)
      return emitIntConstant(stat.getValue(), baseQual, location);
    mlir::Value v = visit_expr(expr);
    return v ? convertImplicitly(v, baseQual, loc(expr)) : v;
  };
  mlir::Value loValue = boundValue(lo, intInfo.staticLower());
  mlir::Value hiValue = boundValue(hi, intInfo.staticUpper());
  if (loValue && hiValue)
    dynamicRanges[typeOp.getSymNameAttr()] = mlir::ada::createRange(
        builder, location, loValue, hiValue,
        mlir::FlatSymbolRefAttr::get(typeOp.getSymNameAttr()));
}

mlir::ada::TypeOp MLIRGenImpl::emitAnonymousSubtype(ada_node &id,
                                                    ada_node &markDecl,
                                                    ada_node lo, ada_node hi,
                                                    mlir::Location location) {
  auto parent = lookupOrEmitTypeOp(markDecl, location);
  if (!parent)
    return {};

  std::string name = makeUniqueInScope(mlir::ada::kAnonymousTypePrefix.str() +
                                       libadalang::getName(&id));

  mlir::ada::TypeOp typeOp;
  {
    mlir::OpBuilder::InsertionGuard guard(builder);
    if (currentDeclsBlock)
      builder.setInsertionPointToEnd(currentDeclsBlock);
    else
      builder.setInsertionPointToStart(adaModule.getBody());
    typeOp = createSubtypeOp(name, parent, lo, hi, location);
  }
  elaborateDynamicRange(typeOp, lo, hi, location);
  return typeOp;
}

mlir::LogicalResult MLIRGenImpl::mlirGenSubtypeDecl(ada_node &type_decl,
                                                    bool external) {
  auto location = loc(type_decl);
  std::optional<ada_node> canon = libadalang::canonicalType(type_decl);
  if (!canon)
    return mlir::emitError(location,
                           "failed to resolve the subtype's base type");
  mlir::ada::TypeOp baseOp = lookupOrEmitTypeOp(canon.value(), location);
  if (!baseOp)
    return mlir::failure();

  auto typeName = resolveTypeDeclName(type_decl, external);
  if (mlir::failed(typeName))
    return mlir::failure();

  ada_node indication = {}, constraint = {}, lo = {}, hi = {};
  ada_internal_discrete_range range{};
  if (ada_subtype_decl_f_subtype(&type_decl, &indication) &&
      !ada_node_is_null(&indication) &&
      ada_subtype_indication_f_constraint(&indication, &constraint) &&
      !ada_node_is_null(&constraint) &&
      ada_base_type_decl_p_discrete_range(&type_decl, &range)) {
    lo = range.low_bound;
    hi = range.high_bound;
  }

  auto parent = baseOp;
  ada_node markName{};
  if (ada_subtype_indication_f_name(&indication, &markName) &&
      ada_node_kind(&markName) != ada_attribute_ref) {
    auto mark = libadalang::designatedTypeDecl(indication);
    parent =
        mark ? lookupOrEmitTypeOp(mark.value(), location) : mlir::ada::TypeOp{};
    if (!parent)
      return mlir::emitError(location, "failed to resolve subtype mark");
  }
  auto typeOp = createSubtypeOp(*typeName, parent, lo, hi, location);
  typeDecls[type_decl.node] = typeOp;

  // The bound values go in the body's entry block, before this `ada.decls`,
  // so they dominate the checks, the `ada.type` symbol stays in `ada.decls`.
  mlir::Operation *declsOp = builder.getInsertionBlock()->getParentOp();
  if (mlir::isa<mlir::ada::DeclsOp>(declsOp)) {
    mlir::OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPoint(declsOp);
    elaborateDynamicRange(typeOp, lo, hi, location);
    if (!typeOp.getTypeInfoAttr() && typeOp.hasDynamicRange()) {
      auto range = findDynamicRange(typeOp.getParentAttr());
      if (!range)
        return mlir::emitError(location, "missing inherited range descriptor");
      auto bounds = range.getDefiningOp<mlir::ada::RangeOp>();
      dynamicRanges[typeOp.getSymNameAttr()] = mlir::ada::createRange(
          builder, location, bounds.getLow(), bounds.getHigh(),
          mlir::FlatSymbolRefAttr::get(typeOp.getSymNameAttr()));
    }
  }
  return mlir::success();
}

mlir::Value MLIRGenImpl::findDynamicRange(mlir::FlatSymbolRefAttr sym) {
  return dynamicRanges.lookup(sym.getAttr());
}

mlir::LogicalResult MLIRGenImpl::mlirGenTypeDecl(ada_node &type_decl,
                                                 bool external) {
  mlir::LogicalResult result = mlir::success();
  if (ada_node_kind(&type_decl) == ada_subtype_decl)
    result = mlirGenSubtypeDecl(type_decl, external);
  // Universal types (@rm{3-4-1}) and numeric types
  // (@rm{3-5-4}, @rm{3-5-6}, @rm{3-5-7}).
  else if (libadalang::isUniversalTypeDecl(type_decl) ||
           libadalang::isNumericTypeDecl(type_decl))
    result = mlirGenNumericTypeDecl(type_decl, external);
  // Array types (@rm{3-6}).
  else if (libadalang::isArrayTypeDecl(type_decl))
    result = mlirGenArrayTypeDecl(type_decl, external);
  else
    result = mlirGenEnumTypeDecl(type_decl, external);
  if (mlir::failed(result))
    return result;

  auto typeOp = typeDecls.lookup(type_decl.node);
  if (typeOp && ada_node_kind(&type_decl) != ada_subtype_decl) {
    if (auto parentDecl = libadalang::baseType(type_decl)) {
      auto parent = lookupOrEmitTypeOp(parentDecl.value(), loc(type_decl));
      if (!parent)
        return mlir::failure();
      typeOp.setKind(mlir::ada::AdaTypeKind::Derived);
      typeOp.setParentAttr(
          mlir::FlatSymbolRefAttr::get(parent.getSymNameAttr()));
    }
  }
  if (typeOp && mlir::ada::isNumericMachineType(typeOp.getMlirType()) &&
      !typeOp.hasDefaultValue() && libadalang::defaultValueExpr(type_decl))
    typeOp.setHasDefaultValueAttr(builder.getUnitAttr());
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenNumericTypeDecl(ada_node &type_decl,
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
      return mlir::emitError(location,
                             "modular type modulus is not a static expression");
    auto modulusAP = libadalang::evalExprAsInt(expr);
    if (!modulusAP)
      return mlir::emitError(location, "invalid modular type modulus");
    // Check the modular type limits (@rm{3-5-4}(7)).
    // @todo Retrieve System.Max_Binary_Modulus and
    //       System.Max_Nonbinary_Modulus through Libadalang.
    if (!modulusAP->isStrictlyPositive())
      return mlir::emitError(loc(expr), "modulus value must be positive");
    unsigned bits = modulusAP->getActiveBits();
    bool binary = modulusAP->isPowerOf2();
    if (bits > 129 || (bits == 129 && !binary))
      return mlir::emitError(loc(expr), "modulus exceeds limit (2 ** 128)");
    if (bits > 32 && !binary)
      return mlir::emitError(loc(expr),
                             "nonbinary modulus exceeds limit (2 ** 32 - 1)");
    modulus = minimalWidthIntAttr(*modulusAP);
    // Width holds 0 .. modulus-1, byte-rounded to a power of two (at most
    // i128, see above).
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
    typeInfo = mlir::ada::FloatTypeInfoAttr::get(builder.getContext(), *digits);
  } else {
    // Record the declared range as metadata: static bounds as values,
    // dynamic bounds as UnitAttr (printed `?`). Universal integer is
    // unconstrained and keeps the bare attribute: its placeholder range
    // in LAL's Standard (-1 .. 1 standing for an infinite range) must
    // not be recorded.
    mlir::Attribute lower, upper;
    if (libadalang::isRootIntegerTypeDecl(type_decl)) {
      // Libadalang's root_integer range is a placeholder, like
      // universal_integer.
      auto [lo, hi] = getRuntimeIntegerBounds();
      lower = minimalWidthIntAttr(lo);
      upper = minimalWidthIntAttr(hi);
    } else if (!libadalang::isUniversalTypeDecl(type_decl)) {
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

  auto typeOp = mlir::ada::TypeOp::create(builder, location, *typeName,
                                          mlirType, typeInfo);
  typeDecls[type_decl.node] = typeOp;
  return mlir::success();
}

mlir::LogicalResult MLIRGenImpl::mlirGenArrayTypeDecl(ada_node &type_decl,
                                                      bool external) {
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
  if (!ada_base_type_decl_p_comp_type(&type_decl, /*is_subscript=*/false,
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
  if (!ada_base_type_decl_p_index_type(&type_decl, /*dim=*/0,
                                       &libadalang::kNullOrigin, &index_type) ||
      ada_node_is_null(&index_type))
    return mlir::failure();

  mlir::Type indexType = getMLIRTypeFromDecl(index_type, location);
  if (!indexType)
    return mlir::failure();
  mlir::ada::TypeOp indexOp = lookupOrEmitTypeOp(index_type, location);
  if (!indexOp)
    return mlir::failure();
  auto indexSym = mlir::FlatSymbolRefAttr::get(indexOp.getSymNameAttr());

  // The index bounds live in the ConstraintList, as a BinOp (LO .. HI).
  ada_node constraint_list = {};
  ada_constrained_array_indices_f_list(&indices, &constraint_list);
  ada_node dim0 = {};
  auto bounds = ada_node_child(&constraint_list, 0, &dim0)
                    ? libadalang::explicitRangeBounds(dim0)
                    : std::nullopt;
  if (!bounds)
    return mlir::emitError(location,
                           "array index constraint must be a .. binary op");

  auto [lo_node, hi_node] = *bounds;
  if (!libadalang::isStaticExpr(lo_node) || !libadalang::isStaticExpr(hi_node))
    return mlir::emitError(location,
                           "array index constraint bounds must be static");
  auto lo = libadalang::evalExprAsInt(lo_node);
  auto hi = libadalang::evalExprAsInt(hi_node);
  if (!lo || !hi)
    return mlir::failure();

  auto extent = mlir::ada::arrayExtent(lo.value(), hi.value());
  if (!extent)
    return mlir::emitError(location, "array extent exceeds the layout limit");

  // The index constraint defines an anonymous index subtype (@rm{3-6}(13)),
  // emitted next to the array type so it shares its scope.
  auto indexSubtype = createSubtypeOp(
      makeUniqueInScope(mlir::ada::kAnonymousTypePrefix.str() +
                        mlir::ada::bareName(*typeName).str() + "_index_1"),
      indexOp, lo_node, hi_node, location);
  indexSym = mlir::FlatSymbolRefAttr::get(indexSubtype.getSymNameAttr());

  auto *ctx = builder.getContext();

  // Build the !ada.array machine type
  auto arrayType = mlir::ada::ArrayType::get(ctx, componentType, {indexType},
                                             {extent.value()});
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

mlir::LogicalResult MLIRGenImpl::mlirGenEnumTypeDecl(ada_node &type_decl,
                                                     bool external) {
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

    std::optional<int64_t> rep =
        isChar ? charLiteralRep(lit, location) : enumLiteralRep(lit, location);
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

ada_node MLIRGenImpl::resolveBooleanTypeDecl(ada_node &node,
                                             mlir::Location location) {
  ada_node bool_decl;
  if (!ada_ada_node_p_bool_type(&node, &bool_decl) ||
      ada_node_is_null(&bool_decl)) {
    mlir::emitError(location, "failed to resolve Boolean type");
    return {};
  }
  return bool_decl;
}

mlir::IntegerAttr MLIRGenImpl::minimalWidthIntAttr(const llvm::APInt &value) {
  return mlir::ada::minimalWidthIntAttr(builder.getContext(), value);
}

mlir::Attribute MLIRGenImpl::rangeBoundAttr(ada_node &bound) {
  if (!ada_node_is_null(&bound))
    if (auto value = libadalang::evalExprAsInt(bound))
      return minimalWidthIntAttr(*value);
  return mlir::UnitAttr::get(builder.getContext());
}

std::optional<uint32_t> MLIRGenImpl::evalFloatDigits(ada_node &type_decl,
                                                     mlir::Location location) {
  auto float_def = libadalang::typeDefOfKind(type_decl, ada_floating_point_def);
  if (!float_def)
    return 0;
  ada_node digits_expr;
  ada_big_integer bigint;
  if (!ada_floating_point_def_f_num_digits(&float_def.value(), &digits_expr) ||
      ada_node_is_null(&digits_expr) ||
      !ada_expr_p_eval_as_int(&digits_expr, &bigint)) {
    mlir::emitError(location, "failed to evaluate floating-point type digits");
    return std::nullopt;
  }
  auto value = libadalang::bigIntToAPSInt(bigint);
  // @todo Retrieve System.Max_Base_Digits and System.Max_Digits through
  //       Libadalang instead of hardcoding the precision limit.
  if (!value || value->ugt(18)) {
    mlir::emitError(loc(digits_expr),
                    "digits value out of range, maximum is 18");
    return std::nullopt;
  }
  return static_cast<uint32_t>(value->getZExtValue());
}

mlir::Type MLIRGenImpl::getMLIRTypeFromDecl(ada_node &type_decl,
                                            mlir::Location diagLoc) {
  // Follow the subtype chain to the canonical (base) type so that subtypes
  // of Integer map to the same MLIR type as Integer itself; fall back to the
  // declaration when resolution fails.
  ada_node canon_type =
      libadalang::canonicalType(type_decl).value_or(type_decl);

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

std::optional<mlir::Type>
MLIRGenImpl::getModularMLIRType(ada_node &canon_type, mlir::Location diagLoc) {
  if (!libadalang::typeDefOfKind(canon_type, ada_mod_int_type_def))
    return std::nullopt;

  auto it = typeDecls.find(canon_type.node);
  if (it == typeDecls.end()) {
    mlir::emitError(diagLoc, "modular type used before its declaration");
    return mlir::Type{};
  }
  return it->second.getMlirType();
}

std::optional<mlir::Type> MLIRGenImpl::getEnumMLIRType(ada_node &canon_type,
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
  unsigned bits =
      std::max(llvm::APInt(64, minRep, /*isSigned=*/true).getSignificantBits(),
               llvm::APInt(64, maxRep, /*isSigned=*/true).getSignificantBits());
  return builder.getIntegerType(bits);
}

std::optional<mlir::Type>
MLIRGenImpl::getArrayMLIRType(ada_node &canon_type, mlir::Location diagLoc) {
  if (!libadalang::isArrayTypeDecl(canon_type))
    return std::nullopt;

  // Search for the stored type.
  if (auto it = typeDecls.find(canon_type.node); it != typeDecls.end())
    return it->second.getMlirType();

  mlir::emitError(diagLoc, "array type used before its declaration");
  return mlir::Type{};
}

mlir::IntegerType MLIRGenImpl::getRuntimeIntegerType() {
  // @todo Derive the width and base range from System.Min_Int and
  //       System.Max_Int instead of assuming a signed 128 bit range.
  return builder.getIntegerType(128);
}

std::pair<llvm::APSInt, llvm::APSInt> MLIRGenImpl::getRuntimeIntegerBounds() {
  unsigned bits = getRuntimeIntegerType().getWidth();
  return {llvm::APSInt::getMinValue(bits, false),
          llvm::APSInt::getMaxValue(bits, false)};
}

std::optional<mlir::Type>
MLIRGenImpl::getIntegerMLIRType(ada_node &canon_type, mlir::Location diagLoc) {
  ada_bool is_int = false;
  if (libadalang::isUniversalTypeDecl(canon_type) ||
      !ada_base_type_decl_p_is_int_type(&canon_type, &libadalang::kNullOrigin,
                                        &is_int) ||
      !is_int)
    return std::nullopt;
  if (libadalang::isRootIntegerTypeDecl(canon_type))
    return getRuntimeIntegerType();
  ada_internal_discrete_range range;
  if (!ada_base_type_decl_p_discrete_range(&canon_type, &range) ||
      ada_node_is_null(&range.low_bound) || ada_node_is_null(&range.high_bound))
    return std::nullopt;
  std::optional<llvm::APSInt> lo = libadalang::evalExprAsInt(range.low_bound);
  std::optional<llvm::APSInt> hi = libadalang::evalExprAsInt(range.high_bound);
  if (!lo || !hi) {
    mlir::emitError(diagLoc, "failed to evaluate integer type bounds");
    return mlir::Type{};
  }
  unsigned bits = std::max(lo->getSignificantBits(), hi->getSignificantBits());
  // @todo Retrieve System.Min_Int and System.Max_Int through Libadalang
  //       to check the supported integer range (@rm{3-5-4}(6)).
  if (bits > 128) {
    mlir::emitError(diagLoc, "unsupported integer type wider than 128 bits");
    return mlir::Type{};
  }
  return builder.getIntegerType(llvm::bit_ceil(std::max(bits, 8u)));
}

std::optional<mlir::Type>
MLIRGenImpl::getFloatMLIRType(ada_node &canon_type, mlir::Location diagLoc) {
  ada_bool is_float = false;
  if (libadalang::isUniversalTypeDecl(canon_type) ||
      !ada_base_type_decl_p_is_float_type(&canon_type, &libadalang::kNullOrigin,
                                          &is_float) ||
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

mlir::Type MLIRGenImpl::getUniversalMLIRType(ada_node &canon_type,
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
    return getRuntimeIntegerType();
  if (name == libadalang::kUniversalRealTypeName)
    return mlir::Float128Type::get(builder.getContext());

  mlir::emitError(diagLoc, "unsupported Ada type '") << name << "'";
  return {};
}

} // namespace lalvm::mlirgen
