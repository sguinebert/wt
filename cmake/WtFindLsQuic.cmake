# Find lsquic library (built from source with BoringSSL)
# Sets: LSQUIC_INCLUDE_DIR, LSQUIC_LIBRARY, LSQUIC_FOUND
# Also sets: BORINGSSL_INCLUDE_DIR, BORINGSSL_PREFIX_INCLUDE_DIR (for nexus compilation)

SET(LSQUIC_FOUND FALSE)

# lsquic built from source in 3rdparty (bundled with BoringSSL, symbols prefixed with BSSL_)
SET(_3RDPARTY_LSQUIC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/lsquic-src")
SET(_3RDPARTY_BORINGSSL  "${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/boringssl-src")

IF(EXISTS "${_3RDPARTY_LSQUIC_SRC}/build/liblsquic_bundle.a"
   AND EXISTS "${_3RDPARTY_LSQUIC_SRC}/include/lsquic.h")

  SET(LSQUIC_INCLUDE_DIR "${_3RDPARTY_LSQUIC_SRC}/include" CACHE PATH "lsquic include dir")
  # Bundle = lsquic + BoringSSL with all symbols BSSL_-prefixed (no conflict with system OpenSSL)
  SET(LSQUIC_LIBRARY "${_3RDPARTY_LSQUIC_SRC}/build/liblsquic_bundle.a" CACHE FILEPATH "lsquic library")

  IF(EXISTS "${_3RDPARTY_BORINGSSL}/include/openssl/ssl.h")
    SET(BORINGSSL_INCLUDE_DIR "${_3RDPARTY_BORINGSSL}/include" CACHE PATH "BoringSSL include dir (for nexus)")
  ENDIF()
  # Prefix headers directory (contains boringssl_prefix_symbols.h)
  IF(EXISTS "${_3RDPARTY_BORINGSSL}/build/symbol_prefix_include/boringssl_prefix_symbols.h")
    SET(BORINGSSL_PREFIX_INCLUDE_DIR "${_3RDPARTY_BORINGSSL}/build/symbol_prefix_include" CACHE PATH "BoringSSL prefix headers")
  ENDIF()

ELSE()
  # Fall back: check old bundled location
  SET(_3RDPARTY_LSQUIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/lsquic")
  IF(EXISTS "${_3RDPARTY_LSQUIC_DIR}/lsquic.h")
    SET(LSQUIC_INCLUDE_DIR "${_3RDPARTY_LSQUIC_DIR}" CACHE PATH "lsquic include dir")
    IF(EXISTS "${_3RDPARTY_LSQUIC_DIR}/fpic/liblsquic.a")
      SET(LSQUIC_LIBRARY "${_3RDPARTY_LSQUIC_DIR}/fpic/liblsquic.a" CACHE FILEPATH "lsquic library")
    ELSEIF(EXISTS "${_3RDPARTY_LSQUIC_DIR}/static/liblsquic.a")
      SET(LSQUIC_LIBRARY "${_3RDPARTY_LSQUIC_DIR}/static/liblsquic.a" CACHE FILEPATH "lsquic library")
    ENDIF()
  ELSE()
    FIND_PATH(LSQUIC_INCLUDE_DIR NAMES lsquic.h
      PATHS /usr/include /usr/local/include
    )
    FIND_LIBRARY(LSQUIC_LIBRARY NAMES lsquic
      PATHS /usr/lib /usr/lib64 /usr/local/lib /opt/local/lib
    )
  ENDIF()
ENDIF()

IF(LSQUIC_LIBRARY AND LSQUIC_INCLUDE_DIR)
  SET(LSQUIC_FOUND TRUE)
ENDIF()

message(STATUS "lsquic include dir: ${LSQUIC_INCLUDE_DIR} - lsquic library: ${LSQUIC_LIBRARY} : ${LSQUIC_FOUND}")
IF(BORINGSSL_INCLUDE_DIR)
  message(STATUS "BoringSSL headers for nexus: ${BORINGSSL_INCLUDE_DIR}")
ENDIF()
IF(BORINGSSL_PREFIX_INCLUDE_DIR)
  message(STATUS "BoringSSL prefix headers: ${BORINGSSL_PREFIX_INCLUDE_DIR}")
ENDIF()
