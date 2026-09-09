# Find Libadalang's C API and expose it as the imported target
# Libadalang::libadalang. LIBADALANG_DIR points at a build tree or an install
# prefix. Only the shared library is searched for in a build tree.

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

# The C API carries no version. An Alire crate records it in `alire.toml` at the
# crate root (`version = "26.0.0"`). A generated tree may record it in
# `libadalang.ads` next to the header (`Version : constant String := "..."`),
# though a development build says "undefined" there. Libadalang_VERSION stays
# empty when neither gives one.
set(Libadalang_VERSION "")
if(Libadalang_INCLUDE_DIR)
  get_filename_component(_libadalang_root "${Libadalang_INCLUDE_DIR}" DIRECTORY)
  set(_libadalang_version "")
  if(EXISTS "${_libadalang_root}/alire.toml")
    file(STRINGS "${_libadalang_root}/alire.toml" _libadalang_version
      REGEX "^version *= *\"")
  elseif(EXISTS "${Libadalang_INCLUDE_DIR}/libadalang.ads")
    file(STRINGS "${Libadalang_INCLUDE_DIR}/libadalang.ads" _libadalang_version
      REGEX "^ *Version *: *constant String := \"[0-9]")
  endif()
  if(_libadalang_version MATCHES "\"([^\"]*)\"")
    set(Libadalang_VERSION "${CMAKE_MATCH_1}")
  endif()
  unset(_libadalang_root)
  unset(_libadalang_version)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libadalang
  REQUIRED_VARS Libadalang_LIBRARY Libadalang_INCLUDE_DIR
  VERSION_VAR Libadalang_VERSION
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
