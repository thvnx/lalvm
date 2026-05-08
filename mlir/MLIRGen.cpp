#include "ada/MLIRGen.h"

#include "mlir/IR/Block.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LogicalResult.h"
#include "ada/Dialect.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Verifier.h"

#include "mlir/Dialect/Arith/IR/Arith.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopedHashTable.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/StringSaver.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <numeric>
#include <optional>
#include <cerrno>
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

/// Walks a Libadalang AST and emits Ada dialect MLIR operations into a module.
/// The public entry point is mlirGen() at the bottom of this file.
class MLIRGenImpl {
public:
  MLIRGenImpl(mlir::MLIRContext &context) : builder(&context) {}

  /// Public API: convert the AST for an Ada source file to an MLIR Module.
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
    // have on the Ada operations.
    if (failed(mlir::verify(theModule))) {
      theModule.emitError("module verification error");
      return nullptr;
    }

    return theModule;
  }

private:
  /// A "module" matches an Ada source file: containing a list of subprograms.
  mlir::ModuleOp theModule;

  /// The builder is a helper class to create IR inside a function. The builder
  /// is stateful, in particular it keeps an "insertion point": this is where
  /// the next operations will be introduced.
  mlir::OpBuilder builder;

  // Maps variable names to their current SSA Value within the active scope.
  // ScopedHashTable automatically pops bindings when a scope is destroyed, which
  // handles Ada's block scoping. In SSA form, assignment rebinds the name to a
  // new Value rather than mutating in place.
  llvm::ScopedHashTable<StringRef, mlir::Value> symbolTable;

  // Arena for string keys stored in symbolTable. StringRef is non-owning, so
  // names must outlive their symbol-table entry; stringSaver copies each name
  // into stringPool which lives as long as MLIRGenImpl.
  llvm::BumpPtrAllocator stringPool;
  llvm::StringSaver stringSaver{stringPool};

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

    // getStringAttr copies the string into the MLIR context, so filename can
    // be freed immediately.
    auto result = mlir::FileLineColLoc::get(builder.getStringAttr(filename),
                                            loc.line,
                                            loc.column);
    free(filename);
    return result;
  }

  /// Declare a variable in the current scope, return success if the variable
  /// wasn't declared yet.
  llvm::LogicalResult declare(llvm::StringRef var, mlir::Value value) {
#define DEBUG_TYPE MLIRGEN_DEBUG
    LLVM_DEBUG(llvm::dbgs() << "declare variable: " << var.data());
#undef  DEBUG_TYPE
    if (symbolTable.count(var))
      return mlir::failure();
    symbolTable.insert(stringSaver.save(var), value);
    return mlir::success();
  }

  // Recursive AST walker. Handles the node kinds we know how to codegen;
  // everything else is ignored at this level and its children are visited.
  // Returning early (without visiting children) stops descent into a subtree —
  // used when a handler already walked it (e.g. mlirGenSubpBody visits stmts).
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
    case ada_assign_stmt:
      if (mlir::failed(mlirGenAssign(moduleAST)))
        return;
      break;
    default:
      break;
    }

    unsigned i, count = ada_node_children_count(&moduleAST);
    for (i = 0; i < count; ++i)
      {
        ada_node child;
        if (ada_node_child(&moduleAST, i, &child) == 0)
          llvm::errs() << "Error while getting a child (MLIRGen::visit)\n";
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
      llvm::errs() << "Error while visiting unsupported binop: ";
      libadalang::dump(&op);
      llvm::errs() << "\n";
    }

    emitError(location, "invalid binary operator: ");
      libadalang::dump(&binop);
    return nullptr;
  }

  mlir::Value mlirGenIntLiteral(ada_node &node) {
    // p_denoted_value gives us the evaluated integer value as a big integer.
    // We convert it through its UTF-8 text representation since there is no
    // direct C API to extract a 64-bit integer from ada_big_integer.
    ada_big_integer bigint;
    if (!ada_int_literal_p_denoted_value(&node, &bigint)) {
      emitError(loc(node), "failed to evaluate integer literal");
      return nullptr;
    }
    ada_text text;
    ada_big_integer_text(bigint, &text);
    char *buf;
    size_t length;
    ada_text_to_utf8(&text, &buf, &length);
    std::string literal(buf, length);
    free(buf);
    ada_big_integer_decref(bigint);
    errno = 0;
    char *endptr;
    int64_t value = static_cast<int64_t>(std::strtoll(literal.c_str(), &endptr, 10));
    if (errno == ERANGE) {
      emitError(loc(node), "integer literal ") << literal.c_str() << " out of range for i64";
      return nullptr;
    }

    // p_expression_type on an integer literal returns universal_integer
    // (Libadalang's "universal_int_type_"), not the concrete type.
    // When that happens, p_expected_expression_type gives the type required
    // by the surrounding context (e.g. the return type of the enclosing function).
    ada_node type_decl;
    if (!ada_expr_p_expression_type(&node, &type_decl) || ada_node_is_null(&type_decl)) {
      emitError(loc(node), "failed to resolve type of integer literal");
      return nullptr;
    }
    ada_node type_name;
    if (ada_base_type_decl_f_name(&type_decl, &type_name) &&
        !ada_node_is_null(&type_name) &&
        libadalang::getName(&type_name) == "universal_int_type_") {
      if (!ada_expr_p_expected_expression_type(&node, &type_decl) ||
          ada_node_is_null(&type_decl)) {
        emitError(loc(node), "failed to resolve expected type of integer literal");
        return nullptr;
      }
    }
    mlir::Type type = getMLIRTypeFromDecl(type_decl, loc(node));
    if (!type)
      return nullptr;

    unsigned width = mlir::cast<mlir::IntegerType>(type).getWidth();
    // Reject literals that don't fit in the target type.
    // Values outside i64 range are already caught above via std::stoll.
    if (width < 64) {
      int64_t maxVal = (1LL << (width - 1)) - 1;
      int64_t minVal = -(1LL << (width - 1));
      if (value < minVal || value > maxVal) {
        emitError(loc(node), "integer literal ") << value
            << " out of range for i" << width;
        return nullptr;
      }
    }
    return builder.create<mlir::arith::ConstantIntOp>(loc(node), value, width);
  }

  mlir::Value mlirGenRealLiteral(ada_node &node) {
    // Unlike ada_int_literal, ada_real_literal has no p_denoted_value in the
    // C API, so we extract the value by reading the literal's source text.
    ada_text text;
    ada_node_text(&node, &text);
    char *buf;
    size_t length;
    ada_text_to_utf8(&text, &buf, &length);
    // Ada allows underscores as digit separators; strip them while copying.
    std::string literal;
    literal.reserve(length);
    for (size_t i = 0; i < length; ++i)
      if (buf[i] != '_') literal += buf[i];
    free(buf);

    errno = 0;
    char *endptr;
    double value = std::strtod(literal.c_str(), &endptr);
    if (errno == ERANGE || std::isinf(value)) {
      emitError(loc(node), "real literal ") << literal.c_str() << " out of range for f64";
      return nullptr;
    }

    // Real literals have universal_real type; fall back to the expected type
    // to get the concrete type required by the surrounding context.
    ada_node type_decl;
    if (!ada_expr_p_expression_type(&node, &type_decl) || ada_node_is_null(&type_decl)) {
      emitError(loc(node), "failed to resolve type of real literal");
      return nullptr;
    }
    ada_node type_name;
    if (ada_base_type_decl_f_name(&type_decl, &type_name) &&
        !ada_node_is_null(&type_name) &&
        libadalang::getName(&type_name) == "universal_real_type_") {
      if (!ada_expr_p_expected_expression_type(&node, &type_decl) ||
          ada_node_is_null(&type_decl)) {
        emitError(loc(node), "failed to resolve expected type of real literal");
        return nullptr;
      }
    }
    mlir::Type type = getMLIRTypeFromDecl(type_decl, loc(node));
    if (!type)
      return nullptr;

    // Reject values that overflow f32 (a double-precision parse is always
    // needed first; then we check if the value fits in the narrower type).
    auto floatType = mlir::cast<mlir::FloatType>(type);
    if (floatType.getWidth() == 32 && std::isinf(static_cast<float>(value))) {
      emitError(loc(node), "real literal ") << literal.c_str() << " out of range for f32";
      return nullptr;
    }

    return builder.create<mlir::arith::ConstantOp>(loc(node),
        builder.getFloatAttr(type, value));
  }

  mlir::Value visit_expr(ada_node &expr) {
    switch (ada_node_kind (&expr)) {
    case ada_identifier:
      return mlirGenVariable(expr);
    case ada_int_literal:
      return mlirGenIntLiteral(expr);
    case ada_real_literal:
      return mlirGenRealLiteral(expr);
    case ada_bin_op:
      return mlirGenBinOp(expr);
    default:
      llvm::errs() << "Error while visiting unsupported expression: ";
      libadalang::dump(&expr);
      llvm::errs() << "\n";
    }

    return nullptr;
  }

  /// Lower one Ada subprogram body to an ada.func or ada.proc operation.
  /// This is the main codegen entry point for a subprogram: it creates the
  /// function op, binds argument SSA values in the symbol table, then walks
  /// the statement list to emit the body.
  mlir::Operation *mlirGenSubpBody(ada_node &subp_body) {
    // Push a new scope so that argument names and local variables are cleaned
    // up automatically when we leave this subprogram.
    ScopedHashTableScope<llvm::StringRef, mlir::Value> varScope(symbolTable);

    ada_node ada_subp_spec;
    ada_base_subp_body_f_subp_spec(&subp_body, &ada_subp_spec);

    ada_node ret_type_expr;
    ada_subp_spec_f_subp_returns(&ada_subp_spec, &ret_type_expr);
    bool isProc = ada_node_is_null(&ret_type_expr);

    builder.setInsertionPointToEnd(theModule.getBody());
    mlir::Operation *op;
    mlir::Block *entryBlock;
    if (isProc) {
      mlir::ada::ProcOp proc = mlirGenProcSpec(ada_subp_spec);
      if (!proc) return nullptr;
      op = proc;
      entryBlock = &proc.front();
    } else {
      mlir::ada::FuncOp function = mlirGenSubpSpec(ada_subp_spec);
      if (!function) return nullptr;
      op = function;
      entryBlock = &function.front();
    }

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
           llvm::zip(args_v, entryBlock->getArguments())) {
      ada_node p = std::get<0>(nameValue);
      if (failed(declare(libadalang::getName(&p).data(),
                         std::get<1>(nameValue))))
        return nullptr;
      std::get<1>(nameValue).setLoc(loc(p));
    }

    builder.setInsertionPointToStart(entryBlock);

    ada_node stmts;
    ada_subp_body_f_stmts(&subp_body, &stmts);
    visit(stmts);

    // Procedures have no explicit return statement; add an implicit one.
    if (isProc)
      builder.create<mlir::ada::ReturnOp>(loc(subp_body),
                                          ArrayRef<mlir::Value>{});

    return op;
  }

  /// Create an ada.proc with the signature derived from the Ada subprogram spec.
  mlir::ada::ProcOp mlirGenProcSpec(ada_node &subp_spec) {
    auto location = loc(subp_spec);

    ada_node name;
    ada_subp_spec_f_subp_name(&subp_spec, &name);

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

    auto funcType = builder.getFunctionType(argTypes, {});
    return builder.create<mlir::ada::ProcOp>(location,
                                             libadalang::getName(&name).data(),
                                             funcType);
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

  /// Emit an assignment statement. In SSA form this rebinds the name to the
  /// new value; out-parameter write-back semantics are not yet implemented.
  llvm::LogicalResult mlirGenAssign(ada_node &assign_stmt) {
    ada_node dest_node, expr_node;
    ada_assign_stmt_f_dest(&assign_stmt, &dest_node);
    ada_assign_stmt_f_expr(&assign_stmt, &expr_node);

    mlir::Value rhs = visit_expr(expr_node);
    if (!rhs)
      return mlir::failure();

    if (ada_node_kind(&dest_node) != ada_identifier) {
      emitError(loc(assign_stmt), "unsupported assignment destination");
      return mlir::failure();
    }

    auto name = libadalang::getName(&dest_node);
    if (!symbolTable.count(name.data())) {
      emitError(loc(dest_node), "unknown variable '")
          << name.data() << "'";
      return mlir::failure();
    }

    symbolTable.insert(stringSaver.save(name), rhs);
    return mlir::success();
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


  /// Map a type declaration node to an MLIR type. Follows the subtype chain
  /// to the canonical base type, then matches its name against known Ada types.
  /// diagLoc is used only for the "unsupported type" warning.
  mlir::Type getMLIRTypeFromDecl(ada_node &type_decl, mlir::Location diagLoc) {
    // Follow the subtype chain to the canonical (base) type so that subtypes
    // of Integer map to the same MLIR type as Integer itself.
    // TODO: nullptr would be the correct origin but crashes with a
    // CONSTRAINT_ERROR in libadalang-implementation-c.adb; using self for now.
    ada_node canon_type;
    if (!ada_base_type_decl_p_canonical_type(&type_decl, &type_decl, &canon_type) ||
        ada_node_is_null(&canon_type))
      canon_type = type_decl;

    // f_name gives the defining identifier of the type declaration, whose
    // lower-cased text we use to drive the mapping below.
    ada_node type_name;
    if (!ada_base_type_decl_f_name(&canon_type, &type_name) ||
        ada_node_is_null(&type_name)) {
      mlir::emitError(diagLoc, "failed to get name of type declaration");
      return {};
    }

    std::string name = libadalang::getName(&type_name);
    if (name == "integer")       return builder.getI32Type();
    if (name == "long_integer")  return builder.getI64Type();
    if (name == "short_integer") return builder.getIntegerType(16);
    if (name == "float")         return builder.getF32Type();
    if (name == "long_float")    return builder.getF64Type();

    mlir::emitError(diagLoc, "unsupported Ada type '") << name << "'";
    return {};
  }

  /// Resolve an Ada type expression (e.g. a SubtypeIndication node like
  /// "Long_Integer") to the corresponding MLIR type. Returns a null type
  /// and emits an error on failure.
  mlir::Type getMLIRType(ada_node &type_expr) {
    // p_designated_type_decl resolves a type expression to its declaration.
    // This works on SubtypeIndication nodes (parameter / return types), unlike
    // p_expression_type which only works on value expressions.
    ada_node type_decl;
    if (!ada_type_expr_p_designated_type_decl(&type_expr, &type_decl) ||
        ada_node_is_null(&type_decl)) {
      mlir::emitError(loc(type_expr), "failed to resolve type expression");
      return {};
    }

    return getMLIRTypeFromDecl(type_decl, loc(type_expr));
  }

};

} // namespace

namespace ada {

// The public API for codegen.
mlir::OwningOpRef<mlir::ModuleOp> mlirGen(mlir::MLIRContext &context,
                                          ada_node &moduleAST) {
  return MLIRGenImpl(context).mlirGen(moduleAST);
}

} // namespace ada
