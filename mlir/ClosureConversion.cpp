//===- ClosureConversion.cpp - Lambda-lift nested Ada subprograms ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the ClosureConversionPass. Because `ada.subp` is not
// IsolatedFromAbove, a nested subprogram may reference variables from an
// enclosing scope (an up-level reference) as ordinary cross-region SSA uses.
// This pass lambda-lifts each such capture into an explicit parameter, rewrites
// call sites to pass the captured value, and hoists the now self-contained
// subprogram to module level (LLVM has no nested functions).
//
// Lambda lifting (capture-as-parameter) rather than a static link (one hidden
// pointer to the enclosing frame, GNAT's approach) works because every call
// here is direct, so the pass can rewrite each known call site to forward the
// captures. A static link would be required only for indirect calls through an
// access-to-subprogram value, which is not yet supported.
//
// It must run before `mem2reg`: a captured local has to stay an alloca (passed
// by `memref`) so the parameter can carry up-level writes back to the caller.
// Once lifted, a captured local's address escapes through the call, so
// `mem2reg` leaves it in memory on its own.
//
//===----------------------------------------------------------------------===//

#include "ada/Dialect.h"
#include "ada/Passes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/TypeID.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;

namespace {
struct ClosureConversionPass
    : public PassWrapper<ClosureConversionPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ClosureConversionPass)
  void runOnOperation() final;
};
} // namespace

void ClosureConversionPass::runOnOperation() {
  ModuleOp module = getOperation();
  MLIRContext *ctx = &getContext();
  IRRewriter rewriter(ctx);

  // Collect nested subprograms innermost-first: a post-order walk visits a
  // nested subprogram before the one enclosing it, so lifting an outer
  // subprogram sees the captures its (already lifted) callees forward. A nested
  // subprogram's parent is its scope's `ada.decls`; library-level ones (parent
  // the module) have nothing above to capture. Collected before any mutation so
  // the hoisting below does not disturb the walk.
  llvm::SmallVector<ada::SubpOp> nested;
  module.walk<WalkOrder::PostOrder>([&](ada::SubpOp subp) {
    if (isa<ada::DeclsOp>(subp->getParentOp()))
      nested.push_back(subp);
  });

  // Subprograms already lifted and hoisted this run. A call to a capturing
  // subprogram from one of these can't be given the captures (see below).
  llvm::SmallPtrSet<Operation *, 8> processed;

  for (ada::SubpOp subp : nested) {
    // Lift: append a block argument per value used from above, rewrite in-body
    // uses to it, and return the captured values in argument order.
    llvm::SmallVector<Value> captures =
        makeRegionIsolatedFromAbove(rewriter, subp.getBody());

    if (!captures.empty()) {
      // Extend the function type with the captured values' types. This assumes
      // the subprogram carries no arg_attrs: MLIRGen sets none (parameter names
      // ride on the block arguments' NameLoc), so there is nothing to keep in
      // sync. If ada.subp ever gains arg_attrs, extend them with one entry per
      // capture here, or the FunctionOpInterface verifier will reject the
      // arg-count/attr-count mismatch.
      FunctionType fnType = subp.getFunctionType();
      llvm::SmallVector<Type> inputs(fnType.getInputs());
      for (Value capture : captures)
        inputs.push_back(capture.getType());
      subp.setFunctionType(FunctionType::get(ctx, inputs, fnType.getResults()));

      // The new block arguments (the lifted parameters) are the last
      // `captures.size()` arguments of the entry block.
      auto args = subp.getBody().getArguments();
      ValueRange liftedArgs = args.drop_front(args.size() - captures.size());

      // Pass the captures at every call to this subprogram (matched by its
      // dialect FQN; mangling happens later).
      StringRef name = subp.getSymName();
      bool unsupported = false;
      module.walk([&](ada::CallOp call) {
        if (call.getCallee() != name)
          return WalkResult::advance();
        // A self-recursive call inside the body forwards the lifted parameters.
        if (subp->isProperAncestor(call)) {
          call->insertOperands(call.getNumOperands(), liftedArgs);
          return WalkResult::advance();
        }
        // An external call forwards the captured values, which are in scope at
        // the call, unless the call sits in an already-lifted (hoisted) nested
        // subprogram, which cannot forward this subprogram's captures.
        // That is an upward or mutually recursive call among capturing nested
        // subprograms, which this single innermost-first pass does not handle
        // (a fixpoint over the call graph would). Diagnose it rather than emit
        // IR that later fails verification.
        if (ada::SubpOp caller = call->getParentOfType<ada::SubpOp>();
            caller && processed.contains(caller.getOperation())) {
          mlir::emitError(call.getLoc(),
                          "closure conversion does not support a capturing "
                          "nested subprogram called from a mutually recursive "
                          "or enclosing nested subprogram");
          unsupported = true;
          return WalkResult::interrupt();
        }
        call->insertOperands(call.getNumOperands(), ValueRange(captures));
        return WalkResult::advance();
      });
      if (unsupported)
        return signalPassFailure();
    }

    // Self-contained now: hoist to module level.
    subp->moveBefore(module.getBody(), module.getBody()->end());
    processed.insert(subp.getOperation());
  }

  // Erase any `ada.decls` left empty (it held only subprograms). Containers
  // still holding `ada.type`s are left for the mangling pass, which hoists the
  // types and erases them. Erasing the visited op here is safe: the walk
  // early-increments its operation iterator (post-order, and ada.decls don't
  // nest), so erasing the current op can't invalidate the traversal.
  module.walk([&](ada::DeclsOp decls) {
    if (decls.getBody().front().empty())
      decls.erase();
  });
}

std::unique_ptr<mlir::Pass> mlir::ada::createClosureConversionPass() {
  return std::make_unique<ClosureConversionPass>();
}
