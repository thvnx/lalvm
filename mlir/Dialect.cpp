//===- Dialect.cpp - Ada dialect registration in MLIR ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Ada dialect: custom assembly format and
// operation verification.
//
// NOTE: doxygen 1.15.0 limitation. Ops with more than one builder defined in
// this file trigger "no uniquely matching class member found" warnings. Doxygen
// scans Ops.h.inc globally and, when two overloads share the same parameter
// prefix, it cannot uniquely match each implementation to its declaration.
// Prefer a single builder per op to keep the generated docs warning-free.
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"

#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/Value.h"
#include "mlir/Interfaces/FunctionImplementation.h"
#include "mlir/Interfaces/MemorySlotInterfaces.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/Support/Casting.h"
#include <string>

using namespace mlir;
using namespace mlir::ada;

#include "ada/AdaOpsEnums.cpp.inc"

#define GET_ATTRDEF_CLASSES
#include "ada/Attrs.cpp.inc"

#define GET_TYPEDEF_CLASSES
#include "ada/Types.cpp.inc"

#include "ada/Dialect.cpp.inc"

//===----------------------------------------------------------------------===//
// AdaDialect
//===----------------------------------------------------------------------===//

/// Dialect initialization, the instance will be owned by the context. This is
/// the point of registration of types and operations for the dialect.
void AdaDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "ada/Ops.cpp.inc"
      >();
  addAttributes<
#define GET_ATTRDEF_LIST
#include "ada/Attrs.cpp.inc"
      >();
  addTypes<
#define GET_TYPEDEF_LIST
#include "ada/Types.cpp.inc"
      >();
}

//===----------------------------------------------------------------------===//
// EnumTypeInfoAttr
//===----------------------------------------------------------------------===//

std::optional<int64_t> EnumTypeInfoAttr::enumRep(llvm::StringRef name) const {
  for (auto [n, v] : literals())
    if (n == name)
      return v;
  return std::nullopt;
}

/// `value` as an IntegerAttr of minimal signed width (canonical at any width).
/// The encoding must match MLIRGen's `minimalWidthIntAttr`.
static mlir::IntegerAttr minWidthIntAttr(mlir::MLIRContext *ctx,
                                         const llvm::APInt &value) {
  unsigned bits = value.getSignificantBits();
  return mlir::IntegerAttr::get(mlir::IntegerType::get(ctx, bits),
                                value.sextOrTrunc(bits));
}

/// Shared `LO to HI` bounds syntax of the type info attributes (see
/// IntegerTypeInfoAttr for the encoding).
static mlir::ParseResult parseRangeBounds(mlir::AsmParser &parser,
                                          mlir::Attribute &lower,
                                          mlir::Attribute &upper) {
  auto parseBound = [&](mlir::Attribute &bound) -> mlir::ParseResult {
    if (succeeded(parser.parseOptionalQuestion())) {
      bound = mlir::UnitAttr::get(parser.getContext());
      return mlir::success();
    }
    llvm::APInt value;
    mlir::OptionalParseResult res = parser.parseOptionalInteger(value);
    if (!res.has_value() || failed(*res))
      return parser.emitError(parser.getCurrentLocation(),
                              "expected integer or `?` range bound");
    bound = minWidthIntAttr(parser.getContext(), value);
    return mlir::success();
  };
  if (parseBound(lower) || parser.parseKeyword("to") || parseBound(upper))
    return mlir::failure();
  return mlir::success();
}

static void printRangeBounds(mlir::AsmPrinter &p, mlir::IntegerAttr lower,
                             mlir::IntegerAttr upper) {
  auto printBound = [&](mlir::IntegerAttr bound) {
    if (bound)
      bound.getValue().print(p.getStream(), /*isSigned=*/true);
    else
      p << '?';
  };
  p << "range ";
  printBound(lower);
  p << " to ";
  printBound(upper);
}

/// Assembly format: `<"name1" = val1, "name2" = val2>` for an enumeration
/// declaration; `<range LO to HI>` for a constrained enum subtype (bounds
/// are representation values).
mlir::Attribute EnumTypeInfoAttr::parse(mlir::AsmParser &parser, mlir::Type) {
  llvm::SmallVector<mlir::Attribute> names;
  llvm::SmallVector<int64_t> values;
  mlir::Attribute lower, upper;

  if (parser.parseLess())
    return {};
  if (succeeded(parser.parseOptionalKeyword("range"))) {
    if (parseRangeBounds(parser, lower, upper))
      return {};
  } else if (parser.parseCommaSeparatedList([&]() -> mlir::ParseResult {
               std::string s;
               int64_t val;
               if (parser.parseString(&s) || parser.parseEqual() ||
                   parser.parseInteger(val))
                 return mlir::failure();
               names.push_back(mlir::StringAttr::get(parser.getContext(), s));
               values.push_back(val);
               return mlir::success();
             })) {
    return {};
  }
  if (parser.parseGreater())
    return {};

  return EnumTypeInfoAttr::get(parser.getContext(),
                               mlir::ArrayAttr::get(parser.getContext(), names),
                               values, lower, upper);
}

