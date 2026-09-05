# LALVM AGENTS.md

## Project context

LALVM compiles Ada source code to LLVM IR using MLIR as an intermediate
representation. Ada parsing and semantic analysis are provided by
[Libadalang](https://github.com/AdaCore/libadalang), accessed entirely through
its C API (`libadalang.h`). Libadalang resolves types, evaluates literals, and
provides source locations. MLIRGen does not re-implement any of that logic.

The Ada Reference Manual (ARM) is the authoritative spec for language semantics.
Online version: https://ada-rapporteur-group.github.io/ARM/Ada_202Y/AA-TOC.html.
ARM section numbers appear throughout the code (e.g. `RM 5.1` for null
statements, `RM 5.6` for block statements). In doxygen comments, write RM
references as `@rm{5-1}` (dashes, not dots).

## General conventions

- Never generate code unless I explicitly ask for it.
- If asked to commit, follow the LLVM conventions (see previous history).

## Response style

- Answer yes/no questions with just "Yes" or "No", nothing more.
- Keep answers to 10 lines or fewer unless I ask for more.
- In text, never use em dashes, en dashes or hyphens; use commas, parentheses,
  or a colon instead.

## English

English isn't my first language. I'd like to get better at it, so correct me as
we go: if I write something wrong or something a native speaker just wouldn't
say, tell me at the end of your answer and show me how you'd put it. Keep it
short, I'll ask for more if I'm interested.

## Memory and skills

- Do not load or store memories.
- Do not load or store skills.
