#ifndef ADA_DIALECT_H
#define ADA_DIALECT_H

#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Interfaces/FunctionInterfaces.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

/// Include the auto-generated header file containing the declaration of the Ada
/// dialect.
#include "ada/Dialect.h.inc"

/// Include the auto-generated enum declarations (AdaBinaryOp, AdaBinaryOpAttr).
#include "ada/AdaOpsEnums.h.inc"

/// Include the auto-generated header file containing the declarations of the
/// Ada operations.
#define GET_OP_CLASSES
#include "ada/Ops.h.inc"

#endif // ADA_DIALECT_H