void EnumTypeInfoAttr::print(mlir::AsmPrinter &p) const {
  p << '<';
  if (hasRange()) {
    printRangeBounds(p, staticLower(), staticUpper());
  } else {
    llvm::interleaveComma(literals(), p.getStream(), [&](auto pair) {
      auto [name, val] = pair;
      p.printString(name);
      p << " = " << val;
    });
  }
  p << '>';
}

/// Assembly format: `<mod N>`, `<range LO to HI>`, `<mod N, range LO to HI>`,
/// or empty. LO/HI are integers, or `?` for a dynamic bound.
mlir::Attribute IntegerTypeInfoAttr::parse(mlir::AsmParser &parser,
                                           mlir::Type) {
  mlir::IntegerAttr modulus;
  mlir::Attribute lower, upper;

  if (failed(parser.parseOptionalLess()))
    return IntegerTypeInfoAttr::get(parser.getContext(), modulus, lower, upper);

  bool expectRange = true;
  if (succeeded(parser.parseOptionalKeyword("mod"))) {
    llvm::APInt value;
    mlir::OptionalParseResult res = parser.parseOptionalInteger(value);
    if (!res.has_value() || failed(*res))
      return {};
    modulus = minWidthIntAttr(parser.getContext(), value);
    expectRange = succeeded(parser.parseOptionalComma());
  }

  if (expectRange) {
    if (parser.parseKeyword("range") || parseRangeBounds(parser, lower, upper))
      return {};
  }

  if (parser.parseGreater())
    return {};
  return IntegerTypeInfoAttr::get(parser.getContext(), modulus, lower, upper);
}

void IntegerTypeInfoAttr::print(mlir::AsmPrinter &p) const {
  mlir::IntegerAttr modulus = getModulus();
  if (!modulus && !hasRange())
    return;
  p << '<';
  if (modulus) {
    p << "mod ";
    modulus.getValue().print(p.getStream(), /*isSigned=*/false);
    if (hasRange())
      p << ", ";
  }
  if (hasRange())
    printRangeBounds(p, staticLower(), staticUpper());
  p << '>';
}

//===----------------------------------------------------------------------===//
// TypeOp
//===----------------------------------------------------------------------===//

/// Builds a `TypeOp` with the given Ada type name, MLIR type, and metadata.
/// @param odsBuilder MLIR op builder; used to intern `name` as a `StringAttr`.
/// @param odsState   Operation construction state accumulating attributes.
/// @param name       Ada type declaration name.
/// @param mlirType   Corresponding MLIR type; wrapped in a `TypeAttr`.
/// @param typeInfo   Kind-specific metadata; one of the Ada dialect type info
///                   attributes, or null (unconstrained subtype).
/// @param base       Canonical base type symbol; null when the type is its
///                   own base.
void TypeOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   llvm::StringRef name, mlir::Type mlirType,
                   mlir::Attribute typeInfo, mlir::FlatSymbolRefAttr base) {
  state.addAttribute(getSymNameAttrName(state.name),
                     builder.getStringAttr(name));
  state.addAttribute(getMlirTypeAttrName(state.name),
                     mlir::TypeAttr::get(mlirType));
  if (typeInfo)
    state.addAttribute(getTypeInfoAttrName(state.name), typeInfo);
  if (base)
    state.addAttribute(getBaseAttrName(state.name), base);
}

/// Assembly format: `@sym_name (base @ref)? : mlir_type = type_info_attr`
mlir::ParseResult TypeOp::parse(mlir::OpAsmParser &parser,
                                mlir::OperationState &result) {
  mlir::StringAttr symName;
  if (parser.parseSymbolName(symName, getSymNameAttrName(result.name),
                             result.attributes))
    return mlir::failure();

  if (succeeded(parser.parseOptionalKeyword("base"))) {
    mlir::StringAttr baseName;
    if (parser.parseSymbolName(baseName))
      return mlir::failure();
    result.addAttribute(getBaseAttrName(result.name),
                        mlir::FlatSymbolRefAttr::get(baseName));
  }

  mlir::Type mlirType;
  if (parser.parseColon() || parser.parseType(mlirType))
    return mlir::failure();
  result.addAttribute(getMlirTypeAttrName(result.name),
                      mlir::TypeAttr::get(mlirType));

  if (succeeded(parser.parseOptionalEqual())) {
    mlir::Attribute typeInfo;
    if (parser.parseAttribute(typeInfo, getTypeInfoAttrName(result.name),
                              result.attributes))
      return mlir::failure();
  }

  return mlir::success();
}

