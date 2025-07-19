message("Project sources for WebAssembly client: ${PROJECT_SOURCE_DIR}")

if (NOT BUILD_WASM)
  return()
endif()

cmake_minimum_required(VERSION 3.28)
project(wasm_client LANGUAGES C CXX)

SET(VERSION_SERIES 4)
SET(VERSION_MAJOR 7)
SET(VERSION_MINOR 0)

set(SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../src")

message(STATUS "WebAssembly client sources root: ${SRC_ROOT}")
# 1) pull in Emscripten
# if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
#   message(FATAL_ERROR
#     "set -DCMAKE_TOOLCHAIN_FILE=/path/to/emsdk/upstream/emscripten")
# endif()
# set(CMAKE_TOOLCHAIN_FILE
#     "${CMAKE_TOOLCHAIN_FILE}/cmake/Modules/Platform/Emscripten.cmake"
#     CACHE STRING "")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-DFMT_HEADER_ONLY)
add_definitions(-DBOOST_JSON_STANDALONE)
add_definitions(-DWT_CPP20_DATE_TZ_IMPLEMENTATION=std)

set(WT_DATE_TZ_USE_DATE ON)
SET(WT_DATE_TZ_USE_STD OFF)
SET(WT_THREADED true)
# 2) compiler flags
# set(WASM_CXX_FLAGS
#   "-std=c++23"
#   "-O3"
#   "-s WASM=1"
#   "-s MODULARIZE=1"
#   "-s EXPORT_NAME=\"WtWasm\""
#   "-s ALLOW_MEMORY_GROWTH=1"
#   "-sENVIRONMENT=web"                 # optional but tidy
#   "--bind"
#   "-s USE_BOOST_HEADERS=1"
#   "-s" "FETCH=1"
# )
# # add_compile_options("-s" "USE_BOOST_HEADERS=1")
# # add_link_options   ("-s" "USE_BOOST_HEADERS=1")
# #add_compile_options("--bind -s FETCH=1")

# string(REPLACE ";" " " WASM_FLAGS_STR "${WASM_CXX_FLAGS}")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WASM_FLAGS_STR}")



# 3) your own “web/” sources
#
#  Stand-alone DOM + WebAssembly subset
#
set(WASM_REL_SOURCES
  # ─────────────────── core runtime ───────────────────
  #../src/Wt/
  Wt/WWebWidget.h             Wt/WWebWidget.C
  Wt/WWidget.h                Wt/WWidget.C
  Wt/WWidgetItem.h            Wt/WWidgetItem.C
  Wt/WWidgetItemImpl.h        Wt/WWidgetItemImpl.C
  Wt/WObject.h                Wt/WObject.C
  Wt/WSignal.h                Wt/WSignal.C        # nano-signal impl
  Wt/Signals/signals.hpp      Wt/Signals/signals.cpp
  Wt/Signals/nano_function.hpp
  Wt/Signals/nano_mutex.hpp
  Wt/Signals/nano_observer.hpp
  Wt/Signals/nano_signal_slot.hpp

  # ─────────────────── utility / math ─────────────────
  Wt/Utils.h                  Wt/Utils.C
  Wt/WMatrix4x4.h             Wt/WMatrix4x4.C
  Wt/WVector3.h               Wt/WVector3.C
  Wt/WVector4.h               Wt/WVector4.C
  Wt/WPoint.h                 Wt/WPoint.C
  Wt/WPointF.h                Wt/WPointF.C
  Wt/WLineF.h                 Wt/WLineF.C
  Wt/WRectF.h                 Wt/WRectF.C
  Wt/WPolygonArea.h           Wt/WPolygonArea.C
  Wt/WCircleArea.h            Wt/WCircleArea.C

  # ─────────────────── resource helpers (client-side only) ────────────────
  # Wt/WMemoryResource.h        Wt/WMemoryResource.C   # in-page blobs
  # Wt/WStreamResource.h        Wt/WStreamResource.C   # data URIs
  # Wt/WFileUpload.h            Wt/WFileUpload.C
  # Wt/WFileDropWidget.h        Wt/WFileDropWidget.C
  # (WResource / WFileResource intentionally excluded – server endpoints)

  # ─────────────────── painting / rendering ───────────────────────────────
  Wt/WPaintDevice.h           Wt/WPaintDevice.C
  Wt/WPainter.h               Wt/WPainter.C
  Wt/WPainterPath.h           Wt/WPainterPath.C
  Wt/WCanvasPaintDevice.h     Wt/WCanvasPaintDevice.C
  Wt/Render/Block.h           Wt/Render/Block.C
  Wt/Render/CssData.h         Wt/Render/CssData.C
  Wt/Render/CssData_p.h       Wt/Render/CssData_p.C
  Wt/Render/CssParser.h       Wt/Render/CssParser.C
  Wt/Render/LayoutBox.h       Wt/Render/LayoutBox.C
  Wt/Render/Line.h            Wt/Render/Line.C
  Wt/Render/RenderUtils.h     Wt/Render/RenderUtils.C
  Wt/Render/Specificity.h     Wt/Render/Specificity.C
  Wt/Render/WTextRenderer.h   Wt/Render/WTextRenderer.C

  # ─────────────────── widget toolkit (full client set) ───────────────────
  #   – everything that starts with “W” and is **not** in the omit-list
  #   – kept in alphabetical order so you can diff easily

  Wt/WAbstractArea.h              Wt/WAbstractArea.C
  Wt/WAbstractItemDelegate.h      Wt/WAbstractItemDelegate.C
  Wt/WAbstractItemModel.h         Wt/WAbstractItemModel.C
  Wt/WAbstractItemView.h          Wt/WAbstractItemView.C
  Wt/WAbstractListModel.h         Wt/WAbstractListModel.C
  Wt/WAbstractMedia.h             Wt/WAbstractMedia.C
  Wt/WAbstractProxyModel.h        Wt/WAbstractProxyModel.C
  Wt/WAbstractSpinBox.h           Wt/WAbstractSpinBox.C
  Wt/WAbstractTableModel.h        Wt/WAbstractTableModel.C
  Wt/WAbstractToggleButton.h      Wt/WAbstractToggleButton.C
  Wt/WAggregateProxyModel.h       Wt/WAggregateProxyModel.C
  Wt/WAnchor.h                    Wt/WAnchor.C
  Wt/WAnimation.h                 Wt/WAnimation.C
  Wt/WAny.h                       Wt/WAny.C
  Wt/WApplication.h               Wt/WApplication.C
  Wt/WAudio.h                     Wt/WAudio.C
  Wt/WBatchEditProxyModel.h       Wt/WBatchEditProxyModel.C
  Wt/WBorder.h                    Wt/WBorder.C
  Wt/WBorderLayout.h              Wt/WBorderLayout.C
  Wt/WBoxLayout.h                 Wt/WBoxLayout.C
  Wt/WBreak.h                     Wt/WBreak.C
  Wt/WBrush.h                     Wt/WBrush.C
  Wt/WButtonGroup.h               Wt/WButtonGroup.C
  Wt/WCalendar.h                  Wt/WCalendar.C
  Wt/WCheckBox.h                  Wt/WCheckBox.C
  Wt/WColor.h                     Wt/WColor.C
  Wt/WColorPicker.h               Wt/WColorPicker.C
  Wt/WCombinedLocalizedStrings.h  Wt/WCombinedLocalizedStrings.C
  Wt/WComboBox.h                  Wt/WComboBox.C
  Wt/WCompositeWidget.h           Wt/WCompositeWidget.C
  Wt/WContainerWidget.h           Wt/WContainerWidget.C
  Wt/WCssDecorationStyle.h        Wt/WCssDecorationStyle.C
  Wt/WCssStyleSheet.h             Wt/WCssStyleSheet.C
  Wt/WCssTheme.h                  Wt/WCssTheme.C
  Wt/WDate.h                      Wt/WDate.C
  Wt/WDateEdit.h                  Wt/WDateEdit.C
  Wt/WDatePicker.h                Wt/WDatePicker.C
  Wt/WDateTime.h                  Wt/WDateTime.C
  Wt/WDateValidator.h             Wt/WDateValidator.C
  Wt/WDefaultLoadingIndicator.h   Wt/WDefaultLoadingIndicator.C
  Wt/WDialog.h                    Wt/WDialog.C
  Wt/WDoubleSpinBox.h             Wt/WDoubleSpinBox.C
  Wt/WDoubleValidator.h           Wt/WDoubleValidator.C
  # Wt/WDropZone.h                  Wt/WDropZone.C
  Wt/WEnvironment.h               Wt/WEnvironment.C
  Wt/WEvent.h                     Wt/WEvent.C
  Wt/WException.h                 Wt/WException.C
  Wt/WFitLayout.h                 Wt/WFitLayout.C
  Wt/WFont.h                      Wt/WFont.C
  Wt/WFontMetrics.h               Wt/WFontMetrics.C
  Wt/WFormModel.h                 Wt/WFormModel.C
  Wt/WFormWidget.h                Wt/WFormWidget.C
  Wt/WGLWidget.h                  Wt/WGLWidget.C            # remove if you don't use WebGL
  Wt/WClientGLWidget.h            Wt/WClientGLWidget.C      # idem
  Wt/WGradient.h                  Wt/WGradient.C
  Wt/WGridLayout.h                Wt/WGridLayout.C
  Wt/WGroupBox.h                  Wt/WGroupBox.C
  Wt/WHBoxLayout.h                Wt/WHBoxLayout.C
  Wt/WIcon.h                      Wt/WIcon.C
  Wt/WIconPair.h                  Wt/WIconPair.C
  Wt/WImage.h                     Wt/WImage.C
  Wt/WInPlaceEdit.h               Wt/WInPlaceEdit.C
  Wt/WIntValidator.h              Wt/WIntValidator.C
  Wt/WInteractWidget.h            Wt/WInteractWidget.C
  Wt/WLabel.h                     Wt/WLabel.C
  Wt/WLayout.h                    Wt/WLayout.C
  Wt/WLayoutImpl.h                Wt/WLayoutImpl.C
  Wt/WLayoutItem.h                Wt/WLayoutItem.C
  Wt/WLayoutItemImpl.h            Wt/WLayoutItemImpl.C
  Wt/WLeafletMap.h                Wt/WLeafletMap.C          # delete if not needed
  Wt/WLength.h                    Wt/WLength.C
  Wt/WLengthValidator.h           Wt/WLengthValidator.C
  Wt/WLineEdit.h                  Wt/WLineEdit.C
  Wt/WLink.h                      Wt/WLink.C
  Wt/WLinkedCssStyleSheet.h       Wt/WLinkedCssStyleSheet.C
  Wt/WLoadingIndicator.h          Wt/WLoadingIndicator.C
  Wt/WLocalizedStrings.h          Wt/WLocalizedStrings.C
  Wt/WLogger.h                    Wt/WLogger.C
  Wt/WLogSink.h                   Wt/WLogSink.C
  Wt/WMeasurePaintDevice.h        Wt/WMeasurePaintDevice.C
  Wt/WMediaPlayer.h               Wt/WMediaPlayer.C
  Wt/WMenu.h                      Wt/WMenu.C
  Wt/WMenuItem.h                  Wt/WMenuItem.C
  Wt/WMessageBox.h                Wt/WMessageBox.C
  Wt/WMessageResourceBundle.h     Wt/WMessageResourceBundle.C
  Wt/WMessageResources.h          Wt/WMessageResources.C
  Wt/WModelIndex.h                Wt/WModelIndex.C
  Wt/WNavigationBar.h             Wt/WNavigationBar.C
  Wt/WOverlayLoadingIndicator.h   Wt/WOverlayLoadingIndicator.C
  Wt/WPanel.h                     Wt/WPanel.C
  Wt/WPen.h                       Wt/WPen.C
  Wt/WPopupMenu.h                 Wt/WPopupMenu.C
  Wt/WPopupWidget.h               Wt/WPopupWidget.C
  Wt/WProgressBar.h               Wt/WProgressBar.C
  Wt/WPushButton.h                Wt/WPushButton.C
  Wt/WRadioButton.h               Wt/WRadioButton.C
  Wt/WRandom.h                    Wt/WRandom.C
  Wt/WReadOnlyProxyModel.h        Wt/WReadOnlyProxyModel.C
  Wt/WRectArea.h                  Wt/WRectArea.C
  Wt/WRegExpValidator.h           Wt/WRegExpValidator.C
  Wt/WSelectionBox.h              Wt/WSelectionBox.C
  Wt/WShadow.h                    Wt/WShadow.C
  Wt/WSlider.h                    Wt/WSlider.C
  Wt/WSortFilterProxyModel.h      Wt/WSortFilterProxyModel.C
  Wt/WSpinBox.h                   Wt/WSpinBox.C
  Wt/WSplitButton.h               Wt/WSplitButton.C
  Wt/WStandardItem.h              Wt/WStandardItem.C
  Wt/WStandardItemModel.h         Wt/WStandardItemModel.C
  Wt/WStatelessSlot.h             Wt/WStatelessSlot.C
  Wt/WString.h                    Wt/WString.C
  Wt/WStringListModel.h           Wt/WStringListModel.C
  Wt/WStringStream.h              Wt/WStringStream.C
  Wt/WStringUtil.h                Wt/WStringUtil.C
  Wt/WSuggestionPopup.h           Wt/WSuggestionPopup.C
  Wt/WSvgImage.h                  Wt/WSvgImage.C
  Wt/WTabWidget.h                 Wt/WTabWidget.C
  Wt/WTable.h                     Wt/WTable.C
  Wt/WTableCell.h                 Wt/WTableCell.C
  Wt/WTableColumn.h               Wt/WTableColumn.C
  Wt/WTableRow.h                  Wt/WTableRow.C
  Wt/WTemplate.h                  Wt/WTemplate.C
  Wt/WTemplateFormView.h          Wt/WTemplateFormView.C
  Wt/WTableView.h                 Wt/WTableView.C
  Wt/WText.h                      Wt/WText.C
  Wt/WTextArea.h                  Wt/WTextArea.C
  Wt/WTextEdit.h                  Wt/WTextEdit.C
  Wt/WTheme.h                     Wt/WTheme.C
  Wt/WTime.h                      Wt/WTime.C
  Wt/WTimeEdit.h                  Wt/WTimeEdit.C
  Wt/WTimer.h                     Wt/WTimer.C
  Wt/WTimerWidget.h               Wt/WTimerWidget.C
  Wt/WTimePicker.h                Wt/WTimePicker.C
  Wt/WTimeValidator.h             Wt/WTimeValidator.C
  Wt/WToolBar.h                   Wt/WToolBar.C
  Wt/WTransform.h                 Wt/WTransform.C
  Wt/WTree.h                      Wt/WTree.C
  Wt/WTreeNode.h                  Wt/WTreeNode.C
  Wt/WTreeTable.h                 Wt/WTreeTable.C
  Wt/WTreeTableNode.h             Wt/WTreeTableNode.C
  Wt/WTreeView.h                  Wt/WTreeView.C
  Wt/WValidator.h                 Wt/WValidator.C
  Wt/WVBoxLayout.h                Wt/WVBoxLayout.C
  Wt/WVectorImage.h               Wt/WVectorImage.C
  Wt/WVideo.h                     Wt/WVideo.C
  Wt/WVmlImage.h                  Wt/WVmlImage.C
  Wt/WViewWidget.h                Wt/WViewWidget.C
  Wt/WVirtualImage.h              Wt/WVirtualImage.C

  # ─────────────────── layout / flexbox helpers ───────────────────────────
  Wt/FlexLayoutImpl.h            Wt/FlexLayoutImpl.C
  Wt/FlexItemImpl.h              Wt/FlexItemImpl.C
  Wt/StdGridLayoutImpl2.h        Wt/StdGridLayoutImpl2.C
  Wt/StdLayoutImpl.h             Wt/StdLayoutImpl.C
  Wt/StdLayoutItemImpl.h         Wt/StdLayoutItemImpl.C
  Wt/StdWidgetItemImpl.h         Wt/StdWidgetItemImpl.C

  # ─────────────────── charting (optional; drop block if unwanted) ────────
  Wt/Chart/WAbstractChart.h            Wt/Chart/WAbstractChart.C
  Wt/Chart/WAbstractChartModel.h       Wt/Chart/WAbstractChartModel.C
  Wt/Chart/WAxis.h                     Wt/Chart/WAxis.C
  Wt/Chart/WAxisSliderWidget.h         Wt/Chart/WAxisSliderWidget.C
  Wt/Chart/WDataSeries.h               Wt/Chart/WDataSeries.C
  Wt/Chart/WPieChart.h                 Wt/Chart/WPieChart.C
  Wt/Chart/WCartesianChart.h           Wt/Chart/WCartesianChart.C
  Wt/Chart/WCartesian3DChart.h         Wt/Chart/WCartesian3DChart.C
  Wt/Chart/WAbstractChartImplementation.h  Wt/Chart/WAbstractChartImplementation.C
  Wt/Chart/WChart2DImplementation.h        Wt/Chart/WChart2DImplementation.C
  Wt/Chart/WChart3DImplementation.h        Wt/Chart/WChart3DImplementation.C
  Wt/Chart/WChartPalette.h                 Wt/Chart/WChartPalette.C
  Wt/Chart/WStandardChartProxyModel.h      Wt/Chart/WStandardChartProxyModel.C
  Wt/Chart/WStandardPalette.h              Wt/Chart/WStandardPalette.C
  Wt/Chart/WStandardColorMap.h             Wt/Chart/WStandardColorMap.C
  Wt/Chart/WLegend.h                       Wt/Chart/WLegend.C
  Wt/Chart/WLegend3D.h                     Wt/Chart/WLegend3D.C
  Wt/Chart/WAbstractDataSeries3D.h         Wt/Chart/WAbstractDataSeries3D.C
  Wt/Chart/WAbstractGridData.h             Wt/Chart/WAbstractGridData.C
  Wt/Chart/WGridData.h                     Wt/Chart/WGridData.C
  Wt/Chart/WEquidistantGridData.h          Wt/Chart/WEquidistantGridData.C
  Wt/Chart/WScatterData.h                  Wt/Chart/WScatterData.C
  Wt/Chart/WSelection.h                    Wt/Chart/WSelection.C

  # ─────────────────── compile-time helpers / fmt / dates / json ──────────
  Wt/fmtlog.h                 Wt/fmtlog.cc
  Wt/Json/json.hpp
  Wt/Json/Parser.h            Wt/Json/Parser.C
  Wt/Json/Serializer.h        Wt/Json/Serializer.C
  Wt/cpp17/any.hpp
  Wt/cpp20/date.hpp           Wt/cpp20/tz.hpp
  Wt/cpp20/async_mutex.cpp    Wt/cpp20/async_mutex.h
  Wt/cpp23/MoveOnlyFunction.hpp
  Wt/Core/observable.hpp      Wt/Core/observable.cpp
  Wt/Core/observing_ptr.hpp   Wt/Core/observing_ptr.cpp

  Wt/Date/date.h Wt/Date/include/date/date.h
  Wt/Date/tz.h Wt/Date/include/date/tz.h Wt/Date/src/tz.cpp

  # ─────────────────── turbo-base64 + fasthex/md5/sha1 (client util) ──────
  Wt/turbo-base64/turbob64.h
  Wt/turbo-base64/turbob64c.c
  Wt/turbo-base64/turbob64d.c
  # Wt/turbo-base64/turbob64sse.c
  # Wt/turbo-base64/turbob64avx2.c      # will be compiled to WASM SIMD
  web/fasthex/hex.h                   web/fasthex/hex.cc
  web/md5.h                           web/md5.c
  web/sha1.h                          web/sha1.c

  # ─────────────────── your **web/** glue / DOM bridge ────────────────────
  web/DomElement.h        web/DomElement.C
  web/WebRenderer.h       web/WebRenderer.C
  web/WebUtils.h          web/WebUtils.C
  web/EntryPoint.h        web/EntryPoint.C
 # web/WebMain.h           web/WebMain.C
  web/WebSocketMessage.h  web/WebSocketMessage.C        # for browser ws
  web/WebStream.h         web/WebStream.C
  web/WebSession.h        web/WebSession.C
  web/WebRequest.h        web/WebRequest.C
  web/EscapeOStream.h     web/EscapeOStream.C
  web/ColorUtils.h        web/ColorUtils.C
  web/ImageUtils.h        web/ImageUtils.C
  web/RefEncoder.h        web/RefEncoder.C
  web/SoundManager.h      web/SoundManager.C
  web/FileUtils.h         web/FileUtils.C
  web/PdfUtils.h          web/PdfUtils.C   # can strip if not embedding PDFs
  web/StringUtils.h       web/StringUtils.cpp
  web/TimeUtil.h          web/TimeUtil.C
  web/XSSFilter.h         web/XSSFilter.C
  web/XSSUtils.h          web/XSSUtils.C
  web/SslUtils.h          web/SslUtils.C   # client-side crypto utils only
  web/UriUtils.h          web/UriUtils.C
  web/base64.h            web/base64.cpp
)
list(TRANSFORM WASM_REL_SOURCES
     PREPEND "${SRC_ROOT}/"
     OUTPUT_VARIABLE WASM_SOURCES)

SET(WT_ASIO_IS_BOOST_ASIO ON)
ADD_DEFINITIONS(-DWT_BUILDING)
ADD_DEFINITIONS(-DWT_WASM)
ADD_DEFINITIONS(-DHAS_REMOTE_API=0)  # nocurl API in wasm client


 FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Wt)
 SET(WCONFIG_H_PATH ${CMAKE_CURRENT_BINARY_DIR}/Wt/WConfig.h)
 CONFIGURE_FILE(
   ../WConfig.h.in
   ${WCONFIG_H_PATH}
 )
 INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# 4) build the wasm client
