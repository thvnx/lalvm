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
