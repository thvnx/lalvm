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
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"

#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
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

/// Assembly format: <"name1" = val1, "name2" = val2>
mlir::Attribute EnumTypeInfoAttr::parse(mlir::AsmParser &parser, mlir::Type) {
  if (parser.parseLess())
    return {};

  llvm::SmallVector<std::string> nameStorage;
  llvm::SmallVector<int64_t> values;

  if (parser.parseCommaSeparatedList([&]() -> mlir::ParseResult {
        std::string s;
        int64_t val;
        if (parser.parseString(&s) || parser.parseEqual() ||
            parser.parseInteger(val))
          return mlir::failure();
        nameStorage.push_back(std::move(s));
        values.push_back(val);
        return mlir::success();
      }))
    return {};

  if (parser.parseGreater())
    return {};

  llvm::SmallVector<llvm::StringRef> names(nameStorage.begin(),
                                           nameStorage.end());
  return EnumTypeInfoAttr::get(parser.getContext(), names, values);
}

void EnumTypeInfoAttr::print(mlir::AsmPrinter &p) const {
  p << '<';
  llvm::interleaveComma(llvm::zip(getNames(), getValues()), p.getStream(),
                        [&](auto pair) {
                          auto [name, val] = pair;
                          p << '"' << name << "\" = " << val;
                        });
  p << '>';
}

//===----------------------------------------------------------------------===//
// TypeOp
//===----------------------------------------------------------------------===//

void TypeOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   llvm::StringRef name, mlir::Type mlirType,
                   mlir::Attribute typeInfo) {
  state.addAttribute(getSymNameAttrName(state.name),
                     builder.getStringAttr(name));
  state.addAttribute(getMlirTypeAttrName(state.name),
                     mlir::TypeAttr::get(mlirType));
  state.addAttribute(getTypeInfoAttrName(state.name), typeInfo);
}

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
  p << " : " << getMlirType() << " = ";
  p.printAttribute(getTypeInfo());
}

llvm::LogicalResult TypeOp::verify() {
  if (!mlir::isa<EnumTypeInfoAttr>(getTypeInfo()))
    return emitOpError() << "unsupported type_info attribute kind";
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// ConstantOp
//===----------------------------------------------------------------------===//

void ConstantOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                       mlir::IntegerAttr value, llvm::StringRef adaType) {
  state.addTypes(value.getType());
  state.addAttribute(getValueAttrName(state.name), value);
  state.addAttribute(getAdaTypeAttrName(state.name),
                     mlir::SymbolRefAttr::get(builder.getContext(), adaType));
}

/// Assembly format: @ada_type value : type
mlir::ParseResult ConstantOp::parse(mlir::OpAsmParser &parser,
                                    mlir::OperationState &result) {
  mlir::FlatSymbolRefAttr adaType;
  if (parser.parseAttribute(adaType, getAdaTypeAttrName(result.name),
                            result.attributes))
    return mlir::failure();

  mlir::IntegerAttr value;
  if (parser.parseAttribute(value, getValueAttrName(result.name),
                            result.attributes))
    return mlir::failure();

  result.addTypes(value.getType());
  return mlir::success();
}

void ConstantOp::print(mlir::OpAsmPrinter &p) {
  p << ' ';
  p.printAttribute(getAdaTypeAttr());
  p << ' ';
  p.printAttribute(getValueAttr());
}

llvm::LogicalResult ConstantOp::verify() {
  auto intAttr = mlir::cast<mlir::IntegerAttr>(getValueAttr());
  if (intAttr.getType() != getResult().getType())
    return emitOpError() << "value type " << intAttr.getType()
                         << " does not match result type "
                         << getResult().getType();
  return mlir::success();
}

// ada.subp carries SymbolTable, making it an opaque scope boundary: a plain
// FlatSymbolRefAttr on a ConstantOp inside a subp body is only resolved
// against that subp's own SymbolTable, missing module-level ada.type ops
// (e.g. standard.boolean). Walk the full parent chain explicitly instead.
llvm::LogicalResult
ConstantOp::verifySymbolUses(mlir::SymbolTableCollection &) {
  auto name = mlir::StringAttr::get(getContext(), getAdaType());
  for (mlir::Operation *cur = getOperation(); cur;) {
    mlir::Operation *table = mlir::SymbolTable::getNearestSymbolTable(cur);
    if (!table)
      break;
    if (mlir::SymbolTable::lookupSymbolIn(table, name))
      return mlir::success();
    cur = table->getParentOp();
  }
  return emitOpError() << "unknown ada.type '" << getAdaType() << "'";
}

//===----------------------------------------------------------------------===//
// Ada Operations
//===----------------------------------------------------------------------===//

/// Shared verifier for add/sub/mul: rejects types that have no corresponding
/// arith lowering (the lowering dispatches on IntegerType vs FloatType).
static llvm::LogicalResult verifyNumericOp(mlir::Operation *op) {
  mlir::Type type = op->getOperand(0).getType();
  if (!mlir::isa<mlir::IntegerType, mlir::FloatType>(type))
    return op->emitOpError() << "unsupported operand type " << type
                             << "; expected integer or float";
  return mlir::success();
}

