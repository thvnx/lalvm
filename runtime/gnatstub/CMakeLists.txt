# Temporary substitutes for the GNAT runtime entry points, to link the programs
# lalvm binds with until the Ada runtime provides them.

# Drop sanitizers and coverage instrumentation since this is target code.
set(CMAKE_C_FLAGS "")
set_directory_properties(PROPERTIES COMPILE_OPTIONS "")

add_library(gnatstub STATIC rcheck.c)
target_compile_options(gnatstub PRIVATE -Wall -Wextra)
set_target_properties(gnatstub PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY ${LLVM_LIBRARY_OUTPUT_INTDIR})
