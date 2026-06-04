cmake_minimum_required(VERSION 3.20.0)
project(lalvm LANGUAGES CXX C)

set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to conform to")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  find_package(MLIR REQUIRED CONFIG)

  message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
  set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/bin)
  set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)
  set(MLIR_BINARY_DIR ${CMAKE_BINARY_DIR})

  list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

  include(TableGen)
  include(AddLLVM)
  include(AddMLIR)
  include(HandleLLVMOptions)
else()
  # Build via external projects mechanism
  set(MLIR_MAIN_SRC_DIR ${LLVM_MAIN_SRC_DIR}/../mlir)
  set(MLIR_INCLUDE_DIR ${MLIR_MAIN_SRC_DIR}/include)
  set(MLIR_GENERATED_INCLUDE_DIR ${LLVM_BINARY_DIR}/tools/mlir/include)
  set(MLIR_INCLUDE_DIRS "${MLIR_INCLUDE_DIR};${MLIR_GENERATED_INCLUDE_DIR}")
endif()

if(MLIR_ENABLE_BINDINGS_PYTHON)
  include(MLIRDetectPythonEnv)
  mlir_configure_python_dev_packages()
endif()

set(STANDALONE_SOURCE_DIR ${PROJECT_SOURCE_DIR})
set(STANDALONE_BINARY_DIR ${PROJECT_BINARY_DIR})
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${MLIR_INCLUDE_DIRS})
include_directories(${STANDALONE_SOURCE_DIR}/include)
include_directories(${STANDALONE_BINARY_DIR}/include)
link_directories(${LLVM_BUILD_LIBRARY_DIR})
add_definitions(${LLVM_DEFINITIONS})

add_subdirectory(include)
add_subdirectory(test)
set(LIBADALANG_INCLUDE_DIR "" CACHE PATH "Directory containing libadalang.h (typically libadalang/build/src)")
if(NOT LIBADALANG_INCLUDE_DIR)
  message(FATAL_ERROR "LIBADALANG_INCLUDE_DIR must be set, e.g. -DLIBADALANG_INCLUDE_DIR=/path/to/libadalang/build/src")
endif()
include_directories(${LIBADALANG_INCLUDE_DIR})
# TODO: replace global include_directories with target_include_directories(lalvm ...)
# so that source-relative paths are scoped to the lalvm target only.
include_directories(mlir frontend)


add_llvm_executable(lalvm
  frontend/DiagnosticPrinter.cpp
  mlir/MLIRGen.cpp
  mlir/Dialect.cpp
  mlir/ClosureConversion.cpp
  mlir/HoistNestedSymbolOperations.cpp
  mlir/LowerToLLVM.cpp
  mlir/AdaToLLVMIRTranslation.cpp
  mlir/AdaDebugInfo.cpp
  mlir/EnumDITypes.cpp
  frontend/AST.cpp
  lalvm.cpp

  DEPENDS
  AdaOpsIncGen
  )

# TODO: replace catch-all dialect/conversion/extension lib sets with an explicit
# minimal list to reduce binary size and link time.
get_property(dialect_libs GLOBAL PROPERTY MLIR_DIALECT_LIBS)
get_property(conversion_libs GLOBAL PROPERTY MLIR_CONVERSION_LIBS)
get_property(extension_libs GLOBAL PROPERTY MLIR_EXTENSION_LIBS)
target_link_libraries(lalvm
  PRIVATE
      ${dialect_libs}
    ${conversion_libs}
    ${extension_libs}
  adalang
  MLIRAnalysis
  MLIRIR
  MLIRParser
  MLIRSupport
  MLIRLLVMToLLVMIRTranslation
  MLIRExecutionEngine
  MLIRLLVMCommonConversion
  MLIRSCFDialect
)

# Format all C++ sources in-place using clang-format.
file(GLOB_RECURSE ALL_CXX_SOURCES
  ${CMAKE_SOURCE_DIR}/*.cpp
  ${CMAKE_SOURCE_DIR}/*.h
)
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT} -i --style=LLVM ${ALL_CXX_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format"
  )
else()
  message(STATUS "clang-format not found, 'format' target unavailable")
endif()

find_package(Doxygen OPTIONAL_COMPONENTS dot)
if(DOXYGEN_FOUND)
  if(DOXYGEN_DOT_EXECUTABLE)
    set(DOXYGEN_HAVE_DOT YES)
  else()
    set(DOXYGEN_HAVE_DOT NO)
    message(STATUS "Graphviz dot not found, Doxygen graphs disabled")
  endif()
  configure_file(
    ${CMAKE_SOURCE_DIR}/docs/Doxyfile.in
    ${CMAKE_BINARY_DIR}/docs/Doxyfile
    @ONLY
  )
  add_custom_target(doxygen
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/docs/Doxyfile
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating HTML documentation with Doxygen"
    VERBATIM
  )
  add_dependencies(doxygen AdaOpsIncGen)
else()
  message(STATUS "Doxygen not found, 'doxygen' target unavailable")
endif()