void TypeOp::print(mlir::OpAsmPrinter &p) {
  p << ' ';
  p.printSymbolName(getSymName());
  if (auto base = getBaseAttr()) {
    p << " base ";
    p.printSymbolName(base.getValue());
  }
  p << " : ";
  p.printType(getMlirType());
  if (auto typeInfo = getTypeInfoAttr()) {
    p << " = ";
    p.printAttribute(typeInfo);
  }
}

llvm::LogicalResult TypeOp::verify() {
  if (mlir::Attribute info = getTypeInfoAttr()) {
    if (!mlir::isa<EnumTypeInfoAttr, IntegerTypeInfoAttr, FloatTypeInfoAttr>(
            info))
      return emitOpError() << "unsupported type_info attribute kind";
    bool kindMatches = mlir::isa<FloatTypeInfoAttr>(info)
                           ? mlir::isa<mlir::FloatType>(getMlirType())
                           : mlir::isa<mlir::IntegerType>(getMlirType());
    if (!kindMatches)
      return emitOpError() << "type_info kind does not match mlir_type";
  } else if (!getBaseAttr()) {
    return emitOpError()
           << "type without a base must carry a type_info attribute";
  }
  if (auto base = getBaseAttr()) {
    // Best-effort cross-check: hoisting moves type symbols across scopes
    // mid-pipeline, so an unresolved base is not an error here.
    auto *sym =
        mlir::SymbolTable::lookupNearestSymbolFrom(getOperation(), base);
    if (auto baseOp = mlir::dyn_cast_or_null<TypeOp>(sym)) {
      if (baseOp.getMlirType() != getMlirType())
        return emitOpError()
               << "base type '" << base.getValue()
               << "' has different mlir_type (" << baseOp.getMlirType()
               << " vs " << getMlirType() << ")";
    } else if (sym) {
      return emitOpError() << "base symbol '" << base.getValue()
                           << "' is not an ada.type";
    }
  }
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// AllocaOp
//===----------------------------------------------------------------------===//

llvm::LogicalResult AllocaOp::verify() {
  auto memrefType = mlir::cast<mlir::MemRefType>(getResult().getType());
  if (!mlir::isa<QualType>(memrefType.getElementType()))
    return emitOpError("result element type must be ada.qual");
  return mlir::success();
}

// PromotableAllocationOpInterface: declare the single memory slot owned by
// this alloca. Only entry-block allocas are eligible for mem2reg promotion.
llvm::SmallVector<mlir::MemorySlot> AllocaOp::getPromotableSlots() {
  if (!getOperation()->getBlock()->isEntryBlock())
    return {};
  auto elemType =
      mlir::cast<mlir::MemRefType>(getResult().getType()).getElementType();
  return {mlir::MemorySlot{getResult(), elemType}};
}

// PromotableAllocationOpInterface: provide a zero value of the slot type for
// uses that are reached before any write (uninitialized variable reads).
mlir::Value AllocaOp::getDefaultValue(const mlir::MemorySlot &slot,
                                      mlir::OpBuilder &builder) {
  auto typedType = mlir::cast<QualType>(slot.elemType);
  return ConstantOp::create(builder, getLoc(), typedType,
                            builder.getZeroAttr(typedType.getMlirType()));
}

// PromotableAllocationOpInterface: called when mem2reg introduces a block
// argument at a join point. The argument already has the right type and
// directly replaces slot uses, so no extra insertion is needed.
void AllocaOp::handleBlockArgument(const mlir::MemorySlot &slot,
                                   mlir::BlockArgument argument,
                                   mlir::OpBuilder &builder) {}

// PromotableAllocationOpInterface: called after promotion is complete. Erases
// the alloca and the default zero value if it was never used.
std::optional<mlir::PromotableAllocationOpInterface>
AllocaOp::handlePromotionComplete(const mlir::MemorySlot &slot,
                                  mlir::Value defaultValue,
                                  mlir::OpBuilder &builder) {
  if (defaultValue && defaultValue.use_empty())
    defaultValue.getDefiningOp()->erase();
  erase();
  return std::nullopt;
}

//===----------------------------------------------------------------------===//
// ConstantOp
//===----------------------------------------------------------------------===//

// Assembly format: `:` <ada.qual type> `=` <value>
//   ada.constant : !ada.qual<i32, @standard.integer> = 42
//   ada.constant : !ada.qual<f32, @standard.float> = 1.000000e+00
//   ada.constant : !ada.qual<f64, @standard.long_float> = 0x400921FB54442D18
mlir::ParseResult ConstantOp::parse(mlir::OpAsmParser &parser,
                                    mlir::OperationState &result) {
  if (parser.parseColon())
    return mlir::failure();
  mlir::SMLoc typeLoc = parser.getCurrentLocation();
  mlir::Type resultType;
  if (parser.parseType(resultType))
    return mlir::failure();
  auto typedType = mlir::dyn_cast<mlir::ada::QualType>(resultType);
  if (!typedType)
    return parser.emitError(typeLoc, "expected !ada.qual result type");
  result.addTypes(resultType);
  if (parser.parseEqual())
    return mlir::failure();
  mlir::Attribute valueAttr;
  if (parser.parseAttribute(valueAttr, typedType.getMlirType()))
    return mlir::failure();
  result.addAttribute(getValueAttrName(result.name), valueAttr);
  return mlir::success();
}

void ConstantOp::print(mlir::OpAsmPrinter &p) {
  p << " : " << getResult().getType() << " = ";
  p.printAttributeWithoutType(getValue());
}

llvm::LogicalResult ConstantOp::verify() {
  auto typedResult = mlir::cast<ada::QualType>(getResult().getType());
  // Kind before type: the parser types any attribute it accepts to the
  // result's machine type, so a `StringAttr` can arrive typed `i32`.
  if (!mlir::isa<mlir::IntegerAttr, mlir::FloatAttr>(getValue()))
    return emitOpError() << "value must be an integer or float attribute";
  auto typedAttr = mlir::cast<mlir::TypedAttr>(getValue());
  if (typedAttr.getType() != typedResult.getMlirType())
    return emitOpError() << "value type (" << typedAttr.getType()
                         << ") does not match result mlir type ("
                         << typedResult.getMlirType() << ")";
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// CoerceOp
//===----------------------------------------------------------------------===//

llvm::LogicalResult CoerceOp::verify() {
  auto inTyped = mlir::cast<ada::QualType>(getInput().getType());
  auto outTyped = mlir::cast<ada::QualType>(getResult().getType());
  if (inTyped == outTyped)
    return emitOpError() << "input and result types are identical; "
                            "use the value directly";
  bool intToInt = mlir::isa<mlir::IntegerType>(inTyped.getMlirType()) &&
                  mlir::isa<mlir::IntegerType>(outTyped.getMlirType());
  bool floatToFloat = mlir::isa<mlir::FloatType>(inTyped.getMlirType()) &&
                      mlir::isa<mlir::FloatType>(outTyped.getMlirType());
  if (!intToInt && !floatToFloat)
    return emitOpError() << "unsupported conversion: " << inTyped.getMlirType()
                         << " to " << outTyped.getMlirType();
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// RangeOp
//===----------------------------------------------------------------------===//

// Restrict the range's `boundType` to a scalar machine kind (matching
// `BinOp`/`CmpOp`) and pin the bounds' machine type to it. The bounds carry the
// base subtype identity, so the match is on the machine type alone.
llvm::LogicalResult RangeOp::verify() {
  mlir::Type boundType =
      mlir::cast<ada::RangeType>(getResult().getType()).getBoundType();
  if (!mlir::isa<mlir::IntegerType, mlir::FloatType>(boundType))
    return emitOpError() << "range bound type must be integer or float, got "
                         << boundType;
  mlir::Type boundMachineType =
      mlir::cast<ada::QualType>(getLow().getType()).getMlirType();
  if (boundMachineType != boundType)
    return emitOpError() << "bound machine type " << boundMachineType
                         << " does not match range bound type " << boundType;
  return mlir::success();
}

std::pair<mlir::TypedAttr, mlir::TypedAttr> RangeOp::staticBounds() {
  // A compile-time constant bound is defined by an `ada.constant`: read its
  // value attribute. A dynamic (runtime) bound has none, so it stays null.
  auto constOf = [](mlir::Value v) -> mlir::TypedAttr {
    if (auto c = v.getDefiningOp<ConstantOp>())
      return mlir::dyn_cast<mlir::TypedAttr>(c.getValue());
    return {};
  };
  return {constOf(getLow()), constOf(getHigh())};
}

//===----------------------------------------------------------------------===//
// RangeCheckOp
//===----------------------------------------------------------------------===//

mlir::OpFoldResult RangeCheckOp::fold(FoldAdaptor adaptor) {
  // Defense-in-depth only: MLIRGen resolves static checks at emission time and
  // never emits a statically-passing one, and the pipeline runs no
  // canonicalizer. Should a statically-passing check reach here anyway, drop
  // it by forwarding the value, but only when the bounds and the value are
  // all compile-time constants and the value provably lies within range.
  auto rangeOp = getRange().getDefiningOp<RangeOp>();
  if (!rangeOp)
    return {};
  auto [loAttr, hiAttr] = rangeOp.staticBounds();
  mlir::Attribute valAttr = adaptor.getValue();
  if (!loAttr || !hiAttr || !valAttr)
    return {};

  if (auto lo = mlir::dyn_cast<mlir::IntegerAttr>(loAttr)) {
    auto hi = mlir::dyn_cast<mlir::IntegerAttr>(hiAttr);
    auto v = mlir::dyn_cast<mlir::IntegerAttr>(valAttr);
    if (!hi || !v)
      return {};
    const llvm::APInt &l = lo.getValue(), &h = hi.getValue(), &x = v.getValue();
    // Bounds are signless `iN`. Without the subtype's signedness, forward only
    // when the value is in range under both signed and unsigned readings;
    // sound either way, at the cost of not folding some negative-bound ranges
    // (acceptable for a defense-in-depth fold).
    if (l.sle(x) && x.sle(h) && l.ule(x) && x.ule(h))
      return getValue();
    return {};
  }
  if (auto lo = mlir::dyn_cast<mlir::FloatAttr>(loAttr)) {
    auto hi = mlir::dyn_cast<mlir::FloatAttr>(hiAttr);
    auto v = mlir::dyn_cast<mlir::FloatAttr>(valAttr);
    if (!hi || !v)
      return {};
    using llvm::APFloat;
    APFloat::cmpResult loCmp = v.getValue().compare(lo.getValue());
    APFloat::cmpResult hiCmp = v.getValue().compare(hi.getValue());
    // Positive tests so an unordered compare (NaN) does not fold.
    bool geLo = loCmp == APFloat::cmpGreaterThan || loCmp == APFloat::cmpEqual;
    bool leHi = hiCmp == APFloat::cmpLessThan || hiCmp == APFloat::cmpEqual;
    if (geLo && leHi)
      return getValue();
  }
  return {};
}

//===----------------------------------------------------------------------===//
// AttrOp
//===----------------------------------------------------------------------===//

llvm::LogicalResult AttrOp::verify() {
  // The only bound-reading attributes modelled so far (@rm{3-5}); names are
  // lowercased, as Ada identifiers are normalized elsewhere.
  if (getName() != "first" && getName() != "last")
    return emitOpError() << "unsupported attribute '" << getName()
                         << "'; expected \"first\" or \"last\"";
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// Operator assembly-format directives
//===----------------------------------------------------------------------===//

// `custom<Operator>($kind)`: the predefined operator as a quoted keyword
// (`"+"`, `"="`, `"not"`) -- the symbol/enum mapping is the only part of these
// ops not expressible declaratively. One directive, overloaded per operator
// enum, shared by `binop`, `cmp`, and `unop`.
static mlir::ParseResult parseOperator(mlir::OpAsmParser &parser,
                                       ada::AdaBinaryOpAttr &kind) {
  std::string sym;
  SMLoc loc = parser.getCurrentLocation();
  if (parser.parseString(&sym))
    return mlir::failure();
  auto k = ada::symbolizeAdaBinaryOp(sym);
  if (!k)
    return parser.emitError(loc, "unknown binary operator '") << sym << "'";
  kind = ada::AdaBinaryOpAttr::get(parser.getContext(), *k);
  return mlir::success();
}
static mlir::ParseResult parseOperator(mlir::OpAsmParser &parser,
                                       ada::AdaRelationalOpAttr &kind) {
  std::string sym;
  SMLoc loc = parser.getCurrentLocation();
  if (parser.parseString(&sym))
    return mlir::failure();
  auto k = ada::symbolizeAdaRelationalOp(sym);
  if (!k)
    return parser.emitError(loc, "unknown relational operator '") << sym << "'";
  kind = ada::AdaRelationalOpAttr::get(parser.getContext(), *k);
  return mlir::success();
}
static mlir::ParseResult parseOperator(mlir::OpAsmParser &parser,
                                       ada::AdaUnaryOpAttr &kind) {
  std::string sym;
  SMLoc loc = parser.getCurrentLocation();
  if (parser.parseString(&sym))
    return mlir::failure();
  auto k = ada::symbolizeAdaUnaryOp(sym);
  if (!k)
    return parser.emitError(loc, "unknown unary operator '") << sym << "'";
  kind = ada::AdaUnaryOpAttr::get(parser.getContext(), *k);
  return mlir::success();
}

static void printOperator(mlir::OpAsmPrinter &p, mlir::Operation *,
                          ada::AdaBinaryOpAttr kind) {
  p.printString(ada::stringifyAdaBinaryOp(kind.getValue()));
}
static void printOperator(mlir::OpAsmPrinter &p, mlir::Operation *,
                          ada::AdaRelationalOpAttr kind) {
  p.printString(ada::stringifyAdaRelationalOp(kind.getValue()));
}
static void printOperator(mlir::OpAsmPrinter &p, mlir::Operation *,
                          ada::AdaUnaryOpAttr kind) {
  p.printString(ada::stringifyAdaUnaryOp(kind.getValue()));
}

// `custom<Checks>($checks)`: the optional `checks<overflow|...>` group on
// `ada.binop` (`|`-separated, matching the bit enum's spelling). An absent
// group leaves the attribute null.
static mlir::ParseResult parseChecks(mlir::OpAsmParser &parser,
                                     ada::AdaChecksAttr &checks) {
  if (failed(parser.parseOptionalKeyword("checks")))
    return mlir::success();
  auto bits = ada::AdaChecks{};
  if (parser.parseLess())
    return mlir::failure();
  do {
    llvm::StringRef flag;
    SMLoc flagLoc = parser.getCurrentLocation();
    if (parser.parseKeyword(&flag))
      return mlir::failure();
    auto bit = ada::symbolizeAdaChecks(flag);
    if (!bit)
      return parser.emitError(flagLoc, "unknown check '") << flag << "'";
    bits = bits | *bit;
  } while (succeeded(parser.parseOptionalVerticalBar()));
  if (parser.parseGreater())
    return mlir::failure();
  checks = ada::AdaChecksAttr::get(parser.getContext(), bits);
  return mlir::success();
}
static void printChecks(mlir::OpAsmPrinter &p, mlir::Operation *,
                        ada::AdaChecksAttr checks) {
  if (checks && checks.getValue() != ada::AdaChecks{})
    p << "checks<" << ada::stringifyAdaChecks(checks.getValue()) << ">";
}

//===----------------------------------------------------------------------===//
// BinOp
//===----------------------------------------------------------------------===//

llvm::LogicalResult BinOp::verify() {
  // SameOperandsAndResultType guarantees all operands share this type.
  mlir::Type type = getLhs().getType();
  if (auto typedType = mlir::dyn_cast<ada::QualType>(type))
    type = typedType.getMlirType();
  if (!mlir::isa<mlir::IntegerType, mlir::FloatType>(type))
    return emitOpError() << "unsupported operand type " << type
                         << "; expected integer or float";
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// CmpOp
//===----------------------------------------------------------------------===//

llvm::LogicalResult CmpOp::verify() {
  // SameTypeOperands guarantees both operands share this type.
  mlir::Type operandType = getLhs().getType();
  if (auto typedType = mlir::dyn_cast<ada::QualType>(operandType))
    operandType = typedType.getMlirType();
  if (!mlir::isa<mlir::IntegerType, mlir::FloatType>(operandType))
    return emitOpError() << "unsupported operand type " << operandType
                         << "; expected integer or float";

  // Relational operators yield the predefined type Boolean (@rm{4-5-2}).
  mlir::Type resultType = getResult().getType();
  if (auto typedType = mlir::dyn_cast<ada::QualType>(resultType))
    resultType = typedType.getMlirType();
  if (!resultType.isInteger(1))
    return emitOpError() << "result must be Boolean (i1), got " << resultType;
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// UnOp
//===----------------------------------------------------------------------===//

llvm::LogicalResult UnOp::verify() {
  // SameOperandsAndResultType guarantees the operand shares the result type.
  mlir::Type type = getOperand().getType();
  if (auto typedType = mlir::dyn_cast<ada::QualType>(type))
    type = typedType.getMlirType();
  if (!mlir::isa<mlir::IntegerType>(type))
    return emitOpError() << "unsupported operand type " << type
                         << "; expected integer (Boolean)";
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// UnwrapOp
//===----------------------------------------------------------------------===//

llvm::LogicalResult UnwrapOp::verify() {
  auto qual = mlir::dyn_cast<ada::QualType>(getValue().getType());
  if (!qual)
    return emitOpError() << "operand must be an ada.qual type, got "
                         << getValue().getType();
  if (getResult().getType() != qual.getMlirType())
    return emitOpError() << "result type " << getResult().getType()
                         << " must be the operand's underlying type "
                         << qual.getMlirType();
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// bareName
//===----------------------------------------------------------------------===//

llvm::StringRef mlir::ada::bareName(llvm::StringRef qualified) {
  llvm::StringRef seg = qualified.rsplit('.').second;
  if (seg.empty())
    seg = qualified;
  // Drop the collision suffix added by MLIRGen's makeUnique. Only LALVM ever
  // introduces a double underscore; canonical Ada names never contain
  // consecutive underscores, so its presence unambiguously marks the suffix.
  size_t pos = seg.rfind("__");
  if (pos != llvm::StringRef::npos)
    seg = seg.substr(0, pos);
  return seg;
}

//===----------------------------------------------------------------------===//
// SubpOp
//===----------------------------------------------------------------------===//

/// Map a bare Ada operator symbol to the corresponding GNAT O-name.
/// `numArgs` disambiguates `+`/`-` (1 = unary, 2 = binary).
/// Returns an empty StringRef for non-operator symbols.
static llvm::StringRef gnatOperatorName(llvm::StringRef sym, unsigned numArgs) {
  if (sym == "+")
    return numArgs == 1 ? "Oplus" : "Oadd";
  if (sym == "-")
    return numArgs == 1 ? "Ominus" : "Osubtract";
  return llvm::StringSwitch<llvm::StringRef>(sym)
      .Case("&", "Oconcat")
      .Case("*", "Omultiply")
      .Case("**", "Oexpon")
      .Case("/", "Odivide")
      .Case("/=", "One")
      .Case("<", "Olt")
      .Case("<=", "Ole")
      .Case("=", "Oeq")
      .Case(">", "Ogt")
      .Case(">=", "Oge")
      .Case("abs", "Oabs")
      .Case("and", "Oand")
      .Case("mod", "Omod")
      .Case("not", "Onot")
      .Case("or", "Oor")
      .Case("rem", "Orem")
      .Case("xor", "Oxor")
      .Default("");
}

/// Return the GNAT ABI name for this subprogram, derived from the unique
/// qualified dialect sym_name: split on the dot, map operator segments to their
/// GNAT O-names (e.g. + to Oadd) via gnatOperatorName, and join with double
/// underscores (e.g. proc.b.inner to proc__b__inner). Library-level
/// subprograms (public symbol visibility) get the _ada_ prefix; reading
/// visibility rather than the op's parent keeps the name stable across passes
/// that move the op to module level.
std::string SubpOp::getMangledName() {
  llvm::SmallVector<llvm::StringRef> segs;
  getSymName().split(segs, '.');
  std::string name;
  for (size_t i = 0; i < segs.size(); ++i) {
    std::string seg = segs[i].str();
    // Arity is only known for the leaf segment (this op); a non-leaf operator
    // scope is rare and defaults to binary.
    unsigned arity =
        (i + 1 == segs.size()) ? getFunctionType().getNumInputs() : 2;
    if (seg.size() <= 3)
      if (llvm::StringRef gnat = gnatOperatorName(seg, arity); !gnat.empty())
        seg = gnat.str();
    if (!name.empty())
      name += "__";
    name += seg;
  }
  if (isPublic())
    return "_ada_" + name;
  return name;
}

/// Builds a `SubpOp` and creates its entry block with arguments matching the
/// function type inputs.
/// @param odsBuilder MLIR op builder.
/// @param odsState   Operation construction state accumulating attributes.
/// @param name       Ada subprogram name (bare, no ABI mangling).
/// @param type       Function type; empty result list for procedures, one
///                   result type for functions.
/// @param attrs      Additional named attributes (e.g. arg/res attrs).
void SubpOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   llvm::StringRef name, mlir::FunctionType type,
                   llvm::ArrayRef<mlir::NamedAttribute> attrs) {
  buildWithEntryBlock(builder, state, name, type, attrs, type.getInputs());
}

/// Assembly format: standard MLIR function syntax (name, argument list,
/// optional `->` result type, attribute dictionary, region).
mlir::ParseResult SubpOp::parse(mlir::OpAsmParser &parser,
                                mlir::OperationState &result) {
  auto buildFuncType =
      [](mlir::Builder &builder, llvm::ArrayRef<mlir::Type> argTypes,
         llvm::ArrayRef<mlir::Type> results,
         mlir::function_interface_impl::VariadicFlag,
         std::string &) { return builder.getFunctionType(argTypes, results); };
  return mlir::function_interface_impl::parseFunctionOp(
      parser, result, /*allowVariadic=*/false,
      getFunctionTypeAttrName(result.name), buildFuncType,
      getArgAttrsAttrName(result.name), getResAttrsAttrName(result.name));
}

void SubpOp::print(mlir::OpAsmPrinter &p) {
  mlir::function_interface_impl::printFunctionOp(
      p, *this, /*isVariadic=*/false, getFunctionTypeAttrName(),
      getArgAttrsAttrName(), getResAttrsAttrName());
}

//===----------------------------------------------------------------------===//
// CallOp
//===----------------------------------------------------------------------===//

/// Resolve the nested symbol `name` visible from `from`, walking enclosing
/// scopes up to the module. `lookupNearestSymbolFrom` cannot be used because
/// `SubpOp` is not itself a SymbolTable: its nested symbols (`ada.subp`,
/// `ada.type`) live in an interior `ada.decls`, a sibling of the statements
/// rather than an ancestor, so at each enclosing `SubpOp` the walk searches
/// that subprogram's `ada.decls` children too. Used for both call and target
/// type resolution during MLIRGen.
mlir::Operation *mlir::ada::lookupSymbolFrom(mlir::Operation *from,
                                             llvm::StringRef name) {
  for (mlir::Operation *scope = from; scope; scope = scope->getParentOp()) {
    if (mlir::isa<DeclsOp, mlir::ModuleOp>(scope))
      if (auto *sym = mlir::SymbolTable::lookupSymbolIn(scope, name))
        return sym;
    if (auto subp = mlir::dyn_cast<SubpOp>(scope))
      for (mlir::Block &block : subp.getBody())
        for (mlir::Operation &op : block)
          if (auto decls = mlir::dyn_cast<DeclsOp>(&op))
            if (auto *sym = mlir::SymbolTable::lookupSymbolIn(decls, name))
              return sym;
  }
  return nullptr;
}

/// Callee resolution for `verifySymbolUses`'s best-effort
/// procedure-vs-function check; MLIRGen resolves calls by node identity, not
/// through this.
mlir::Operation *CallOp::lookupCallee(mlir::Operation *from,
                                      llvm::StringRef name) {
  return mlir::ada::lookupSymbolFrom(from, name);
}

llvm::LogicalResult CallOp::verifySymbolUses(mlir::SymbolTableCollection &) {
  // External callees (defined in other Ada units) are not in this module and
  // will not be found; their signature is validated by Libadalang before
  // MLIRGen runs. Link-time resolution handles the rest.
  auto subp = mlir::dyn_cast_or_null<SubpOp>(lookupCallee(*this, getCallee()));
  if (!subp)
    return mlir::success();

  if (subp.isProcedure() && getResult())
    return emitOpError() << "callee '" << getCallee()
                         << "' is a procedure but call carries a result";
  if (subp.isFunction() && !getResult())
    return emitOpError() << "callee '" << getCallee()
                         << "' is a function but call carries no result";
  return mlir::success();
}

/// Builds a `CallOp`. Pass a null `resultType` for procedure calls (no
/// result) and a non-null type for function calls (one result).
void CallOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   mlir::FlatSymbolRefAttr callee, mlir::Type resultType,
                   mlir::ValueRange operands) {
  if (resultType)
    state.addTypes(resultType);
  state.addAttribute(getCalleeAttrName(state.name), callee);
  state.addOperands(operands);
}

//===----------------------------------------------------------------------===//
// ReturnOp
//===----------------------------------------------------------------------===//

llvm::LogicalResult ReturnOp::verify() {
  // The HasParent<ada.subp> trait already verified the parent is an ada.subp.
  auto subp = mlir::cast<SubpOp>((*this)->getParentOp());
  mlir::FunctionType funcType = subp.getFunctionType();
  const auto &results = funcType.getResults();
  if (getNumOperands() != results.size())
    return emitOpError() << "does not return the same number of values ("
                         << getNumOperands() << ") as the enclosing function ("
                         << results.size() << ")";
  if (!getInput())
    return mlir::success();

  auto inputType = getInput().getType();
  auto resultType = results.front();
  if (inputType != resultType)
    return emitOpError() << "type of return operand (" << inputType
                         << ") doesn't match function result type ("
                         << resultType << ")";
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// TableGen'd op method definitions
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "ada/Ops.cpp.inc"
