# ---------------------------------------------------------------------------
# wt-core — Lifecycle, signals, base types, utilities, HTTP wrappers,
#            web internals, rendering, JSON, configuration, mail, crypto.
#
# Provides the foundation layer that wt-widgets (and user code) builds on.
# Links PUBLIC whttp so consumers automatically get HTTP/2+3 support.
# ---------------------------------------------------------------------------

set(WT_CORE_SOURCES
  # -- fmtlog --
  Wt/fmtlog.h Wt/fmtlog.cc

  # -- Utilities --
  Wt/Utils.h Wt/Utils.C

  # -- Any --
  Wt/WAny.h Wt/WAny.C

  # -- Application / Environment / Events --
  Wt/WApplication.h Wt/WApplication.C
  Wt/WEnvironment.h Wt/WEnvironment.C
  Wt/WEvent.h Wt/WEvent.C
  Wt/WException.h Wt/WException.C

  # -- Resources --
  Wt/WFileResource.h Wt/WFileResource.C
  Wt/WMemoryResource.h Wt/WMemoryResource.C
  Wt/WResource.h Wt/WResource.C
  Wt/WStreamResource.h Wt/WStreamResource.C

  # -- I/O, Server --
  Wt/WIOService.h Wt/WIOService.C
  Wt/WServer.h Wt/WServer.C

  # -- Date / Time --
  Wt/WLocale.h Wt/WLocale.C
  Wt/WLocalDateTime.h Wt/WLocalDateTime.C
  Wt/WDate.h Wt/WDate.C
  Wt/WDateTime.h Wt/WDateTime.C
  Wt/WTime.h Wt/WTime.C

  # -- Link --
  Wt/WLink.h Wt/WLink.C

  # -- Localisation --
  Wt/WLocalizedStrings.h Wt/WLocalizedStrings.C
  Wt/WCombinedLocalizedStrings.h Wt/WCombinedLocalizedStrings.C
  Wt/WMessageResourceBundle.h Wt/WMessageResourceBundle.C
  Wt/WMessageResources.h Wt/WMessageResources.C

  # -- Logging --
  Wt/WLogger.h Wt/WLogger.C
  Wt/WLogSink.h Wt/WLogSink.C

  # -- Object model / Signals / Slots --
  Wt/WObject.h Wt/WObject.C
  Wt/WSignal.h Wt/WSignal.C
  Wt/WStatelessSlot.h Wt/WStatelessSlot.C
  Wt/WSocketNotifier.h Wt/WSocketNotifier.C

  # -- Random --
  Wt/WRandom.h Wt/WRandom.C

  # -- String --
  Wt/WString.h Wt/WString.C
  Wt/WStringStream.h Wt/WStringStream.C
  Wt/WStringUtil.h Wt/WStringUtil.C

  # -- SSL --
  Wt/WSslCertificate.h Wt/WSslCertificate.C
  Wt/WSslInfo.h Wt/WSslInfo.C

  # -- JavaScript support --
  Wt/WJavaScript.h Wt/WJavaScript.C
  Wt/WJavaScriptSlot.h Wt/WJavaScriptSlot.C
  Wt/WJavaScriptExposableObject.h Wt/WJavaScriptExposableObject.C
  Wt/WJavaScriptHandle.h Wt/WJavaScriptHandle.C
  Wt/WJavaScriptObjectStorage.h Wt/WJavaScriptObjectStorage.C
  Wt/WJavaScriptPreamble.h Wt/WJavaScriptPreamble.C

  # -- Configuration --
  Wt/Configuration.h Wt/Configuration.C

  # -- WebController --
  Wt/WebController.h Wt/WebController.C

  # -- Core observables --
  Wt/Core/observable.hpp Wt/Core/observable.cpp
  Wt/Core/observing_ptr.hpp Wt/Core/observing_ptr.cpp

  # -- C++ compatibility / standard library extensions --
  Wt/cpp17/any.hpp Wt/cpp17/any/any.hpp
  Wt/cpp20/date.hpp Wt/cpp20/tz.hpp Wt/cpp20/async_mutex.cpp Wt/cpp20/async_mutex.h
  Wt/cpp20/async_mutex.hpp
  Wt/cpp23/MoveOnlyFunction.hpp
  Wt/cpp23/embed.hpp
  Wt/cpp23/httpresponse.hpp
  Wt/cpp26/reflection.hpp
  Wt/cpp26/WWidget-impl.hpp

  # -- JSON --
  Wt/Json/json.hpp
  Wt/Json/simdjson.h Wt/Json/simdjson.cpp
  Wt/Json/Parser.h Wt/Json/Parser.C
  Wt/Json/Serializer.h Wt/Json/Serializer.C

  # -- HTTP --
  Wt/Http/Configuration.h Wt/Http/Configuration.C
  Wt/Http/HttpUtils.h Wt/Http/HttpUtils.C
  Wt/Http/Client.h Wt/Http/Client.C
  Wt/Http/Cookie.h Wt/Http/Cookie.C
  Wt/Http/Message.h Wt/Http/Message.C
  Wt/Http/Request.h Wt/Http/Request.C
  Wt/Http/Response.h Wt/Http/Response.C
  Wt/Http/ResponseContinuation.h Wt/Http/ResponseContinuation.C
  Wt/Http/WtClient.h Wt/Http/WtClient.C

  # -- Mail --
  Wt/Mail/Client.h Wt/Mail/Client.C
  Wt/Mail/Mailbox.h Wt/Mail/Mailbox.C
  Wt/Mail/Message.h Wt/Mail/Message.C

  # -- Signals library --
  Wt/Signals/signals.hpp Wt/Signals/signals.cpp
  Wt/Signals/nano_function.hpp Wt/Signals/nano_mutex.hpp
  Wt/Signals/nano_observer.hpp Wt/Signals/nano_signal_slot.hpp

  # -- Base64 (turbo) --
  Wt/turbo-base64/turbob64.h Wt/turbo-base64/turbob64c.c
  Wt/turbo-base64/turbob64d.c Wt/turbo-base64/turbob64sse.c
  Wt/turbo-base64/turbob64avx2.c

  # -- Geometry / math primitives --
  Wt/WColor.h Wt/WColor.C
  Wt/WLength.h Wt/WLength.C
  Wt/WFont.h Wt/WFont.C
  Wt/WFontMetrics.h Wt/WFontMetrics.C
  Wt/WPoint.h Wt/WPoint.C
  Wt/WPointF.h Wt/WPointF.C
  Wt/WLineF.h Wt/WLineF.C
  Wt/WRectF.h Wt/WRectF.C
  Wt/WAnimation.h Wt/WAnimation.C
  Wt/WBorder.h Wt/WBorder.C
  Wt/WBrush.h Wt/WBrush.C
  Wt/WGradient.h Wt/WGradient.C
  Wt/WPen.h Wt/WPen.C
  Wt/WShadow.h Wt/WShadow.C
  Wt/WTransform.h Wt/WTransform.C
  Wt/WMatrix4x4.h Wt/WMatrix4x4.C
  Wt/WVector3.h Wt/WVector3.C
  Wt/WVector4.h Wt/WVector4.C

  # -- Model --
  Wt/WModelIndex.h Wt/WModelIndex.C
  Wt/WFormModel.h Wt/WFormModel.C

  # -- CSS --
  Wt/WCssDecorationStyle.h Wt/WCssDecorationStyle.C
  Wt/WCssStyleSheet.h Wt/WCssStyleSheet.C
  Wt/WLinkedCssStyleSheet.h Wt/WLinkedCssStyleSheet.C

  # -- Paint --
  Wt/WPaintDevice.h Wt/WPaintDevice.C
  Wt/WPainter.h Wt/WPainter.C
  Wt/WPainterPath.h Wt/WPainterPath.C

  # -- Validator --
  Wt/WValidator.h Wt/WValidator.C

  # -- cuehttp deps (ada.cpp moved to whttp) --

  # -- Render engine --
  Wt/Render/Block.h Wt/Render/Block.C
  Wt/Render/CssData.h Wt/Render/CssData.C
  Wt/Render/CssData_p.h Wt/Render/CssData_p.C
  Wt/Render/CssParser.h Wt/Render/CssParser.C
  Wt/Render/Line.h Wt/Render/Line.C
  Wt/Render/LayoutBox.h Wt/Render/LayoutBox.C
  Wt/Render/RenderUtils.h Wt/Render/RenderUtils.C
  Wt/Render/Specificity.h Wt/Render/Specificity.C
  Wt/Render/WTextRenderer.h Wt/Render/WTextRenderer.C

  # -- Template (core variant) --
  Wt/WTemplate_.C Wt/WTemplate_.h

  # -- cuehttp headers (header-only, for IDE) --
  Wt/cuehttp/compress.hpp Wt/cuehttp/context.hpp Wt/cuehttp/cookies.hpp
  Wt/cuehttp/cuehttp.hpp
  Wt/cuehttp/deps/json.hpp Wt/cuehttp/deps/picohttpparser.h
  Wt/cuehttp/detail/body_stream.hpp Wt/cuehttp/detail/buffered_streambuf.hpp
  Wt/cuehttp/detail/common.hpp Wt/cuehttp/detail/connection.hpp
  Wt/cuehttp/detail/connection_old.hpp Wt/cuehttp/detail/endian.hpp
  Wt/cuehttp/detail/MoveOnlyFunction.hpp
  Wt/cuehttp/detail/engines.hpp Wt/cuehttp/detail/gzip.hpp
  Wt/cuehttp/detail/middlewares.hpp Wt/cuehttp/detail/mime.hpp
  Wt/cuehttp/detail/noncopyable.hpp Wt/cuehttp/detail/send.hpp
  Wt/cuehttp/detail/sha1.hpp Wt/cuehttp/detail/static.hpp
  Wt/cuehttp/detail/stream.hpp
  Wt/cuehttp/request.hpp Wt/cuehttp/response.hpp Wt/cuehttp/router.hpp
  Wt/cuehttp/send.hpp Wt/cuehttp/server.hpp Wt/cuehttp/session.hpp
  Wt/cuehttp/static.hpp Wt/cuehttp/use_session.hpp
  Wt/cuehttp/websocket.hpp Wt/cuehttp/ws_server.hpp

  # -- web internals --
  web/fasthex/hex.h web/fasthex/hex.cc
  web/md5.h web/md5.c
  web/sha1.h web/sha1.c
  web/CgiParser.h web/CgiParser.C
  web/DomElement.h web/DomElement.C
  web/EntryPoint.h web/EntryPoint.C
  web/EscapeOStream.h web/EscapeOStream.C
  web/FileServe.h web/FileServe.C
  web/ColorUtils.h web/ColorUtils.C
  web/ImageUtils.h web/ImageUtils.C
  web/RefEncoder.h web/RefEncoder.C
  web/SoundManager.h web/SoundManager.C
  web/WebMain.h web/WebMain.C
  web/WebRequest.h web/WebRequest.C
  web/WebStream.h web/WebStream.C
  web/WebSession.h web/WebSession.C
  web/WebSocketMessage.h web/WebSocketMessage.C
  web/WebRenderer.h web/WebRenderer.C
  web/WebUtils.h web/WebUtils.C
  web/FileUtils.h web/FileUtils.C
  web/PdfUtils.h web/PdfUtils.C
  web/StringUtils.h web/StringUtils.cpp
  web/TimeUtil.h web/TimeUtil.C
  web/XSSFilter.h web/XSSFilter.C
  web/XSSUtils.h web/XSSUtils.C
  web/SslUtils.h web/SslUtils.C
  web/UriUtils.h web/UriUtils.C
  web/base64.h web/base64.cpp
)

