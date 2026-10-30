// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;

namespace {

struct Formal {
  ada::QualType type;
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
  if (!op->getAttrOfType<UnitAttr>("ada.new_lowering"))
    return op.emitOpError("ada.new_lowering must be a unit attribute");
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
    auto modeAttr = op.getArgAttrOfType<ada::AdaParamModeAttr>(i, "ada.mode");
    auto mode = modeAttr ? modeAttr.getValue() : ada::AdaParamMode::In;
    bool writable = mode != ada::AdaParamMode::In;
    if (auto ref = dyn_cast<ada::RefType>(input)) {
      if (!writable)
        return op.emitOpError("in formals require a value type");
      input = ref.getValueType();
    } else if (writable) {
      return op.emitOpError("writable formals require ada.ref type");
    }
    auto qual = dyn_cast<ada::QualType>(input);
    if (!qual || !ada::isNumericMachineType(qual.getMlirType()))
      return op.emitOpError("unsupported formal type ") << input;
    auto base = ada::baseOfAdaType(op, qual.getAdaType());
    if (!base)
      return op.emitOpError("unresolved formal type ") << qual.getAdaType();

    auto formalType = ada::lookupAdaType(op, qual.getAdaType());
    bool hasDefaultValue = formalType && formalType.hasDefaultValue();
    Location loc = op.isExternal()
                       ? op.getLoc()
                       : op.getBody().front().getArgument(i).getLoc();
    Formal formal{qual,
                  ada::QualType::get(op.getContext(), qual.getMlirType(), base),
                  mode, loc, std::nullopt};
    if (mode != ada::AdaParamMode::Out || hasDefaultValue) {
      formal.input = signature.inputs.size();
      signature.inputs.push_back(mode == ada::AdaParamMode::Out ? formal.base
                                                                : qual);
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
        key != "res_attrs" && key != "ada.new_lowering")
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
    Location inputLoc =
        formal.mode == ada::AdaParamMode::In
            ? ada::attachAdaTypeRef(formal.loc, formal.type.getAdaType())
            : ada::unnamedLoc(formal.loc);
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
    return "Lower Ada formal objects and results to the value ABI";
  }
  void runOnOperation() final {
    ModuleOp module = getOperation();
    SmallVector<ada::SubpOp> callees;
    module.walk([&](ada::SubpOp op) {
      if (op->hasAttr("ada.new_lowering"))
        callees.push_back(op);
    });

    // Caller lowering is introduced in the next implementation step.
    bool pendingCall = false;
    module.walk([&](ada::CallOp call) {
      Operation *callee = ada::CallOp::lookupCallee(call, call.getCallee());
      if (callee && callee->hasAttr("ada.new_lowering")) {
        call.emitOpError(
            "caller lowering for the value ABI is not implemented yet");
        pendingCall = true;
      }
    });
    if (pendingCall)
      return signalPassFailure();
    for (auto callee : callees) {
      Signature signature;
      signature.source = callee;
      if (failed(mapSignature(signature)))
        return signalPassFailure();
      lowerCallee(signature);
    }
  }
};

} // namespace

std::unique_ptr<Pass> mlir::ada::createLowerAdaCallsPass() {
  return std::make_unique<LowerAdaCallsPass>();
}
