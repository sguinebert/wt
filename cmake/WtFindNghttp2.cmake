# Find nghttp2 with Windows support
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(NGHTTP2 IMPORTED_TARGET libnghttp2)
endif()

if(NOT NGHTTP2_FOUND)
  # Windows-specific paths
  if(WIN32)
    list(APPEND NGHTTP2_SEARCH_PATHS
      "$ENV{PROGRAMFILES}/nghttp2"
      "$ENV{PROGRAMFILES\(X86\)}/nghttp2"
      "C:/nghttp2"
      "$ENV{VCPKG_ROOT}/installed/x64-windows"
      "$ENV{VCPKG_ROOT}/installed/x86-windows"
    )
  endif()

  # Unix-style paths
  list(APPEND NGHTTP2_SEARCH_PATHS
    /usr/local
    /usr
    /opt/local
    /opt
  )

  find_path(NGHTTP2_INCLUDE_DIR
    NAMES nghttp2/nghttp2.h
    PATH_SUFFIXES include
    PATHS ${NGHTTP2_SEARCH_PATHS}
  )

  find_library(NGHTTP2_LIBRARY
    NAMES nghttp2 libnghttp2
    PATH_SUFFIXES lib lib64 lib/x64 lib/x86
    PATHS ${NGHTTP2_SEARCH_PATHS}
  )

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(NGHTTP2
    DEFAULT_MSG
    NGHTTP2_LIBRARY NGHTTP2_INCLUDE_DIR
  )

  if(NGHTTP2_FOUND)
    set(NGHTTP2_LIBRARIES ${NGHTTP2_LIBRARY})
    set(NGHTTP2_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIR})
  endif()
endif()