# ---------------------------------------------------------------------------
# Conditional core sources
# ---------------------------------------------------------------------------

# -- Date/tz implementation (Howard Hinnant) --
IF(WT_CPP20_DATE_TZ_IMPLEMENTATION STREQUAL "date")
  list(APPEND WT_CORE_SOURCES
    Wt/Date/date.h Wt/Date/include/date/date.h
    Wt/Date/tz.h Wt/Date/include/date/tz.h Wt/Date/src/tz.cpp)

  set(_TZ_BUILD_DEFS "")
  IF(WIN32)
    set(_TZ_BUILD_DEFS USE_OS_TZDB=0 HAS_REMOTE_API=0)
  ELSE()
    set(_TZ_BUILD_DEFS USE_OS_TZDB=1)
  ENDIF()
  IF(SHARED_LIBS)
    list(APPEND _TZ_BUILD_DEFS DATE_BUILD_DLL)
  ELSE()
    list(APPEND _TZ_BUILD_DEFS DATE_BUILD_LIB)
  ENDIF()
  set_source_files_properties(Wt/Date/src/tz.cpp
    PROPERTIES COMPILE_DEFINITIONS "${_TZ_BUILD_DEFS}")
  unset(_TZ_BUILD_DEFS)
ENDIF()

# -- SAML (OpenSAML) --
IF(WT_HAS_SAML)
  set(_saml_sources
    Wt/Auth/Saml/Assertion.h Wt/Auth/Saml/Assertion.C
    Wt/Auth/Saml/Process.h Wt/Auth/Saml/Process.C
    Wt/Auth/Saml/ProcessImpl.h Wt/Auth/Saml/ProcessImpl.C
    Wt/Auth/Saml/Service.h Wt/Auth/Saml/Service.C
    Wt/Auth/Saml/ServiceImpl.h Wt/Auth/Saml/ServiceImpl.C
    Wt/Auth/Saml/Widget.h Wt/Auth/Saml/Widget.C
  )
  set_source_files_properties(${_saml_sources}
    PROPERTIES COMPILE_DEFINITIONS XSEC_HAVE_OPENSSL=1)
  list(APPEND WT_CORE_SOURCES ${_saml_sources})
