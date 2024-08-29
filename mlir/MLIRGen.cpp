#include "MLIRGen.h"

#include "mlir/IR/Block.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LogicalResult.h"
//#include "toy/AST.h"
#include "Dialect.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Verifier.h"
//#include "toy/Lexer.h"

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
    dump(&moduleAST, 0);
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

    std::cout << loc.line << ":" << loc.column << std::endl;
    std::cout << filename << std::endl;

    return mlir::FileLineColLoc::get(builder.getStringAttr(filename),
                                     loc.line,
                                     loc.column);
  }

  /// Declare a variable in the current scope, return success if the variable
  /// wasn't declared yet.
  llvm::LogicalResult declare(llvm::StringRef var, mlir::Value value) {
    if (symbolTable.count(var))
      return mlir::failure();
    symbolTable.insert(var, value);
    return mlir::success();
  }

   void visit(ada_node &moduleAST) {
    switch (ada_node_kind (&moduleAST)) {
        case ada_subp_spec: {
          builder.setInsertionPointToEnd(theModule.getBody());
          mlir::ada::FuncOp function = mlirGenSubpSpec(moduleAST);
          mlir::Block &entryBlock = function.front();
          builder.setInsertionPointToStart(&entryBlock);
          builder.create<mlir::ada::ReturnOp>(loc(moduleAST));
          return;}
        default:
          break;
      }

    unsigned i, count = ada_node_children_count(&moduleAST);
    for (i = 0; i < count; ++i)
      {
        ada_node child;
        //TODO check return value
        ada_node_child(&moduleAST, i, &child);
        visit(child);
      }
  }

  /// Create the prototype for an MLIR function with as many arguments as the
  /// provided Libadalang AST prototype.
  // TODO: convert libadalang ast to C++ classes so that we can use overloading for mlirGen instead of ada_node for all nodes.
  mlir::ada::FuncOp mlirGenSubpSpec(ada_node &subp_spec) {
    auto location = loc(subp_spec);

    ada_node name;
    ada_symbol_type symbol;
    ada_text text;
    ada_subp_spec_f_subp_name (&subp_spec, &name);
    ada_name_p_canonical_text (&name, &symbol);
    ada_symbol_text (&symbol, &text);
    char *subp_name;
    ada_text_to_utf8(&text, &subp_name, &text.length);

    std::cout << subp_name << std::endl;

    // This is a generic function, the return type will be inferred later.
    // Arguments type are uniformly unranked tensors.
    //llvm::SmallVector<mlir::Type, 4> argTypes(proto.getArgs().size(),
    //                                          getType(VarType{}));
    auto funcType = builder.getFunctionType(/*argTypes*/std::nullopt, std::nullopt);
    return builder.create<mlir::ada::FuncOp>(location,
                                             subp_name,
                                             funcType);
  }

};

} // namespace

namespace ada {

// The public API for codegen.
mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &moduleAST) {
  return MLIRGenImpl(context).mlirGen(moduleAST);
}

int fn (int a) {
  return 10 * a;
}

} // namespace toy
