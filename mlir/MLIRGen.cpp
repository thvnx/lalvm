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

#include "mlir/Dialect/Arith/IR/Arith.h"

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

  /// Emit a binary operation
  mlir::Value mlirGenBinOp(ada_node &binop) {
    // First emit the operations for each side of the operation before emitting
    // the operation itself. For example if the expression is `a + foo(a)`
    // 1) First it will visiting the LHS, which will return a reference to the
    //    value holding `a`. This value should have been emitted at declaration
    //    time and registered in the symbol table, so nothing would be
    //    codegen'd. If the value is not in the symbol table, an error has been
    //    emitted and nullptr is returned.
    // 2) Then the RHS is visited (recursively) and a call to `foo` is emitted
    //    and the result value is returned. If an error occurs we get a nullptr
    //    and propagate.
    //
    ada_node left;
    ada_bin_op_f_left(&binop, &left);
    mlir::Value lhs = visit_expr(left);
    if (!lhs)
      return nullptr;
    ada_node right;
    ada_bin_op_f_right(&binop, &right);
    mlir::Value rhs = visit_expr(right);
    if (!rhs)
      return nullptr;
    auto location = loc(binop);

    // Derive the operation name from the binary operator. At the moment we only
    // support '+' and '*'.
    ada_node op;
    ada_bin_op_f_op(&binop, &op);
    switch (ada_node_kind (&op)) {
    case ada_op_plus:
      return builder.create<mlir::ada::AddOp>(location, lhs, rhs);
    case ada_op_minus:
      return builder.create<mlir::ada::SubOp>(location, lhs, rhs);
    case ada_op_mult:
      return builder.create<mlir::ada::MulOp>(location, lhs, rhs);
    default:
      std::cerr << "Error while visiting unsupported binop: ";
      libadalang::dump(&op);
      std::cerr << "\n";
    }

    emitError(location, "invalid binary operator: ");
      libadalang::dump(&binop);
    return nullptr;
  }

  mlir::Value mlirGenIntLiteral(ada_node &node) {
    ada_big_integer bigint;
    if (!ada_int_literal_p_denoted_value(&node, &bigint)) {
      emitError(loc(node), "failed to evaluate integer literal");
      return nullptr;
    }
    ada_text text;
    ada_big_integer_text(bigint, &text);
    char *str;
    size_t length;
    ada_text_to_utf8(&text, &str, &length);
    str[length] = '\0';
    int64_t value = std::stoll(str);
    ada_big_integer_decref(bigint);
    return builder.create<mlir::arith::ConstantIntOp>(loc(node), value, 32);
  }

  mlir::Value visit_expr(ada_node &expr) {
    switch (ada_node_kind (&expr)) {
    case ada_identifier:
      return mlirGenVariable(expr);
    case ada_int_literal:
      return mlirGenIntLiteral(expr);
    case ada_bin_op:
      return mlirGenBinOp(expr);
    default:
      std::cerr << "Error while visiting unsupported expression: ";
      libadalang::dump(&expr);
      std::cerr << "\n";
    }

    return nullptr;
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
        // Set location of function parameters
        std::get<1>(nameValue).setLoc(loc(p));

    }

    builder.setInsertionPointToStart(&entryBlock);
    //          builder.create<mlir::ada::ReturnOp>(loc(moduleAST));

    ada_node stmts;
    ada_subp_body_f_stmts (&subp_body, &stmts);
    visit(stmts);

    return function;
  }

  /// Create the prototype for an MLIR function with as many arguments as the
  /// provided Libadalang AST prototype.
  // TODO: convert libadalang ast to C++ classes so that we can use overloading for mlirGen instead of ada_node for all nodes.
  mlir::ada::FuncOp mlirGenSubpSpec(ada_node &subp_spec) {
    auto location = loc(subp_spec);

    ada_node name;
    ada_subp_spec_f_subp_name (&subp_spec, &name);

    ada_node_array params;
    ada_node ids;
    ada_base_subp_spec_p_params(&subp_spec, &params);

    llvm::SmallVector<mlir::Type, 4> argTypes;
    for (int i = 0; i < params->n; i++) {
      ada_node type_expr;
      ada_param_spec_f_type_expr(&params->items[i], &type_expr);
      mlir::Type paramType = getMLIRType(type_expr);
      ada_param_spec_f_ids(&params->items[i], &ids);
      for (unsigned j = 0; j < ada_node_children_count(&ids); j++)
        argTypes.push_back(paramType);
    }

    ada_node ret_type_expr;
    ada_subp_spec_f_subp_returns(&subp_spec, &ret_type_expr);
    mlir::Type retType = ada_node_is_null(&ret_type_expr)
                             ? builder.getI32Type()
                             : getMLIRType(ret_type_expr);
    auto funcType = builder.getFunctionType(argTypes, {retType});
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


  /// Resolve an Ada type expression node to an MLIR type.
  mlir::Type getMLIRType(ada_node &type_expr) {
    ada_node type_decl;
    if (!ada_expr_p_expression_type(&type_expr, &type_decl) ||
        ada_node_is_null(&type_decl))
      return builder.getI32Type();

    ada_node canon_type;
    if (!ada_base_type_decl_p_canonical_type(&type_decl, nullptr, &canon_type) ||
        ada_node_is_null(&canon_type))
      canon_type = type_decl;

    ada_node type_name;
    if (!ada_base_type_decl_f_name(&canon_type, &type_name) ||
        ada_node_is_null(&type_name))
      return builder.getI32Type();

    llvm::StringRef name = libadalang::getName(&type_name);
    if (name == "integer")       return builder.getI32Type();
    if (name == "long_integer")  return builder.getI64Type();
    if (name == "short_integer") return builder.getIntegerType(16);
    if (name == "float")         return builder.getF32Type();
    if (name == "long_float")    return builder.getF64Type();

    mlir::emitWarning(loc(type_expr), "unsupported Ada type '")
        << name << "', defaulting to i32";
    return builder.getI32Type();
  }

  mlir::Type getType() { return builder.getI32Type(); }

};

} // namespace

namespace ada {

// The public API for codegen.
mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &moduleAST) {
  return MLIRGenImpl(context).mlirGen(moduleAST);
}

} // namespace toy
