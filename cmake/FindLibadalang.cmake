# Find Libadalang's C API and expose it as the imported target
# Libadalang::libadalang. LIBADALANG_DIR points at a build tree or an install
# prefix; only the shared library is searched for in a build tree.

set(LIBADALANG_DIR "" CACHE PATH "Libadalang build tree or install prefix")

find_path(Libadalang_INCLUDE_DIR
  NAMES libadalang.h
  HINTS ${LIBADALANG_DIR}
  PATH_SUFFIXES src include/libadalang include
)

find_library(Libadalang_LIBRARY
  NAMES adalang
  HINTS ${LIBADALANG_DIR}
  PATH_SUFFIXES lib/relocatable/prod lib/relocatable/dev lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libadalang
  REQUIRED_VARS Libadalang_LIBRARY Libadalang_INCLUDE_DIR
  REASON_FAILURE_MESSAGE
    "set LIBADALANG_DIR to a Libadalang build tree or install prefix"
)

if(Libadalang_FOUND AND NOT TARGET Libadalang::libadalang)
  add_library(Libadalang::libadalang UNKNOWN IMPORTED)
  # Include directories of imported targets are treated as system ones.
  set_target_properties(Libadalang::libadalang PROPERTIES
    IMPORTED_LOCATION "${Libadalang_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Libadalang_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(Libadalang_INCLUDE_DIR Libadalang_LIBRARY)