ENDIF()

# -- bcrypt (always built) --
list(APPEND WT_CORE_SOURCES
  Wt/Auth/bcrypt/crypt_blowfish.h Wt/Auth/bcrypt/crypt_blowfish.c
  Wt/Auth/bcrypt/crypt_gensalt.h Wt/Auth/bcrypt/crypt_gensalt.c
  Wt/Auth/bcrypt/wrapper.c)

# -- SocketNotifier (multi-threaded only) --
IF(MULTI_THREADED_BUILD)
  list(APPEND WT_CORE_SOURCES web/SocketNotifier.h web/SocketNotifier.C)
ENDIF()

# -- Windows version resource --
IF(WIN32 AND SHARED_LIBS)
  CONFIGURE_FILE(Wt/wt-version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/wt-core-version.rc)
  list(APPEND WT_CORE_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/wt-core-version.rc)
ENDIF()

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
ADD_LIBRARY(wt-core ${WT_CORE_SOURCES})

target_compile_definitions(wt-core PRIVATE wt_EXPORTS)

SET_PROPERTY(TARGET wt-core PROPERTY C_VISIBILITY_PRESET hidden)
SET_PROPERTY(TARGET wt-core PROPERTY CXX_VISIBILITY_PRESET hidden)
SET_PROPERTY(TARGET wt-core PROPERTY VISIBILITY_INLINES_HIDDEN YES)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
TARGET_INCLUDE_DIRECTORIES(wt-core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/web>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Wt/Date/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    Wt/AsioWrapper
    Wt/Dbo/backend/amalgamation
)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------

