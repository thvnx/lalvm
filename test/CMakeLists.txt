find_program(FILECHECK FileCheck HINTS ${LLVM_TOOLS_BINARY_DIR} REQUIRED)
find_program(NOT_TOOL not HINTS ${LLVM_TOOLS_BINARY_DIR} REQUIRED)
find_program(LLC llc HINTS ${LLVM_TOOLS_BINARY_DIR} REQUIRED)
find_program(LLVM_DWARFDUMP llvm-dwarfdump HINTS ${LLVM_TOOLS_BINARY_DIR} REQUIRED)

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  MAIN_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py
)

add_lit_testsuite(check-lalvm "Running lalvm tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS lalvm
)

# Merge the profiles a coverage-instrumented `check-lalvm` run writes (see
# lit.cfg.py) and render the llvm-cov reports into <build>/coverage/.
if(LALVM_COVERAGE)
  find_program(LLVM_PROFDATA llvm-profdata HINTS ${LLVM_TOOLS_BINARY_DIR} REQUIRED)
  find_program(LLVM_COV llvm-cov HINTS ${LLVM_TOOLS_BINARY_DIR} REQUIRED)
  add_custom_target(coverage-report
    COMMAND ${CMAKE_SOURCE_DIR}/tools/coverage
            --llvm-profdata ${LLVM_PROFDATA} --llvm-cov ${LLVM_COV}
            report ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}
    COMMENT "Rendering llvm-cov coverage report"
    USES_TERMINAL
    VERBATIM)
endif()
