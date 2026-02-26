# ---------------------------------------------------------------------------
# whttp — Standalone HTTP/1.1+2+3 server library
# Can be used independently for REST/JSON APIs without Wt framework.
#   target_link_libraries(myapp PRIVATE whttp)   # standalone HTTP/2+3 server
# ---------------------------------------------------------------------------

set(WHTTP_SOURCES
    Wt/cuehttp/detail/h2_flush_impl.C
    Wt/cuehttp/uploaded_file.C
    Wt/cuehttp/deps/ada.cpp
)

if(WT_HAS_LSQUIC)
    list(APPEND WHTTP_SOURCES Wt/cuehttp/h3_server.C ${NEXUS_FILES})
endif()

add_library(whttp ${WHTTP_SOURCES})

target_compile_definitions(whttp PRIVATE whttp_EXPORTS)

set_target_properties(whttp PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    EXPORT_NAME WtHttp
    VERSION ${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}
    DEBUG_POSTFIX ${DEBUG_LIB_POSTFIX}
)

# Include paths: src/ resolves <Wt/cuehttp/*.hpp>, <Wt/AsioWrapper/*.hpp>, <Wt/fmt/*.h>
target_include_directories(whttp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Boost.Asio
if(WT_ASIO_IS_STANDALONE_ASIO)
    target_link_libraries(whttp PRIVATE ${BOOST_WT_LIBRARIES})
else()
    target_link_libraries(whttp PUBLIC ${BOOST_WT_LIBRARIES})
endif()

# glaze (session JSON serialization)
target_link_libraries(whttp PRIVATE glaze::glaze)

# ZLIB
if(ZLIB_FOUND)
    target_compile_definitions(whttp PUBLIC WTHTTP_WITH_ZLIB)
    target_link_libraries(whttp PUBLIC ZLIB::ZLIB)
endif()

# OpenSSL
if(WT_WITH_SSL)
    target_link_libraries(whttp PUBLIC ${OPENSSL_LIBRARIES})
    target_include_directories(whttp PRIVATE ${OPENSSL_INCLUDE_DIR})
endif()

# nghttp2 (HTTP/2)
if(HAVE_NGHTTP2)
    target_compile_definitions(whttp PUBLIC WT_WITH_HTTP2)
    set(_3RDPARTY_NGHTTP2 "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/nghttp2-src")
    if(EXISTS "${_3RDPARTY_NGHTTP2}/build/lib/libnghttp2.a")
        set(NGHTTP2_STATIC_LIB "${_3RDPARTY_NGHTTP2}/build/lib/libnghttp2.a")
        message(STATUS "whttp: using static nghttp2 (3rdparty PIC): ${NGHTTP2_STATIC_LIB}")
        target_include_directories(whttp PUBLIC
            $<BUILD_INTERFACE:${_3RDPARTY_NGHTTP2}/lib/includes>
            $<BUILD_INTERFACE:${_3RDPARTY_NGHTTP2}/build/lib/includes>)
        target_link_libraries(whttp PUBLIC ${NGHTTP2_STATIC_LIB})
    elseif(TARGET PkgConfig::NGHTTP2)
        target_link_libraries(whttp PUBLIC PkgConfig::NGHTTP2)
    else()
        target_include_directories(whttp PUBLIC ${NGHTTP2_INCLUDE_DIRS})
        target_link_libraries(whttp PUBLIC ${NGHTTP2_LIBRARIES})
    endif()
endif()

# lsquic / BoringSSL (HTTP/3)
if(WT_HAS_LSQUIC)
    message(STATUS "whttp: HTTP/3 enabled (lsquic found)")
    target_include_directories(whttp PRIVATE ${LSQUIC_INCLUDE_DIR})
    target_link_libraries(whttp PUBLIC ${LSQUIC_LIBRARY})
    target_compile_definitions(whttp PUBLIC WT_WITH_HTTP3)

    if(BORINGSSL_INCLUDE_DIR)
        set(_NEXUS_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
        if(BORINGSSL_PREFIX_INCLUDE_DIR)
            set(_NEXUS_INCLUDE_DIRS "${BORINGSSL_PREFIX_INCLUDE_DIR};${_NEXUS_INCLUDE_DIRS}")
        endif()
        set(_NEXUS_INCLUDE_DIRS "${BORINGSSL_INCLUDE_DIR};${LSQUIC_INCLUDE_DIR};${_NEXUS_INCLUDE_DIRS}")
        set_source_files_properties(
            ${NEXUS_FILES} Wt/cuehttp/h3_server.C PROPERTIES
            INCLUDE_DIRECTORIES "${_NEXUS_INCLUDE_DIRS}"
        )
        if(BORINGSSL_PREFIX_INCLUDE_DIR)
            set_source_files_properties(
                ${NEXUS_FILES} Wt/cuehttp/h3_server.C PROPERTIES
                COMPILE_DEFINITIONS "BORINGSSL_PREFIX=BSSL;LSQUIC_HAVE_OPENSSL=1;LSQUIC_HAVE_CXX11=1;LSQUIC_HAVE_CXX14=1;LSQUIC_HAVE_CXX17=1;LSQUIC_HAVE_CXX20=1"
            )
        endif()
    endif()
endif()

# io_uring
if(HAVE_URING)
    target_compile_definitions(whttp PUBLIC
        BOOST_ASIO_HAS_IO_URING ASIO_HAS_IO_URING
        BOOST_ASIO_DISABLE_EPOLL ASIO_DISABLE_EPOLL)
    target_link_libraries(whttp PUBLIC liburing.a)
endif()

# Install
install(TARGETS whttp
    EXPORT wt-target-whttp
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

install(EXPORT wt-target-whttp
    DESTINATION ${CMAKE_INSTALL_DIR}/wt
    NAMESPACE Wt::)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Wt/cuehttp
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/Wt)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Wt/AsioWrapper
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/Wt)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Wt/fmt
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/Wt)
