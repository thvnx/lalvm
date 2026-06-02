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
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
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

/// Assembly format: <"name1" = val1, "name2" = val2>
mlir::Attribute EnumTypeInfoAttr::parse(mlir::AsmParser &parser, mlir::Type) {
  llvm::SmallVector<mlir::Attribute> names;
  llvm::SmallVector<int64_t> values;

  if (parser.parseCommaSeparatedList(
          mlir::AsmParser::Delimiter::LessGreater, [&]() -> mlir::ParseResult {
            std::string s;
            int64_t val;
            if (parser.parseString(&s) || parser.parseEqual() ||
                parser.parseInteger(val))
              return mlir::failure();
            names.push_back(mlir::StringAttr::get(parser.getContext(), s));
            values.push_back(val);
            return mlir::success();
          }))
    return {};

  return EnumTypeInfoAttr::get(parser.getContext(),
                               mlir::ArrayAttr::get(parser.getContext(), names),
                               values);
}

void EnumTypeInfoAttr::print(mlir::AsmPrinter &p) const {
  p << '<';
  llvm::interleaveComma(literals(), p.getStream(), [&](auto pair) {
    auto [name, val] = pair;
    p.printString(name);
    p << " = " << val;
  });
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
///                   attributes.
void TypeOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   llvm::StringRef name, mlir::Type mlirType,
                   mlir::Attribute typeInfo) {
  state.addAttribute(getSymNameAttrName(state.name),
                     builder.getStringAttr(name));
  state.addAttribute(getMlirTypeAttrName(state.name),
                     mlir::TypeAttr::get(mlirType));
  state.addAttribute(getTypeInfoAttrName(state.name), typeInfo);
}

/// Assembly format: `@sym_name : mlir_type = type_info_attr`
mlir::ParseResult TypeOp::parse(mlir::OpAsmParser &parser,
                                mlir::OperationState &result) {
  mlir::StringAttr symName;
  if (parser.parseSymbolName(symName, getSymNameAttrName(result.name),
                             result.attributes))
    return mlir::failure();

  mlir::Type mlirType;
  if (parser.parseColon() || parser.parseType(mlirType))
    return mlir::failure();
  result.addAttribute(getMlirTypeAttrName(result.name),
                      mlir::TypeAttr::get(mlirType));

  if (parser.parseEqual())
    return mlir::failure();
  mlir::Attribute typeInfo;
  if (parser.parseAttribute(typeInfo, getTypeInfoAttrName(result.name),
                            result.attributes))
    return mlir::failure();

  return mlir::success();
}

void TypeOp::print(mlir::OpAsmPrinter &p) {
  p << ' ';
  p.printSymbolName(getSymName());
  p << " : ";
  p.printType(getMlirType());
  p << " = ";
  p.printAttribute(getTypeInfo());
}

llvm::LogicalResult TypeOp::verify() {
  if (!mlir::isa<EnumTypeInfoAttr, NumericTypeInfoAttr>(getTypeInfo()))
    return emitOpError() << "unsupported type_info attribute kind";
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
  return builder.create<ConstantOp>(
      getLoc(), typedType, builder.getZeroAttr(typedType.getMlirType()));
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
  auto typedAttr = mlir::dyn_cast<mlir::TypedAttr>(getValue());
  if (!typedAttr)
    return emitOpError() << "value attribute must be a typed attribute";
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
// BinOp
//===----------------------------------------------------------------------===//

/// Accepts two equivalent text formats:
///   %0 = ada.binop "+" %a, %b : i32              (all types identical)
///   %0 = ada.binop "+" %a, %b : (i32, i32) -> i32  (functional form)
mlir::ParseResult BinOp::parse(mlir::OpAsmParser &parser,
                               mlir::OperationState &result) {
  std::string sym;
  SMLoc symLoc = parser.getCurrentLocation();
  if (parser.parseString(&sym))
    return mlir::failure();

  auto kind = ada::symbolizeAdaBinaryOp(sym);
  if (!kind)
    return parser.emitError(symLoc, "unknown binary operator '") << sym << "'";

  result.addAttribute("kind",
                      ada::AdaBinaryOpAttr::get(parser.getContext(), *kind));

  SmallVector<mlir::OpAsmParser::UnresolvedOperand, 2> operands;
  SMLoc operandsLoc = parser.getCurrentLocation();
  Type type;
  if (parser.parseOperandList(operands, /*requiredOperandCount=*/2) ||
      parser.parseOptionalAttrDict(result.attributes) ||
      parser.parseColonType(type))
    return mlir::failure();

  // Functional form: `(i32, i32) -> i32` — resolve operands and result
  // separately.
  if (FunctionType funcType = llvm::dyn_cast<FunctionType>(type)) {
    if (parser.resolveOperands(operands, funcType.getInputs(), operandsLoc,
                               result.operands))
      return mlir::failure();
    result.addTypes(funcType.getResults());
    return mlir::success();
  }

  if (parser.resolveOperands(operands, type, result.operands))
    return mlir::failure();
  result.addTypes(type);
  return mlir::success();
}

void BinOp::print(mlir::OpAsmPrinter &p) {
  p << " ";
  p.printString(ada::stringifyAdaBinaryOp(getKind()));
  p << " " << getOperands();
  p.printOptionalAttrDict((*this)->getAttrs(), /*elidedAttrs=*/{"kind"});
  p << " : " << getResult().getType();
}

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
/// subprograms (whose parent is the module) get the _ada_ prefix; this relies
/// on the op's parent, so nested subprograms must be mangled before hoisting
/// moves them to module level.
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
  if (mlir::isa<mlir::ModuleOp>((*this)->getParentOp()))
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

/// Resolve a callee symbol by walking the enclosing SymbolTable scopes from
/// `from` up to the module. Used only by `verifySymbolUses` for a best-effort
/// procedure-vs-function check; MLIRGen resolves calls by node identity, not
/// through this. `lookupNearestSymbolFrom` cannot be used because SubpOp
/// carries the SymbolTable trait, an opaque scope boundary, so the walk visits
/// each scope explicitly.
mlir::Operation *CallOp::lookupCallee(mlir::Operation *from,
                                      llvm::StringRef name) {
  for (mlir::Operation *scope = from; scope; scope = scope->getParentOp()) {
    if (mlir::isa<BlockOp, SubpOp, mlir::ModuleOp>(scope))
      if (auto *sym = mlir::SymbolTable::lookupSymbolIn(scope, name))
        return sym;
  }
  return nullptr;
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
  // Walk up through any enclosing block statements to find the subprogram.
  mlir::Operation *parent = (*this)->getParentOp();
  while (parent && mlir::isa<BlockOp>(parent))
    parent = parent->getParentOp();
  auto subp = mlir::dyn_cast<SubpOp>(parent);
  if (!subp)
    return emitOpError() << "expects parent to be ada.subp";
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