# whttp brings HTTP/2+3 transport
TARGET_LINK_LIBRARIES(wt-core PUBLIC whttp)

# Boost
if(WT_ASIO_IS_STANDALONE_ASIO)
  TARGET_LINK_LIBRARIES(wt-core PRIVATE ${BOOST_WT_LIBRARIES})
else()
  TARGET_LINK_LIBRARIES(wt-core PUBLIC ${BOOST_WT_LIBRARIES})
endif()

TARGET_LINK_LIBRARIES(wt-core
  PUBLIC
    ${WT_SOCKET_LIBRARY}
  PRIVATE
    ${WT_SAML_LIBS}
    ${WT_MATH_LIBRARY}
    ${RT_LIBRARY}
)

# -- glaze (JSON) --
TARGET_LINK_LIBRARIES(wt-core PRIVATE glaze::glaze)

# -- OpenSSL --
if(OpenSSL_FOUND)
  TARGET_LINK_LIBRARIES(wt-core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  if(WIN32)
    TARGET_LINK_LIBRARIES(wt-core PRIVATE Crypt32)
  endif()
endif()

# -- Brotli --
if(Brotli_FOUND)
  TARGET_LINK_LIBRARIES(wt-core PUBLIC brotlidec brotlienc brotlicommon)
endif()

# Zlib and io_uring defs/libs are inherited transitively from whttp (PUBLIC)

# -- SAML --
IF(WT_HAS_SAML)
  TARGET_LINK_LIBRARIES(wt-core PRIVATE Shibboleth::OpenSAML)
ENDIF()

# -- Thread library --
IF(MULTI_THREADED_BUILD)
  TARGET_LINK_LIBRARIES(wt-core PRIVATE ${WT_THREAD_LIB})
ENDIF()

# -- libunwind (stacktraces) --
IF(HAVE_UNWIND)
  TARGET_LINK_LIBRARIES(wt-core PRIVATE ${UNWIND_LIBRARIES})
  TARGET_INCLUDE_DIRECTORIES(wt-core PRIVATE ${UNWIND_INCLUDE_DIRS})
ENDIF()

# -- MSVC specifics --
IF(MSVC)
  SET_TARGET_PROPERTIES(wt-core PROPERTIES
    COMPILE_FLAGS "${BUILD_PARALLEL} /wd4251 /wd4275 /wd4355 /wd4800 /wd4996 /wd4101 /wd4267")
  TARGET_LINK_LIBRARIES(wt-core PRIVATE winmm)
ENDIF()

# ---------------------------------------------------------------------------
# Version / install
# ---------------------------------------------------------------------------
SET_TARGET_PROPERTIES(wt-core
  PROPERTIES
    EXPORT_NAME WtCore
    VERSION ${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}
    DEBUG_POSTFIX ${DEBUG_LIB_POSTFIX}
)

INSTALL(TARGETS wt-core
    EXPORT wt-target-wt-core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

INSTALL(EXPORT wt-target-wt-core
        DESTINATION ${CMAKE_INSTALL_DIR}/wt
        NAMESPACE Wt::)
