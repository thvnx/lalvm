#include "ada/MLIRGen.h"

#include "mlir/IR/Block.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LogicalResult.h"
//#include "toy/AST.h"
#include "ada/Dialect.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Verifier.h"
//#include "toy/Lexer.h"

#include "llvm/Support/Debug.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopedHashTable.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <numeric>
#include <optional>
#include <vector>


//using namespace mlir::ada;
//using namespace ada;

using llvm::ArrayRef;
using llvm::cast;
using llvm::dyn_cast;
using llvm::isa;
using llvm::ScopedHashTableScope;
using llvm::SmallVector;
using llvm::StringRef;
using llvm::Twine;

namespace {

/// Implementation of a simple MLIR emission from the Toy AST.
///
/// This will emit operations that are specific to the Toy language, preserving
/// the semantics of the language and (hopefully) allow to perform accurate
/// analysis and transformation based on these high level semantics.
class MLIRGenImpl {
public:
  MLIRGenImpl(mlir::MLIRContext &context) : builder(&context) {}

  /// Public API: convert the AST for a Toy module (source file) to an MLIR
  /// Module operation.
  mlir::ModuleOp mlirGen(ada_node &moduleAST) {
    // We create an empty MLIR module and codegen functions one at a time and
    // add them to the module.
    theModule = mlir::ModuleOp::create(builder.getUnknownLoc());

    //for (FunctionAST &f : moduleAST)
    //  mlirGen(f);
    //TODO: convert ada_node to C++ to use overloading instead of this visit function
    //dump(&moduleAST, 0);
    visit(moduleAST);

    // Verify the module after we have finished constructing it, this will check
    // the structural properties of the IR and invoke any specific verifiers we
    // have on the Toy operations.
    if (failed(mlir::verify(theModule))) {
      theModule.emitError("module verification error");
      return nullptr;
    }

    return theModule;
  }

private:
  /// A "module" matches a Toy source file: containing a list of functions.
  mlir::ModuleOp theModule;

  /// The builder is a helper class to create IR inside a function. The builder
  /// is stateful, in particular it keeps an "insertion point": this is where
  /// the next operations will be introduced.
  mlir::OpBuilder builder;

  /// The symbol table maps a variable name to a value in the current scope.
  /// Entering a function creates a new scope, and the function arguments are
  /// added to the mapping. When the processing of a function is terminated, the
  /// scope is destroyed and the mappings created in this scope are dropped.
  llvm::ScopedHashTable<StringRef, mlir::Value> symbolTable;

  /// Helper conversion for a Libadalang AST location to an MLIR location.
  mlir::Location loc (ada_node &node) {
    ada_source_location_range loc_range;
    ada_node_sloc_range (&node, &loc_range);

    ada_source_location loc = loc_range.start;
    char *filename = ada_unit_filename(ada_node_unit(&node));

    //TODO find a way on how to enable -debug command line option support:
    // requires a debug build of LLVM
#define DEBUG_TYPE MLIRGEN_DEBUG
    LLVM_DEBUG(llvm::dbgs() << loc.line << ":" << loc.column << " (" << filename << ")");
#undef  DEBUG_TYPE

    return mlir::FileLineColLoc::get(builder.getStringAttr(filename),
                                     loc.line,
                                     loc.column);
  }

  /// Declare a variable in the current scope, return success if the variable
  /// wasn't declared yet.
  llvm::LogicalResult declare(llvm::StringRef var, mlir::Value value) {
#define DEBUG_TYPE MLIRGEN_DEBUG
    LLVM_DEBUG(llvm::dbgs() << "declare variable: " << var.data());
#undef  DEBUG_TYPE
    //std::cout << "declare variable: " << var.data() << std::endl;
    if (symbolTable.count(var))
      return mlir::failure();
    symbolTable.insert(var, value);
    return mlir::success();
  }

  void visit(ada_node &moduleAST) {
    switch (ada_node_kind (&moduleAST)) {
    case ada_subp_body:
      mlirGenSubpBody(moduleAST);
      return;
    case ada_return_stmt: {
      if(mlir::succeeded(mlirGenReturn(moduleAST)))
        break;
      else
        return;
    }
    default:
      break;
    }

    unsigned i, count = ada_node_children_count(&moduleAST);
    for (i = 0; i < count; ++i)
      {
        ada_node child;
        if (ada_node_child(&moduleAST, i, &child) == 0)
          std::cerr << "Error while getting a child (MLIRGen::visit)";;
        visit(child);
      }
  }

  /// This is a reference to a variable in an expression. The variable is
  /// expected to have been declared and so should have a value in the symbol
  /// table, otherwise emit an error and return nullptr.
  mlir::Value mlirGenVariable(ada_node &expr) {
    if (auto variable = symbolTable.lookup(libadalang::getName(&expr).data()))
      return variable;

    emitError(loc(expr), "error: unknown variable '")
        << libadalang::getName(&expr).data() << "'";
    return nullptr;
  }

  mlir::Value visit_expr(ada_node &expr) {
    return mlirGenVariable(expr);//mlir::Value();
  }

