// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/ADT/DenseMap.h"

using namespace mlir;

namespace {

struct Formal {
  Type type;
  ada::QualType base;
  ada::AdaParamMode mode;
  Location loc;
  std::optional<unsigned> input;
};

struct Signature {
  ada::SubpOp source;
  SmallVector<Formal> formals;
  SmallVector<Type> inputs;
  SmallVector<Type> results;
  SmallVector<Attribute> inputAttrs;
  SmallVector<Attribute> resultAttrs;
};

//===----------------------------------------------------------------------===//
// Signature mapping
//===----------------------------------------------------------------------===//

static LogicalResult mapSignature(Signature &signature) {
  auto op = signature.source;
  auto type = op.getFunctionType();
  if (!isa<ModuleOp>(op->getParentOp()))
    return op.emitOpError("callee lowering requires a module level subprogram");
  if (type.getNumResults() > 1)
    return op.emitOpError("expected at most one Ada function result");

  OpBuilder builder(op);
  signature.results.append(type.getResults().begin(), type.getResults().end());
  if (type.getNumResults())
    signature.resultAttrs.push_back(op.getResultAttrDict(0)
                                        ? op.getResultAttrDict(0)
                                        : builder.getDictionaryAttr({}));

  for (unsigned i = 0, end = op.getNumSourceFormals(); i < end; ++i) {
    Type input = type.getInput(i);
    auto mode = op.getFormalMode(i);
    bool writable = mode != ada::AdaParamMode::In;
    if (auto ref = dyn_cast<ada::RefType>(input)) {
      if (!writable)
        return op.emitOpError("in formals require a value type");
      input = ref.getValueType();
    } else if (writable) {
      return op.emitOpError("writable formals require ada.ref type");
    }
    ada::QualType base;
    if (writable) {
      auto qual = dyn_cast<ada::QualType>(input);
      if (!qual || !ada::isNumericMachineType(qual.getMlirType()))
        return op.emitOpError("unsupported formal type ") << input;
      base = ada::baseOfAdaType(op, qual);
      if (!base)
        return op.emitOpError("unresolved formal type ") << qual.getAdaType();
    }
    Location loc = op.isExternal()
                       ? op.getLoc()
                       : op.getBody().front().getArgument(i).getLoc();
    Formal formal{input, base, mode, loc, std::nullopt};
    if (op.formalNeedsInput(i)) {
      formal.input = signature.inputs.size();
      signature.inputs.push_back(mode == ada::AdaParamMode::Out ? formal.base
                                                                : input);
      NamedAttrList attrs(op.getArgAttrDict(i));
      attrs.erase("ada.mode");
      signature.inputAttrs.push_back(attrs.getDictionary(op.getContext()));
    }
    if (writable) {
      signature.results.push_back(formal.base);
      signature.resultAttrs.push_back(builder.getDictionaryAttr({}));
    }
    signature.formals.push_back(formal);
  }

  // Closure conversion appends captures after the source formals.
  for (unsigned i = signature.formals.size(); i < type.getNumInputs(); ++i) {
    if (!op.getArgAttrOfType<UnitAttr>(i, "ada.capture"))
      return op.emitOpError("capture arguments must follow source formals");
    signature.inputs.push_back(type.getInput(i));
    NamedAttrList attrs(op.getArgAttrDict(i));
    attrs.erase("ada.capture");
    signature.inputAttrs.push_back(attrs.getDictionary(op.getContext()));
  }
  return success();
}

static DictionaryAttr sourceProfile(OpBuilder &builder,
                                    const Signature &signature) {
  SmallVector<Attribute> formals;
  for (const Formal &formal : signature.formals) {
    NamedAttrList attrs;
    attrs.set("type", TypeAttr::get(formal.type));
    attrs.set("mode",
              ada::AdaParamModeAttr::get(builder.getContext(), formal.mode));
    if (formal.input)
      attrs.set("abi_input", builder.getI64IntegerAttr(*formal.input));
    formals.push_back(attrs.getDictionary(builder.getContext()));
  }
  NamedAttrList profile;
  profile.set("formals", builder.getArrayAttr(formals));
  auto sourceType = ada::SubpOp(signature.source).getFunctionType();
  if (sourceType.getNumResults())
    profile.set("result_type", TypeAttr::get(sourceType.getResult(0)));
  return profile.getDictionary(builder.getContext());
}

//===----------------------------------------------------------------------===//
// Callee lowering
//===----------------------------------------------------------------------===//

static void lowerCallee(Signature &signature) {
  auto source = signature.source;
  OpBuilder builder(source);
  Location loc =
      FusedLoc::get(source.getContext(), {source.getLoc()},
                    ada::DIProfileAttr::get(source.getContext(),
                                            sourceProfile(builder, signature)));
  auto function = func::FuncOp::create(
      builder, loc, source.getSymName(),
      builder.getFunctionType(signature.inputs, signature.results));
  for (NamedAttribute attr : source->getAttrs()) {
    StringRef key = attr.getName().getValue();
    if (key != "sym_name" && key != "function_type" && key != "arg_attrs" &&
        key != "res_attrs")
      function->setAttr(attr.getName(), attr.getValue());
  }
  function.setAllArgAttrs(builder.getArrayAttr(signature.inputAttrs));
  function.setAllResultAttrs(builder.getArrayAttr(signature.resultAttrs));
  if (source.isExternal()) {
    function.setPrivate();
    source.erase();
    return;
  }
  if (source.isPrivate())
    function->setAttr(
        "llvm.linkage",
        LLVM::LinkageAttr::get(source.getContext(), LLVM::Linkage::Internal));

  function.getBody().takeBody(source.getBody());
  Block &entry = function.getBody().front();
  SmallVector<BlockArgument> oldArgs(entry.getArguments());
  SmallVector<BlockArgument> inputs;
  for (const Formal &formal : signature.formals) {
    if (!formal.input)
      continue;
    Location inputLoc = ada::unnamedLoc(formal.loc);
    if (formal.mode == ada::AdaParamMode::In) {
      inputLoc = formal.loc;
      if (auto qual = dyn_cast<ada::QualType>(formal.type))
        inputLoc = ada::attachAdaTypeRef(inputLoc, qual.getAdaType());
    }
    inputs.push_back(
        entry.addArgument(signature.inputs[*formal.input], inputLoc));
  }
  unsigned firstCapture = inputs.size();
  for (BlockArgument capture :
       ArrayRef<BlockArgument>(oldArgs).drop_front(signature.formals.size()))
    inputs.push_back(entry.addArgument(capture.getType(), capture.getLoc()));

  builder.setInsertionPointToStart(&entry);
  SmallVector<Value> objects;
  for (auto [i, formal] : llvm::enumerate(signature.formals)) {
    if (formal.mode == ada::AdaParamMode::In) {
      oldArgs[i].replaceAllUsesWith(inputs[*formal.input]);
      objects.push_back({});
      continue;
    }
    Value initial = formal.input ? Value(inputs[*formal.input]) : Value{};
    auto nameLoc = dyn_cast<NameLoc>(formal.loc);
    Location objectLoc = FusedLoc::get(
        source.getContext(), {nameLoc ? nameLoc.getChildLoc() : formal.loc},
        ada::DIFormalAttr::get(source.getContext(), i));
    if (nameLoc)
      objectLoc = NameLoc::get(nameLoc.getName(), objectLoc);
    auto object = ada::ObjectOp::create(
        builder, objectLoc, ada::RefType::get(source.getContext(), formal.type),
        initial);
    objects.push_back(object);
    oldArgs[i].replaceAllUsesWith(object);
  }
  for (unsigned i = signature.formals.size(); i < oldArgs.size(); ++i)
    oldArgs[i].replaceAllUsesWith(inputs[firstCapture++]);
  entry.eraseArguments(0, oldArgs.size());

  SmallVector<ada::ReturnOp> returns;
  function.walk([&](ada::ReturnOp op) {
    if (op->getParentOp() == function)
      returns.push_back(op);
  });
  for (auto returned : returns) {
    builder.setInsertionPoint(returned);
    SmallVector<Value> values(returned.getOperands());
    for (auto [i, formal] : llvm::enumerate(signature.formals))
      if (formal.mode != ada::AdaParamMode::In)
        values.push_back(ada::ReadOp::create(builder,
                                             ada::unnamedLoc(returned.getLoc()),
                                             formal.base, objects[i]));
    func::ReturnOp::create(builder, returned.getLoc(), values);
    returned.erase();
  }
  source.erase();
}

//===----------------------------------------------------------------------===//
// Caller lowering
//===----------------------------------------------------------------------===//

static void lowerCaller(ada::CallOp source, const Signature &signature) {
  OpBuilder builder(source);
  SmallVector<Value> inputs;
  unsigned initial = 0;
  for (auto [i, formal] : llvm::enumerate(signature.formals)) {
    if (!formal.input)
      continue;
    inputs.push_back(formal.mode == ada::AdaParamMode::In
                         ? source.getInputs()[i]
                         : source.getInitialValues()[initial++]);
  }
  for (Value capture : source.getInputs().drop_front(signature.formals.size()))
    inputs.push_back(capture);
  auto call = func::CallOp::create(builder, source.getLoc(), source.getCallee(),
                                   signature.results, inputs);
  if (auto attrs = source.getArgAttrsAttr()) {
    SmallVector<Attribute> mapped;
    for (auto [i, formal] : llvm::enumerate(signature.formals))
      if (formal.input)
        mapped.push_back(attrs[i]);
    llvm::append_range(mapped,
                       attrs.getValue().drop_front(signature.formals.size()));
    call.setArgAttrsAttr(builder.getArrayAttr(mapped));
  }
  if (auto attrs = source.getResAttrsAttr()) {
    SmallVector<Attribute> mapped(attrs.getValue());
    mapped.resize(signature.results.size(), builder.getDictionaryAttr({}));
    call.setResAttrsAttr(builder.getArrayAttr(mapped));
  }
  // Call results follow formal order. Stores follow association order, with
  // each conversion immediately before its write (@rm{6-4-1}).
  SmallVector<Value> outputs(signature.formals.size());
  unsigned result = source.getNumResults();
  for (auto [i, formal] : llvm::enumerate(signature.formals))
    if (formal.mode != ada::AdaParamMode::In)
      outputs[i] = call.getResult(result++);
  SmallVector<Value> ranges(signature.formals.size());
  if (auto indices =
          source->getAttrOfType<DenseI32ArrayAttr>("ada.range_formals"))
    for (auto [i, index] : llvm::enumerate(indices.asArrayRef()))
      ranges[index] = source.getCopybackRanges()[i];
  SmallVector<int32_t> order;
  if (auto indices =
          source->getAttrOfType<DenseI32ArrayAttr>("ada.actual_order"))
    llvm::append_range(order, indices.asArrayRef());
  else
    for (unsigned i = 0; i < signature.formals.size(); ++i)
      order.push_back(i);
  for (int32_t i : order) {
    if (!outputs[i])
      continue;
    Value reference = source.getInputs()[i];
    auto target = cast<ada::RefType>(reference.getType()).getValueType();
    Location loc = ada::unnamedLoc(source.getLoc());
    Value value = ada::ConvertOp::create(builder, loc, target,
                                         ada::AdaConversionKind::Implicit,
                                         outputs[i], ranges[i]);
    ada::WriteOp::create(builder, loc, value, reference);
  }
  if (source.getResult())
    source.getResult().replaceAllUsesWith(call.getResult(0));
  source.erase();
}

//===----------------------------------------------------------------------===//
// LowerAdaCallsPass
//===----------------------------------------------------------------------===//

struct LowerAdaCallsPass
    : PassWrapper<LowerAdaCallsPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerAdaCallsPass)

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<func::FuncDialect, LLVM::LLVMDialect>();
  }
  StringRef getName() const final { return "LowerAdaCalls"; }
  StringRef getArgument() const final { return "ada-lower-calls"; }
  StringRef getDescription() const final {
    return "Lower Ada subprograms and calls to the value ABI";
  }
  void runOnOperation() final {
    ModuleOp module = getOperation();
    SmallVector<Signature, 0> signatures;
    llvm::DenseMap<Operation *, unsigned> calleeIndices;
    bool invalid = false;
    module.walk([&](ada::SubpOp op) {
      Signature signature;
      signature.source = op;
      if (failed(mapSignature(signature))) {
        invalid = true;
        return;
      }
      calleeIndices[op] = signatures.size();
      signatures.push_back(std::move(signature));
    });
    if (invalid)
      return signalPassFailure();
    SmallVector<ada::CallOp> calls;
    module.walk([&](ada::CallOp call) { calls.push_back(call); });
    SymbolTableCollection symbols;
    for (auto call : calls) {
      auto found =
          calleeIndices.find(ada::CallOp::lookupCallee(call, call.getCallee()));
      if (found == calleeIndices.end()) {
        call.emitOpError("requires an Ada callee declaration");
        return signalPassFailure();
      }
      if (failed(call.verify()) || failed(call.verifySymbolUses(symbols)))
        return signalPassFailure();
      lowerCaller(call, signatures[found->second]);
    }
    for (auto &signature : signatures)
      lowerCallee(signature);
    ConversionTarget target(getContext());
    target.markUnknownOpDynamicallyLegal([](Operation *) { return true; });
    target.addIllegalOp<ada::SubpOp, ada::CallOp, ada::ReturnOp>();
    if (failed(applyPartialConversion(module, target,
                                      RewritePatternSet(&getContext()))))
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::ada::createLowerAdaCallsPass() {
  return std::make_unique<LowerAdaCallsPass>();
}
