IF(ENABLE_LIBWTDBO)

  IF(NOT WT_DBO_CPP26_HARD_CUT)
    MESSAGE(FATAL_ERROR
      "WT_DBO_CPP26_HARD_CUT=OFF is not supported anymore. "
      "Wt::Dbo now requires C++26 reflection (<meta>) in hard-cut mode.")
  ENDIF(NOT WT_DBO_CPP26_HARD_CUT)

  INCLUDE(CheckCXXSourceCompiles)

  # GCC trunk currently requires -freflection to enable C++26 reflection syntax.
  # Clang-based toolchains with P2996 support should not require an extra flag.
  SET(_WT_DBO_REFLECTION_COMPILE_FLAGS "")
  IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    LIST(APPEND _WT_DBO_REFLECTION_COMPILE_FLAGS -freflection)
  ENDIF()

  SET(_WT_DBO_PREV_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
  SET(_WT_DBO_PREV_CMAKE_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
  SET(_WT_DBO_PREV_CMAKE_CXX_STANDARD_REQUIRED "${CMAKE_CXX_STANDARD_REQUIRED}")
  SET(_WT_DBO_PREV_CMAKE_CXX_EXTENSIONS "${CMAKE_CXX_EXTENSIONS}")

  SET(CMAKE_CXX_STANDARD 26)
  SET(CMAKE_CXX_STANDARD_REQUIRED ON)
  SET(CMAKE_CXX_EXTENSIONS OFF)

  IF(_WT_DBO_REFLECTION_COMPILE_FLAGS)
    STRING(JOIN " " _WT_DBO_REFLECTION_FLAGS_STR ${_WT_DBO_REFLECTION_COMPILE_FLAGS})
    SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${_WT_DBO_REFLECTION_FLAGS_STR}")
  ENDIF()

  CHECK_CXX_SOURCE_COMPILES("
    #if !defined(__has_include)
    # error __has_include_is_required
    #endif
    #if !__has_include(<meta>)
    # error std_meta_header_not_found
    #endif
    #include <meta>
    constexpr auto reflected = ^^int;
    int main() { return 0; }
  " WT_DBO_HAS_CPP26_REFLECTION)
  SET(CMAKE_REQUIRED_FLAGS "${_WT_DBO_PREV_CMAKE_REQUIRED_FLAGS}")
  SET(CMAKE_CXX_STANDARD "${_WT_DBO_PREV_CMAKE_CXX_STANDARD}")
  SET(CMAKE_CXX_STANDARD_REQUIRED "${_WT_DBO_PREV_CMAKE_CXX_STANDARD_REQUIRED}")
  SET(CMAKE_CXX_EXTENSIONS "${_WT_DBO_PREV_CMAKE_CXX_EXTENSIONS}")

  IF(NOT WT_DBO_HAS_CPP26_REFLECTION)
    MESSAGE(FATAL_ERROR
      "WT_DBO_CPP26_HARD_CUT=ON requires a C++26 reflection toolchain with <meta> support for Wt::Dbo (target wtdbo).")
  ENDIF(NOT WT_DBO_HAS_CPP26_REFLECTION)

  IF(WIN32)
    IF(SHARED_LIBS)
      CONFIGURE_FILE(wtdbo-version.rc.in
                     ${CMAKE_CURRENT_BINARY_DIR}/wtdbo-version.rc)
      SET(wtdboversionrc ${CMAKE_CURRENT_BINARY_DIR}/wtdbo-version.rc)
    ENDIF(SHARED_LIBS)
  ENDIF(WIN32)

  ADD_LIBRARY(wtdbo
    ${wtdboversionrc}
    ${CMAKE_CURRENT_SOURCE_DIR}/../fmtlog.h ${CMAKE_CURRENT_SOURCE_DIR}/../fmtlog.cc

    # core/
    core/DboTraits.h
    core/fk.h
    core/Error.h core/Compat.h core/ForeignKey.h
    core/Exception.h core/Exception.cpp

    # sql/
    sql/Parameter.h
    sql/Connection.h sql/Connection.cpp
    sql/ConnectionBase.h
    sql/Statement.h sql/Statement.cpp
    sql/Result.h sql/Result.cpp
    sql/Traits.h sql/Traits_impl.h sql/Traits.cpp
    sql/StdTraits.h sql/StdTraits.cpp
    sql/WtTraits.h sql/WtTraits.cpp
    sql/Pool.h sql/Pool.cpp
    sql/FixedPool.h sql/FixedPool.cpp
    sql/QueryParse.cpp
    sql/Util.h

    # reflect/
    reflect/Meta.h reflect/Traits.h reflect/Iterators.h reflect/Friend.h
    reflect/Registry.h
    reflect/Schema.h reflect/Actions.h reflect/Drop.h reflect/Json.h reflect/Sql.h reflect/Ddl.h
    reflect/Dialect.h

    # session/
    session/VersionTracker.h
    session/Session.h session/Session_impl.h session/Session.cpp
    session/Query.h session/Query_impl.h session/Query.cpp
    session/QueryColumn.h session/QueryColumn.cpp
    session/QueryModel.h session/QueryModel_impl.h
    session/Transaction.h session/Transaction.cpp
    session/Call.h session/Call_impl.h session/Call.cpp

    # root
    Json.h Json.cpp
    EscapeOStream.h EscapeOStream.cpp
    Logger.h Logger.cpp
    Redis.h Redis.cpp
  )

  SET_PROPERTY(TARGET wtdbo PROPERTY C_VISIBILITY_PRESET hidden)
  SET_PROPERTY(TARGET wtdbo PROPERTY CXX_VISIBILITY_PRESET hidden)
  SET_PROPERTY(TARGET wtdbo PROPERTY VISIBILITY_INLINES_HIDDEN YES)
  SET_TARGET_PROPERTIES(wtdbo PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )
  TARGET_COMPILE_FEATURES(wtdbo PUBLIC cxx_std_26)
  TARGET_COMPILE_OPTIONS(wtdbo PRIVATE ${_WT_DBO_REFLECTION_COMPILE_FLAGS})

  TARGET_COMPILE_DEFINITIONS(wtdbo PUBLIC WT_DBO_CPP26_HARD_CUT=1)
  TARGET_COMPILE_DEFINITIONS(wtdbo PRIVATE WT_BUILDING)

  IF(HAVE_URING)
    ADD_DEFINITIONS(-DASIO_HAS_IO_URING=1 -DASIO_DISABLE_EPOLL=1)
  ENDIF(HAVE_URING)

  IF(HAVE_UNWIND)
    TARGET_LINK_LIBRARIES(wtdbo PRIVATE ${UNWIND_LIBRARIES})
  ENDIF(HAVE_UNWIND)

  IF(TARGET Boost::headers)
    TARGET_LINK_LIBRARIES(wtdbo PRIVATE Boost::headers)
  ENDIF()

  IF(MULTI_THREADED_BUILD)
    TARGET_LINK_LIBRARIES(wtdbo
                          PRIVATE
                          ${WT_THREAD_LIB})
  ENDIF(MULTI_THREADED_BUILD)

  TARGET_INCLUDE_DIRECTORIES(wtdbo
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
      $<INSTALL_INTERFACE:include>
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src/web
      ${CMAKE_BINARY_DIR})

  INSTALL(TARGETS wtdbo
      EXPORT wt-target-dbo
      RUNTIME DESTINATION bin
      LIBRARY DESTINATION ${LIB_INSTALL_DIR}
      ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

  INSTALL(EXPORT wt-target-dbo
          DESTINATION ${CMAKE_INSTALL_DIR}/wt
          NAMESPACE Wt::)

  SET_TARGET_PROPERTIES(
    wtdbo
  PROPERTIES
    EXPORT_NAME Dbo
    VERSION ${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}
    DEBUG_POSTFIX ${DEBUG_LIB_POSTFIX}
  )
  IF(MSVC)
    SET_TARGET_PROPERTIES(
      wtdbo
    PROPERTIES
      COMPILE_FLAGS "${BUILD_PARALLEL} /wd4251 /wd4275 /wd4267"
    )
	SET_TARGET_PROPERTIES(wtdbo PROPERTIES FOLDER "dbo")
  ENDIF(MSVC)

  SUBDIRS(backend)

  INSTALL_FILES(/include/Wt/Dbo "^[^.]+\\.h$")
  INSTALL_FILES(/include/Wt/Dbo/core "^[^.]+\\.h$")
  INSTALL_FILES(/include/Wt/Dbo/sql "^[^.]+\\.h$")
  INSTALL_FILES(/include/Wt/Dbo/reflect "^[^.]+\\.h$")
  INSTALL_FILES(/include/Wt/Dbo/session "^[^.]+\\.h$")
  INSTALL_FILES(/include/Wt FILES ${WCONFIG_H_PATH})

  IF (ASCIIDOC_FOUND)
    ASCIIDOC_FILE(dbo-tutorial
      ${CMAKE_SOURCE_DIR}/doc/tutorial/dbo.doc
      ${CMAKE_SOURCE_DIR}/doc/tutorial/dbo.html)
  ENDIF (ASCIIDOC_FOUND)

ELSE (ENABLE_LIBWTDBO)
  MESSAGE("** libwtdbo disabled, not building Wt::Dbo.")
ENDIF (ENABLE_LIBWTDBO)
