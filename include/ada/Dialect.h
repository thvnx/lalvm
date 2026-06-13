#ifndef ADA_DIALECT_H
#define ADA_DIALECT_H

#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Interfaces/CallInterfaces.h"
#include "mlir/Interfaces/ControlFlowInterfaces.h"
#include "mlir/Interfaces/FunctionInterfaces.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/MemorySlotInterfaces.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

/// Include the auto-generated header file containing the declaration of the Ada
/// dialect.
#include "ada/Dialect.h.inc"

/// Include the auto-generated enum declarations (AdaBinaryOp, AdaBinaryOpAttr).
#include "ada/AdaOpsEnums.h.inc"

/// Include the auto-generated attribute declarations (EnumTypeInfoAttr, ...).
#define GET_ATTRDEF_CLASSES
#include "ada/Attrs.h.inc"

/// Include the auto-generated type declarations (QualType, ...).
#define GET_TYPEDEF_CLASSES
#include "ada/Types.h.inc"

/// Include the auto-generated header file containing the declarations of the
/// Ada operations.
#define GET_OP_CLASSES
#include "ada/Ops.h.inc"

namespace mlir {
namespace ada {
/// Return the bare Ada name from a qualified dialect symbol: the segment after
/// the last dot, with a trailing __N collision suffix removed. Used for
/// human-facing names (DWARF DW_AT_name) where the simple source name is wanted
/// rather than the qualified, collision-disambiguated symbol.
llvm::StringRef bareName(llvm::StringRef qualified);

/// Resolve the nested symbol `name` visible from `from`, honoring Ada scoping:
/// walk the enclosing scopes and, at each `ada.subp`, search its `ada.decls`
/// SymbolTable children (which `lookupNearestSymbolFrom` cannot reach, since
/// `ada.subp` is not a SymbolTable and `ada.decls` is a sibling of statements).
/// Returns the symbol op, or null.
mlir::Operation *lookupSymbolFrom(mlir::Operation *from, llvm::StringRef name);
} // namespace ada
} // namespace mlir

#endif // ADA_DIALECT_H