add_library(WtWasm STATIC  ${WASM_SOURCES})

target_include_directories(WtWasm PUBLIC
  ${SRC_ROOT}                   # for Wt/…
  ${SRC_ROOT}/
  ${SRC_ROOT}/Wt               # for Wt/…
  ${SRC_ROOT}/Wt/Date/include
  ${CMAKE_CURRENT_BINARY_DIR}
  ${SRC_ROOT}/web               # for web/…
  ${BOOST_INCLUDE_DIRS}
)

if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/Wt/WConfig.h")
  message(FATAL_ERROR "Path is wrong – ${CMAKE_CURRENT_BINARY_DIR}/Wt/WConfig.h not found")
endif()

# 1) flags that Clang understands (compile phase)
# target_compile_options(WtWasm PRIVATE
#     -O3
#     -std=c++23
#     $<$<CXX_COMPILER_ID:Clang>:-Wno-deprecated-literal-operator>
#     )           # this one is accepted at compile too
set_target_properties(WtWasm PROPERTIES
  C_STANDARD            23
  C_STANDARD_REQUIRED   YES
  C_EXTENSIONS          NO
  CXX_STANDARD          23
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS        NO
)

# -----------------  compile flags  -----------------
target_compile_options(WtWasm PRIVATE
  -O3
  -flto=full                 # LTO bitcode generation
  -fconstexpr-steps=20000000 # raise constexpr evaluator limit
)

