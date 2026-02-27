# ===========================================================================
# src/CMakeLists.txt — Orchestrator
#
# Defines common settings, then includes per-module cmake files:
#   cmake/whttp.cmake       → libwhttp.so   (standalone HTTP/1.1+2+3)
#   cmake/wt-core.cmake     → libwt-core.so (lifecycle, signals, types, web)
#   cmake/wt-widgets.cmake  → libwt-widgets.so + wt INTERFACE alias
# ===========================================================================

add_subdirectory(web)
add_subdirectory(Wt)

# ---------------------------------------------------------------------------
# Compiler standards and flags
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
include(CheckCXXCompilerFlag)

set(_wt_extra_cxx_flags
    "-ftemplate-depth=40960"
    "-Wno-deprecated-literal-operator"
    "-march=native"
)

check_cxx_compiler_flag("-fconstexpr-steps=100000000" WT_HAS_FCONSTEXPR_STEPS)
if(WT_HAS_FCONSTEXPR_STEPS)
    list(APPEND _wt_extra_cxx_flags "-fconstexpr-steps=100000000")
endif()

string(JOIN " " _wt_extra_cxx_flags_joined ${_wt_extra_cxx_flags})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_wt_extra_cxx_flags_joined}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

set(_wt_web_renderer_compile_options "-ftemplate-depth=40960")
if(WT_HAS_FCONSTEXPR_STEPS)
    list(APPEND _wt_web_renderer_compile_options "-fconstexpr-steps=100000000")
endif()

set_source_files_properties(
    WebRenderer.C
    PROPERTIES
        COMPILE_OPTIONS "${_wt_web_renderer_compile_options}"
)

# ---------------------------------------------------------------------------
# Global compile definitions
# ---------------------------------------------------------------------------
add_definitions(-DFMT_HEADER_ONLY)
add_definitions(-DBOOST_JSON_STANDALONE)
add_definitions(-DWT_BUILDING)

# ---------------------------------------------------------------------------
# Global include directories (consumed by all targets via CURRENT_SOURCE_DIR)
# ---------------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/web
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Wt/Date/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${BOOST_INCLUDE_DIRS}
    Wt/Dbo/backend/amalgamation
)

link_directories(${BOOST_LIB_DIRS})

# ---------------------------------------------------------------------------
# file_to_string helper (for embedded resources)
# ---------------------------------------------------------------------------
function(file_to_string infile outfile var)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${outfile}
        COMMAND ${CMAKE_COMMAND}
            ARGS -Dinfile:FILEPATH=${CMAKE_CURRENT_SOURCE_DIR}/${infile}
                 -Doutfile:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/${outfile}
                 -Dvar:STRING=${var}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/filetostring.cmake
        MAIN_DEPENDENCY
            ${CMAKE_CURRENT_SOURCE_DIR}/${infile}
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/filetostring.cmake
    )
endfunction()

