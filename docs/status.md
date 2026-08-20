# Feature status

LALVM is a work in progress, so this file may lag its actual status. Only a
subset of Ada is supported so far: most language features are not yet
implemented.

## Expressions

- Integer and real literals; named numbers
- Binary arithmetic: `+`, `-`, `*`, `/`
- Relational comparisons: `=`, `/=`
- If expressions and parenthesized expressions
- Variable and parameter references
- Function calls (including nested)
- Enum and character literals
- `'First`/`'Last` attributes on scalar (sub)types

## Checks

- Overflow check on signed-integer `+`/`-`/`*` and a division check on integer
  `/`, raising `Constraint_Error` (modular arithmetic wraps, no check).
- `Constraint_Error` range checks where a value flows into a constrained
  subtype; statically-resolved checks are decided at compile time (out-of-range
  static expressions are diagnosed, in-range ones emit no check).

## Statements

- Assignments, `return`, `null`
- Procedure calls
- `if` statements
- Loop statements: `while` and bare `loop`, with `exit` (named or plain)
- `goto` statements and labels
- Block statements (`begin`/`end` and `declare`/`begin`/`end`)

## Declarations

- Subprograms: functions and procedures, library-level and nested, including
  up-level references (a nested subprogram reading or writing an enclosing
  subprogram's variables).
- Local variables with and without initializers (multiple names per
  declaration).
- Named numbers (`N : constant := 42`).
- Type declarations: integer, float, modular integer, enum (including
  `Boolean`).
- User-defined operator functions (definitions and calls mangled to GNAT
  O-names).

## Parameters

`in` (by value), `in out` and `out` (by reference).

## Types

- `Integer` (i32), `Short_Integer` (i16), `Long_Integer` (i64)
- `Float` (f32), `Long_Float` (f64)
- `Boolean` (i1) and user-defined enum types (i1 for 2 literals, i8 for 3-256)
- `Character` (i8) and user-defined character types
- Integer subtypes with static or dynamic range constraints (`subtype S is
  Integer range 1 .. N`); widths derived from the declared range

## Debug info (under `-g`)

DWARF 5, `DW_LANG_Ada2012`, source locations on all ops,
`dbg.declare`/`dbg.value` for variables and constants, `DICompositeType` for
enum types, `DISubrangeType` for constrained integer subtypes, `DW_TAG_label`
for `goto` labels.
