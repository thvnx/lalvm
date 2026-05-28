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
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
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
  if (!mlir::isa<mlir::IntegerType, mlir::FloatType>(type))
    return emitOpError() << "unsupported operand type " << type
                         << "; expected integer or float";
  return mlir::success();
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

/// Return the GNAT ABI name for this subprogram.
/// Operator symbols (e.g. `+`, `*`) are mapped to their GNAT O-names
/// (e.g. `Oadd`, `Omultiply`) via `gnatOperatorName`. Library-level
/// subprograms get the `_ada_` prefix; nested ones are qualified with
/// `__`-separated enclosing scope names (e.g. `outer__inner`).
std::string SubpOp::getMangledName() {
  std::string name = getName().str();
  if (name.size() <= 3)
    if (llvm::StringRef gnat =
            gnatOperatorName(name, getFunctionType().getNumInputs());
        !gnat.empty())
      name = gnat.str();
  if (mlir::isa<mlir::ModuleOp>((*this)->getParentOp()))
    return "_ada_" + name;
  // @todo BlockOp is skipped here and does not contribute to the mangled name.
  // Named blocks (RM 5.6) could use their name as a qualifier; unnamed ones
  // need a synthetic index (GNAT uses `B_N`, e.g. `outer__B_1__inner`) so
  // that sibling blocks declaring subprograms with the same name produce
  // distinct symbols.
  for (mlir::Operation *p = (*this)->getParentOp();
       mlir::isa<SubpOp, BlockOp>(p); p = p->getParentOp())
    if (mlir::isa<SubpOp>(p))
      name = mlir::SymbolTable::getSymbolName(p).str() + "__" + name;
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

/// `lookupNearestSymbolFrom` cannot be used here because SubpOp carries the
/// SymbolTable trait, making it an opaque scope boundary that hides sibling
/// nested subprograms. Walk up manually instead.
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