  /// Emit a new function and add it to the MLIR module.
  mlir::ada::FuncOp mlirGenSubpBody(ada_node &subp_body) {
    // Create a scope in the symbol table to hold variable declarations.
    ScopedHashTableScope<llvm::StringRef, mlir::Value> varScope(symbolTable);

    ada_node ada_subp_spec;
    ada_base_subp_body_f_subp_spec (&subp_body, &ada_subp_spec);

    builder.setInsertionPointToEnd(theModule.getBody());
    mlir::ada::FuncOp function = mlirGenSubpSpec(ada_subp_spec);
    mlir::Block &entryBlock = function.front();

    std::vector<ada_node> args_v;

    // auto protoArgs = funcAST.getProto()->getArgs();
    // ada_node params, params_l;
    // ada_subp_spec_f_subp_params (&ada_subp_spec, &params);
    // ada_params_f_params (&params, &params_l);

    // unsigned i, count = ada_node_children_count(&params_l);
    // for (i = 0; i < count; ++i)
    //   {
    //     ada_node child;
    //     //TODO check return value
    //     ada_node_child(&params, i, &child);
    //     args_v.push_back(&child);
    //   }

    ada_node_array params;
    ada_node ids;
    ada_base_subp_spec_p_params (&ada_subp_spec, &params);

    for (int i = 0; i < params->n; i++) {
      ada_param_spec_f_ids(&params->items[i], &ids);
      for (unsigned int j = 0; j < ada_node_children_count(&ids); j++) {
        ada_node child;
        ada_node_child(&ids, j, &child);
        args_v.push_back(child);
      }
    }

    // Declare all the function arguments in the symbol table.
    for (const auto nameValue :
           llvm::zip(args_v, entryBlock.getArguments())) {
      ada_node p = std::get<0>(nameValue);

      //ada_node names;
      //ada_node_child(p, 0, p);
      //ada_param_spec_f_ids(p, &names);
      //ada_node name;
      //ada_node_child(&names, 0, &name);

        if (failed(declare(libadalang::getName(/*&name*/&p).data(),
                           std::get<1>(nameValue))))

          return nullptr;

    }

    builder.setInsertionPointToStart(&entryBlock);
    //          builder.create<mlir::ada::ReturnOp>(loc(moduleAST));

    ada_node stmts;
    ada_subp_body_f_stmts (&subp_body, &stmts);
    visit(stmts);

    mlir::ada::ReturnOp returnOp;
    if (!entryBlock.empty())
      returnOp = dyn_cast<mlir::ada::ReturnOp>(entryBlock.back());
    if (!returnOp) {
      //builder.create<mlir::ada::ReturnOp>(loc(subp_body));
    } else if (returnOp.hasOperand()) {
      // Otherwise, if this return operation has an operand then add a result to
      // the function.
      function.setType(builder.getFunctionType(
                                               function.getFunctionType().getInputs(), getType()));
    }
    return function;
  }

  /// Create the prototype for an MLIR function with as many arguments as the
  /// provided Libadalang AST prototype.
  // TODO: convert libadalang ast to C++ classes so that we can use overloading for mlirGen instead of ada_node for all nodes.
  mlir::ada::FuncOp mlirGenSubpSpec(ada_node &subp_spec) {
    auto location = loc(subp_spec);

    ada_node name;
    ada_subp_spec_f_subp_name (&subp_spec, &name);

    size_t n = 0;
    ada_node_array params;
    ada_node ids;
    ada_base_subp_spec_p_params (&subp_spec, &params);

    for (int i = 0; i < params->n; i++) {
      ada_param_spec_f_ids(&params->items[i], &ids);
      n+=ada_node_children_count(&ids);
    }

    // This is a generic function, the return type will be inferred later (not in ada).
    // Arguments type are uniformly unranked tensors.
    llvm::SmallVector<mlir::Type, 4> argTypes(n, getType(/*VarType{}*/));
    llvm::SmallVector<mlir::Type, 1> retTypes(1, getType(/*VarType{}*/));
    auto funcType = builder.getFunctionType(argTypes, retTypes);
    return builder.create<mlir::ada::FuncOp>(location,
                                             libadalang::getName(&name).data(),
                                             funcType);
  }

  /// Emit a return operation. This will return failure if any generation fails.
  llvm::LogicalResult mlirGenReturn(ada_node &return_stmt) {
    auto location = loc(return_stmt);

    mlir::Value expr = nullptr;
    //if (ret.getExpr().has_value()) {//TODO in ada return op always has a value, keep a check thout?
     // if (!(expr = mlirGen(**ret.getExpr())))
     //   return mlir::failure();
    //}
    ada_node return_expr;
    ada_return_stmt_f_return_expr(&return_stmt, &return_expr);
    expr = visit_expr(return_expr);

    // Otherwise, this return operation has zero operands.
    builder.create<mlir::ada::ReturnOp>(location,
                             expr ? ArrayRef(expr) : ArrayRef<mlir::Value>());
    return mlir::success();
  }


  /// Build a tensor type from a list of shape dimensions.
  mlir::Type getType(ArrayRef<int64_t> shape) {
    // If the shape is empty, then this type is unranked.
    if (shape.empty())
      return mlir::UnrankedTensorType::get(builder.getF64Type());

    // Otherwise, we use the given shape.
    //return mlir::RankedTensorType::get(shape, builder.getF64Type());
    return builder.getIntegerType(32);
  }

  /// Build an MLIR type from a Toy AST variable type (forward to the generic
  /// getType above).
  mlir::Type getType(/*const VarType &type*/) { return getType(/*type.shape*/1); }

};

} // namespace

namespace ada {

// The public API for codegen.
mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &moduleAST) {
  return MLIRGenImpl(context).mlirGen(moduleAST);
}

} // namespace toy
