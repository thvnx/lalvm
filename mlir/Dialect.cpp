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
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/Value.h"
#include "mlir/Interfaces/FunctionImplementation.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include <string>

using namespace mlir;
using namespace mlir::ada;

#include "ada/AdaOpsEnums.cpp.inc"
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
// ProcOp
//===----------------------------------------------------------------------===//

void ProcOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   llvm::StringRef name, mlir::FunctionType type,
                   llvm::ArrayRef<mlir::NamedAttribute> attrs) {
  // buildWithEntryBlock is provided by FunctionOpInterface. It sets all the
  // required attributes and creates the entry block with the right arg types.
  buildWithEntryBlock(builder, state, name, type, attrs, type.getInputs());
}

mlir::ParseResult ProcOp::parse(mlir::OpAsmParser &parser,
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

void ProcOp::print(mlir::OpAsmPrinter &p) {
  mlir::function_interface_impl::printFunctionOp(
      p, *this, /*isVariadic=*/false, getFunctionTypeAttrName(),
      getArgAttrsAttrName(), getResAttrsAttrName());
}

//===----------------------------------------------------------------------===//
// FuncOp
//===----------------------------------------------------------------------===//

void FuncOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                   llvm::StringRef name, mlir::FunctionType type,
                   llvm::ArrayRef<mlir::NamedAttribute> attrs) {
  // FunctionOpInterface provides a convenient `build` method that will populate
  // the state of our FuncOp, and create an entry block.
  buildWithEntryBlock(builder, state, name, type, attrs, type.getInputs());
}

mlir::ParseResult FuncOp::parse(mlir::OpAsmParser &parser,
                                mlir::OperationState &result) {
  // Dispatch to the FunctionOpInterface provided utility method that parses the
  // function operation.
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

void FuncOp::print(mlir::OpAsmPrinter &p) {
  // Dispatch to the FunctionOpInterface provided utility method that prints the
  // function operation.
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

// ada.return is used by both ada.func (with one operand) and ada.proc (with
// none). The parser therefore treats the operand as optional.
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
  // Walk up through any enclosing block statements to find the function/proc.
  mlir::Operation *parent = (*this)->getParentOp();
  while (parent && mlir::isa<BlockStmtOp>(parent))
    parent = parent->getParentOp();
  mlir::FunctionType funcType;
  if (auto func = mlir::dyn_cast<FuncOp>(parent))
    funcType = func.getFunctionType();
  else if (auto proc = mlir::dyn_cast<ProcOp>(parent))
    funcType = proc.getFunctionType();
  else
    return emitOpError() << "expects parent to be ada.func or ada.proc";

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