# -----------------  link flags  --------------------
target_link_options(WtWasm PRIVATE
  -flto=full                 # LTO link-time optimisation pass
  -fuse-ld=lld               # use the LLVM lld linker instead of ld.gold/ld.bfd
)
set_property(TARGET WtWasm PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)


# 2) flags that only em++ (link phase) understands
target_link_options(WtWasm PRIVATE
    -sWASM=1
    -sALLOW_MEMORY_GROWTH=1
    -sENVIRONMENT=web
    -sMODULARIZE=1
    -sEXPORT_NAME=WtWasm
    -sUSE_BOOST_HEADERS=1
    -sFETCH=1
    --bind)
add_compile_definitions(
  BOOST_HAS
  BOOST_ASIO_DISABLE_THREADS
  BOOST_ASIO_DISABLE_SIGNAL_HANDLER
)
# target_compile_options(WtWasm PRIVATE
#     -O3
#     -sWASM=1
#     -sALLOW_MEMORY_GROWTH=1)
# emit a HTML+JS loader
# set_target_properties(WtWasm PROPERTIES
#   LINK_FLAGS "-o ${CMAKE_CURRENT_BINARY_DIR}/wasm_client.html"
# )
message(STATUS "→ compiling WebAssembly client with Emscripten")
message(STATUS "→ wasm-client will install wasm_client.html/.js/.wasm")

install(TARGETS WtWasm
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)   # for .wasm side-module

