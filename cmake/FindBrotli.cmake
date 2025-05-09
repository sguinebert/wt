# FindBrotli.cmake
cmake_minimum_required(VERSION 3.10)

# Handle static library preference
if(BROTLI_USE_STATIC_LIBS)
  set(_brotli_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
  if(WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  else()
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  endif()
endif()

# Find the include directory
find_path(Brotli_INCLUDE_DIR
  NAMES brotli/decode.h
  HINTS ${BROTLI_ROOT_DIR}/include
)
message(STATUS "Brotli_INCLUDE_DIR: ${Brotli_INCLUDE_DIR}")

# Check for pkg-config
find_package(PkgConfig QUIET)

# Define Brotli components
set(_brotli_components common dec enc)

# Use pkg-config if available
if(PKG_CONFIG_FOUND)
  foreach(_component IN LISTS _brotli_components)
    string(TOUPPER ${_component} _COMPONENT)
    pkg_check_modules(PC_BROTLI_${_COMPONENT} QUIET libbrotli${_component})
    message(STATUS "pkg-config for libbrotli${_component}: ${PC_BROTLI_${_COMPONENT}_FOUND}")
    if(PC_BROTLI_${_COMPONENT}_FOUND)
      add_library(Brotli::${_component} UNKNOWN IMPORTED)
      set_target_properties(Brotli::${_component} PROPERTIES
        IMPORTED_LOCATION "${PC_BROTLI_${_COMPONENT}_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${PC_BROTLI_${_COMPONENT}_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${PC_BROTLI_${_COMPONENT}_LINK_LIBRARIES}"
      )
      set(Brotli_${_component}_FOUND TRUE)
    endif()
  endforeach()
endif()

# Manual search for components not found via pkg-config
foreach(_component IN LISTS _brotli_components)
  if(NOT Brotli_${_component}_FOUND)
    find_library(Brotli_${_component}_LIBRARY
      NAMES brotli${_component}
      HINTS ${BROTLI_ROOT_DIR}/lib
    )
    message(STATUS "Brotli_${_component}_LIBRARY: ${Brotli_${_component}_LIBRARY}")
    if(Brotli_${_component}_LIBRARY)
      add_library(Brotli::${_component} UNKNOWN IMPORTED)
      set_target_properties(Brotli::${_component} PROPERTIES
        IMPORTED_LOCATION "${Brotli_${_component}_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Brotli_INCLUDE_DIR}"
      )
      if(_component STREQUAL "dec" OR _component STREQUAL "enc")
        set_target_properties(Brotli::${_component} PROPERTIES
          INTERFACE_LINK_LIBRARIES Brotli::common
        )
      endif()
      set(Brotli_${_component}_FOUND TRUE)
    else()
      set(Brotli_${_component}_FOUND FALSE)
    endif()
  endif()
endforeach()

# Standard CMake find_package handling
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Brotli
  REQUIRED_VARS Brotli_INCLUDE_DIR
  HANDLE_COMPONENTS
)

# Set output variables if found
if(Brotli_FOUND)
  set(Brotli_INCLUDE_DIRS ${Brotli_INCLUDE_DIR})
  message(STATUS "Brotli found: ${Brotli_INCLUDE_DIRS}")
else()
  message(STATUS "Brotli not found")
endif()

# Restore original library suffixes
if(BROTLI_USE_STATIC_LIBS)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_brotli_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()