# ---------------------------------------------------------------------------
# IDE embed-files targets
# ---------------------------------------------------------------------------
file(GLOB SKELETON_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/web/skeleton/*.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/skeleton/*.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/web/skeleton/*.mjs"
)
file(GLOB JS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/js/*.js")
file(GLOB XML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/xml/*.xml")

set(EMBED_FILES ${SKELETON_FILES} ${JS_FILES} ${XML_FILES})
add_custom_target(embed_files SOURCES ${EMBED_FILES})

set(DIR_TO_INCLUDE "/web/skeleton")
file(GLOB_RECURSE DIR_FILES "${DIR_TO_INCLUDE}/*")
add_custom_target(directory_display SOURCES ${DIR_FILES})

# ---------------------------------------------------------------------------
# NEXUS_FILES — shared between whttp and wt (IDE sources if lsquic disabled)
# ---------------------------------------------------------------------------
message(STATUS "WT_HAS_LSQUIC => ${WT_HAS_LSQUIC}")

set(NEXUS_FILES
    Wt/cuehttp/nexus/global/context.hpp Wt/cuehttp/nexus/global/error.hpp
    Wt/cuehttp/nexus/h3/client.hpp Wt/cuehttp/nexus/h3/error.hpp Wt/cuehttp/nexus/h3/fields.hpp
    Wt/cuehttp/nexus/h3/server.hpp Wt/cuehttp/nexus/h3/stream.hpp
    Wt/cuehttp/nexus/quic/detail/connection_impl.hpp Wt/cuehttp/nexus/quic/detail/connection_state.hpp
    Wt/cuehttp/nexus/quic/detail/engine_impl.hpp Wt/cuehttp/nexus/quic/detail/handler_ptr.hpp
    Wt/cuehttp/nexus/quic/detail/operation.hpp Wt/cuehttp/nexus/quic/detail/service.hpp
    Wt/cuehttp/nexus/quic/detail/socket_impl.hpp Wt/cuehttp/nexus/quic/detail/stream_impl.hpp
    Wt/cuehttp/nexus/quic/detail/stream_open_handler.hpp Wt/cuehttp/nexus/quic/detail/stream_state.hpp
    Wt/cuehttp/nexus/quic/client.hpp Wt/cuehttp/nexus/quic/connection.hpp Wt/cuehttp/nexus/quic/connection_id.hpp
    Wt/cuehttp/nexus/quic/error.hpp Wt/cuehttp/nexus/quic/server.hpp Wt/cuehttp/nexus/quic/settings.hpp
    Wt/cuehttp/nexus/quic/socket.hpp Wt/cuehttp/nexus/quic/stream.hpp Wt/cuehttp/nexus/quic/stream_id.hpp
    Wt/cuehttp/nexus/client.cc Wt/cuehttp/nexus/connection_state.cc Wt/cuehttp/nexus/engine.cc
    Wt/cuehttp/nexus/error.cc Wt/cuehttp/nexus/error_code.hpp
    Wt/cuehttp/nexus/global.cc Wt/cuehttp/nexus/global_init.hpp Wt/cuehttp/nexus/h3.hpp
    Wt/cuehttp/nexus/nexus.hpp Wt/cuehttp/nexus/qconnection.cc Wt/cuehttp/nexus/quic.hpp
    Wt/cuehttp/nexus/recv_header_set.hpp Wt/cuehttp/nexus/server.cc Wt/cuehttp/nexus/settings.cc
    Wt/cuehttp/nexus/socket.cc Wt/cuehttp/nexus/ssl.hpp Wt/cuehttp/nexus/stream.cc
    Wt/cuehttp/nexus/stream_state.cc Wt/cuehttp/nexus/udp.hpp
)

if(NOT WT_HAS_LSQUIC)
    add_custom_target(nexus_sources
        SOURCES ${NEXUS_FILES}
        COMMENT "Show nexus sources in the IDE"
    )
endif()

# ---------------------------------------------------------------------------
# RT library check
# ---------------------------------------------------------------------------
set(RT_test_fname "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/rt.c")
file(WRITE "${RT_test_fname}"
    "#include <time.h>\nint main() { struct timespec time1; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1); return 0; }")
try_compile(RT_required "${CMAKE_BINARY_DIR}" "${RT_test_fname}"
    CMAKE_FLAGS -DLINK_LIBRARIES:STRING=rt)
if(RT_required)
    set(RT_LIBRARY "rt")
endif()

# ---------------------------------------------------------------------------
# FetchContent: glaze (JSON library used by whttp session + wt-core)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glaze)

# ===========================================================================
# Module includes — order matters (whttp first, then wt-core, then widgets)
# ===========================================================================
include(cmake/whttp.cmake)
include(cmake/wt-core.cmake)
include(cmake/wt-widgets.cmake)

# ---------------------------------------------------------------------------
# wttest (optional test library)
# ---------------------------------------------------------------------------
if(ENABLE_LIBWTTEST AND NOT WT_UI2_HARD_CUT)
    add_library(wttest Wt/Test/WTestEnvironment.C)

    set_target_properties(wttest PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
        EXPORT_NAME Test
        VERSION ${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}
        DEBUG_POSTFIX ${DEBUG_LIB_POSTFIX}
    )

    target_link_libraries(wttest PUBLIC wt)

    if(TARGET Boost::headers)
        target_link_libraries(wttest PRIVATE Boost::headers)
    endif()

    if(MULTI_THREADED_BUILD)
        target_link_libraries(wttest PRIVATE ${WT_THREAD_LIB})
    endif()

    install(TARGETS wttest
        EXPORT wt-target-test
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

    install(EXPORT wt-target-test
        DESTINATION ${CMAKE_INSTALL_DIR}/wt
        NAMESPACE Wt::)

    if(MSVC)
        set_target_properties(wttest PROPERTIES
            COMPILE_FLAGS "${BUILD_PARALLEL} /wd4251 /wd4275 /wd4355 /wd4800 /wd4996 /wd4101 /wd4267")
        set_target_properties(wttest PROPERTIES FOLDER "test")
    endif()
elseif(ENABLE_LIBWTTEST AND WT_UI2_HARD_CUT)
    message("** Not building libwttest (WT_UI2_HARD_CUT enabled)")
else()
    message("** Not building libwttest")
endif()

# ---------------------------------------------------------------------------
# Install rules (headers, config script)
# ---------------------------------------------------------------------------
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Wt/cuehttp.hpp
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/Wt)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Wt/Configuration.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/Wt)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Wt/EntryPoint.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/Wt)

configure_file(
    ${CMAKE_SOURCE_DIR}/WtInstall.cmake
    ${CMAKE_BINARY_DIR}/WtInstall.cmake
)

# ---------------------------------------------------------------------------
# HTTP test server (standalone whttp test)
# ---------------------------------------------------------------------------
add_executable(http_test ${CMAKE_SOURCE_DIR}/examples/http_test/http_test.C)
target_link_libraries(http_test whttp)