/// Shared operand+type parser for BinOp. Accepts two equivalent text formats:
///   %0 = ada.binop "+" %a, %b : i32              (all types identical)
///   %0 = ada.binop "+" %a, %b : (i32, i32) -> i32  (functional form)
static mlir::ParseResult parseBinaryOp(mlir::OpAsmParser &parser,
                                       mlir::OperationState &result) {
  SmallVector<mlir::OpAsmParser::UnresolvedOperand, 2> operands;
  SMLoc operandsLoc = parser.getCurrentLocation();
  Type type;
  if (parser.parseOperandList(operands, /*requiredOperandCount=*/2) ||
      parser.parseOptionalAttrDict(result.attributes) ||
      parser.parseColonType(type))
    return mlir::failure();

  // If the type is a function type, it contains the input and result types of
  // this operation.
  if (FunctionType funcType = llvm::dyn_cast<FunctionType>(type)) {
    if (parser.resolveOperands(operands, funcType.getInputs(), operandsLoc,
                               result.operands))
      return mlir::failure();
    result.addTypes(funcType.getResults());
    return mlir::success();
  }

  // Otherwise, the parsed type is the type of both operands and results.
  if (parser.resolveOperands(operands, type, result.operands))
    return mlir::failure();
  result.addTypes(type);
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// BinOp
//===----------------------------------------------------------------------===//

mlir::ParseResult BinOp::parse(mlir::OpAsmParser &parser,
                               mlir::OperationState &result) {
  // Parse the quoted Ada operator symbol: "+", "-", or "*".
  std::string sym;
  SMLoc symLoc = parser.getCurrentLocation();
  if (parser.parseString(&sym))
    return mlir::failure();

  auto kind = ada::symbolizeAdaBinaryOp(sym);
  if (!kind)
    return parser.emitError(symLoc, "unknown binary operator '") << sym << "'";

  result.addAttribute("kind",
                      ada::AdaBinaryOpAttr::get(parser.getContext(), *kind));

  return parseBinaryOp(parser, result);
}

void BinOp::print(mlir::OpAsmPrinter &p) {
  p << " \"" << ada::stringifyAdaBinaryOp(getKind()) << "\"";
  p << " " << getOperands();
  p.printOptionalAttrDict((*this)->getAttrs(), /*elidedAttrs=*/{"kind"});
  p << " : ";
  mlir::Type resultType = getResult().getType();
  if (llvm::all_of(getOperandTypes(),
                   [=](mlir::Type t) { return t == resultType; }))
    p << resultType;
  else
    p.printFunctionalType(getOperandTypes(), (*this)->getResultTypes());
}

llvm::LogicalResult BinOp::verify() { return verifyNumericOp(*this); }

//===----------------------------------------------------------------------===//
// SubpOp
//===----------------------------------------------------------------------===//

void SubpOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   llvm::StringRef name, mlir::FunctionType type,
                   llvm::ArrayRef<mlir::NamedAttribute> attrs) {
  buildWithEntryBlock(builder, state, name, type, attrs, type.getInputs());
}

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

void CallOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   llvm::StringRef callee, mlir::ValueRange operands) {
  assert(state.types.empty() && "procedure call must have no result type");
  state.addAttribute(getCalleeAttrName(state.name),
                     mlir::SymbolRefAttr::get(builder.getContext(), callee));
  state.addOperands(operands);
}

void CallOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   llvm::StringRef callee, mlir::Type resultType,
                   mlir::ValueRange operands) {
  assert(resultType && "function call must have a valid result type");
  state.addTypes(resultType);
  state.addAttribute(getCalleeAttrName(state.name),
                     mlir::SymbolRefAttr::get(builder.getContext(), callee));
  state.addOperands(operands);
}

//===----------------------------------------------------------------------===//
// ReturnOp
//===----------------------------------------------------------------------===//

// ada.return operand is optional: ada.subp functions carry one, procedures
// none.
mlir::ParseResult ReturnOp::parse(mlir::OpAsmParser &parser,
                                  mlir::OperationState &result) {
  mlir::OpAsmParser::UnresolvedOperand operand;
  mlir::Type type;
  if (parser.parseOptionalAttrDict(result.attributes))
    return mlir::failure();
  auto optOperand = parser.parseOptionalOperand(operand);
  if (optOperand.has_value()) {
    if (*optOperand || parser.parseColonType(type) ||
        parser.resolveOperand(operand, type, result.operands))
      return mlir::failure();
  }
  return mlir::success();
}

void ReturnOp::print(mlir::OpAsmPrinter &p) {
  if (getNumOperands() > 0)
    p << " " << getOperand(0) << " : " << getOperand(0).getType();
}

llvm::LogicalResult ReturnOp::verify() {
  // Walk up through any enclosing block statements to find the subprogram.
  mlir::Operation *parent = (*this)->getParentOp();
  while (parent && mlir::isa<BlockStmtOp>(parent))
    parent = parent->getParentOp();
  auto subp = mlir::dyn_cast<SubpOp>(parent);
  if (!subp)
    return emitOpError() << "expects parent to be ada.subp";
  mlir::FunctionType funcType = subp.getFunctionType();

  /// ReturnOps can only have a single optional operand.
  if (getNumOperands() > 1)
    return emitOpError() << "expects at most 1 return operand";

  // The operand number and types must match the function signature.
  const auto &results = funcType.getResults();
  if (getNumOperands() != results.size())
    return emitOpError() << "does not return the same number of values ("
                         << getNumOperands() << ") as the enclosing function ("
                         << results.size() << ")";

  // If the operation does not have an input, we are done.
  if (!hasOperand())
    return mlir::success();

  auto inputType = *operand_type_begin();
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
