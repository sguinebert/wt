# ---------------------------------------------------------------------------
# wt-widgets — All W*Widget classes, Auth, Chart, Form, Payment, themes,
#               layouts, and every other UI component not in wt-core.
#
# Links PUBLIC wt-core so consumers get the full dependency chain:
#   wt-widgets -> wt-core -> whttp
#
# A backward-compatible INTERFACE alias "wt" is provided at the bottom so
# existing targets that do  target_link_libraries(myapp PRIVATE wt)  keep
# working without changes.
# ---------------------------------------------------------------------------

set(WT_WIDGETS_SOURCES
  # -- Layout internals --
  Wt/FlexLayoutImpl.h Wt/FlexLayoutImpl.C
  Wt/FlexItemImpl.h Wt/FlexItemImpl.C
  Wt/StdGridLayoutImpl2.h Wt/StdGridLayoutImpl2.C
  Wt/StdLayoutImpl.h Wt/StdLayoutImpl.C
  Wt/StdLayoutItemImpl.h Wt/StdLayoutItemImpl.C
  Wt/StdWidgetItemImpl.h Wt/StdWidgetItemImpl.C

  # -- UI helpers --
  Wt/PopupWindow.h Wt/PopupWindow.C
  Wt/Resizable.h Wt/Resizable.C
  Wt/ResizeSensor.h Wt/ResizeSensor.C
  Wt/ServerSideFontMetrics.h Wt/ServerSideFontMetrics.C
  Wt/SizeHandle.h Wt/SizeHandle.C

  # -- Abstract widgets / areas --
  Wt/WAbstractArea.h Wt/WAbstractArea.C
  Wt/WAbstractItemDelegate.h Wt/WAbstractItemDelegate.C
  Wt/WAbstractItemModel.h Wt/WAbstractItemModel.C
  Wt/WAbstractItemView.h Wt/WAbstractItemView.C
  Wt/WAbstractListModel.h Wt/WAbstractListModel.C
  Wt/WAbstractMedia.h Wt/WAbstractMedia.C
  Wt/WAbstractProxyModel.h Wt/WAbstractProxyModel.C
  Wt/WAbstractSpinBox.h Wt/WAbstractSpinBox.C
  Wt/WAbstractTableModel.h Wt/WAbstractTableModel.C
  Wt/WAbstractToggleButton.h Wt/WAbstractToggleButton.C

  # -- Proxy models --
  Wt/WAggregateProxyModel.h Wt/WAggregateProxyModel.C
  Wt/WBatchEditProxyModel.h Wt/WBatchEditProxyModel.C
  Wt/WIdentityProxyModel.h Wt/WIdentityProxyModel.C
  Wt/WReadOnlyProxyModel.h Wt/WReadOnlyProxyModel.C
  Wt/WSortFilterProxyModel.h Wt/WSortFilterProxyModel.C

  # -- Concrete widgets (alphabetical) --
  Wt/WAnchor.h Wt/WAnchor.C
  Wt/WAudio.h Wt/WAudio.C
  Wt/WBootstrapTheme.h Wt/WBootstrapTheme.C
  Wt/WBootstrap2Theme.h Wt/WBootstrap2Theme.C
  Wt/WBootstrap3Theme.h Wt/WBootstrap3Theme.C
  Wt/WBootstrap5Theme.h Wt/WBootstrap5Theme.C
  Wt/WBorderLayout.h Wt/WBorderLayout.C
  Wt/WBoxLayout.h Wt/WBoxLayout.C
  Wt/WBreak.h Wt/WBreak.C
  Wt/WButtonGroup.h Wt/WButtonGroup.C
  Wt/WCalendar.h Wt/WCalendar.C
  Wt/WCanvasPaintDevice.h Wt/WCanvasPaintDevice.C
  Wt/WCheckBox.h Wt/WCheckBox.C
  Wt/WCircleArea.h Wt/WCircleArea.C
  Wt/WColorPicker.h Wt/WColorPicker.C
  Wt/WComboBox.h Wt/WComboBox.C
  Wt/WCompositeWidget.h Wt/WCompositeWidget.C
  Wt/WContainerWidget.h Wt/WContainerWidget.C
  Wt/WCssTheme.h Wt/WCssTheme.C
  Wt/WDateEdit.h Wt/WDateEdit.C
  Wt/WDatePicker.h Wt/WDatePicker.C
  Wt/WDateValidator.h Wt/WDateValidator.C
  Wt/WDefaultLoadingIndicator.h Wt/WDefaultLoadingIndicator.C
  Wt/WDialog.h Wt/WDialog.C
  Wt/WDoubleSpinBox.h Wt/WDoubleSpinBox.C
  Wt/WDoubleValidator.h Wt/WDoubleValidator.C
  Wt/WDropZone.h Wt/WDropZone.C
  Wt/WFileDropWidget.h Wt/WFileDropWidget.C
  Wt/WFileUpload.h Wt/WFileUpload.C
  Wt/WFitLayout.h Wt/WFitLayout.C
  Wt/WFlashObject.h Wt/WFlashObject.C
  Wt/WFormWidget.h Wt/WFormWidget.C
  Wt/WGLWidget.h Wt/WGLWidget.C
  Wt/WAbstractGLImplementation.h Wt/WAbstractGLImplementation.C
  Wt/WClientGLWidget.h Wt/WClientGLWidget.C
  Wt/WGoogleMap.h Wt/WGoogleMap.C
  Wt/WGridLayout.h Wt/WGridLayout.C
  Wt/WGroupBox.h Wt/WGroupBox.C
  Wt/WHBoxLayout.h Wt/WHBoxLayout.C
  Wt/WIcon.h Wt/WIcon.C
  Wt/WIconPair.h Wt/WIconPair.C
  Wt/WImage.h Wt/WImage.C
  Wt/WInPlaceEdit.h Wt/WInPlaceEdit.C
  Wt/WIntValidator.h Wt/WIntValidator.C
  Wt/WInteractWidget.h Wt/WInteractWidget.C
  Wt/WItemDelegate.h Wt/WItemDelegate.C
  Wt/WItemSelectionModel.h Wt/WItemSelectionModel.C
  Wt/WLabel.h Wt/WLabel.C
  Wt/WLayout.h Wt/WLayout.C
  Wt/WLayoutImpl.h Wt/WLayoutImpl.C
  Wt/WLayoutItem.h Wt/WLayoutItem.C
  Wt/WLayoutItemImpl.h Wt/WLayoutItemImpl.C
  Wt/WLeafletMap.h Wt/WLeafletMap.C
  Wt/WLengthValidator.h Wt/WLengthValidator.C
  Wt/WLineEdit.h Wt/WLineEdit.C
  Wt/WLoadingIndicator.h Wt/WLoadingIndicator.C
  Wt/WMeasurePaintDevice.h Wt/WMeasurePaintDevice.C
  Wt/WMediaPlayer.h Wt/WMediaPlayer.C
  Wt/WMenu.h Wt/WMenu.C
  Wt/WMenuItem.h Wt/WMenuItem.C
  Wt/WMessageBox.h Wt/WMessageBox.C
  Wt/WNavigationBar.h Wt/WNavigationBar.C
  Wt/WOverlayLoadingIndicator.h Wt/WOverlayLoadingIndicator.C
  Wt/WPaintedWidget.h Wt/WPaintedWidget.C
  Wt/WPanel.h Wt/WPanel.C
  Wt/WPolygonArea.h Wt/WPolygonArea.C
  Wt/WPopupMenu.h Wt/WPopupMenu.C
  Wt/WPopupWidget.h Wt/WPopupWidget.C
  Wt/WProgressBar.h Wt/WProgressBar.C
  Wt/WPushButton.h Wt/WPushButton.C
  Wt/WRadioButton.h Wt/WRadioButton.C
  Wt/WRectArea.h Wt/WRectArea.C
  Wt/WRegExpValidator.h Wt/WRegExpValidator.C
  Wt/WSelectionBox.h Wt/WSelectionBox.C
  Wt/WSlider.h Wt/WSlider.C
  Wt/WSound.h Wt/WSound.C
  Wt/WSpinBox.h Wt/WSpinBox.C
  Wt/WSplitButton.h Wt/WSplitButton.C
  Wt/WStackedWidget.h Wt/WStackedWidget.C
  Wt/WStandardItem.h Wt/WStandardItem.C
  Wt/WStandardItemModel.h Wt/WStandardItemModel.C
  Wt/WStringListModel.h Wt/WStringListModel.C
  Wt/WSuggestionPopup.h Wt/WSuggestionPopup.C
  Wt/WSvgImage.h Wt/WSvgImage.C
  Wt/WTabWidget.h Wt/WTabWidget.C
  Wt/WTable.h Wt/WTable.C
  Wt/WTableCell.h Wt/WTableCell.C
  Wt/WTableColumn.h Wt/WTableColumn.C
  Wt/WTableRow.h Wt/WTableRow.C
  Wt/WTemplate.h Wt/WTemplate.C
  Wt/WTemplateFormView.h Wt/WTemplateFormView.C
  Wt/WTableView.h Wt/WTableView.C
  Wt/WText.h Wt/WText.C
  Wt/WTextArea.h Wt/WTextArea.C
  Wt/WTextEdit.h Wt/WTextEdit.C
  Wt/WTheme.h Wt/WTheme.C
  Wt/WTimeEdit.h Wt/WTimeEdit.C
  Wt/WTimer.h Wt/WTimer.C
  Wt/WTimerWidget.h Wt/WTimerWidget.C
  Wt/WTimePicker.h Wt/WTimePicker.C
  Wt/WTimeValidator.h Wt/WTimeValidator.C
  Wt/WToolBar.h Wt/WToolBar.C
  Wt/WTree.h Wt/WTree.C
  Wt/WTreeNode.h Wt/WTreeNode.C
  Wt/WTreeTable.h Wt/WTreeTable.C
  Wt/WTreeTableNode.h Wt/WTreeTableNode.C
  Wt/WTreeView.h Wt/WTreeView.C
  Wt/WVBoxLayout.h Wt/WVBoxLayout.C
  Wt/WVectorImage.h Wt/WVectorImage.C
  Wt/WVideo.h Wt/WVideo.C
  Wt/WVmlImage.h Wt/WVmlImage.C
  Wt/WViewWidget.h Wt/WViewWidget.C
  Wt/WVirtualImage.h Wt/WVirtualImage.C
  Wt/WWebassembly.h Wt/WWebassembly.C
  Wt/WWebWidget.h Wt/WWebWidget.C
  Wt/WWidget.h Wt/WWidget.C
  Wt/WWidgetItem.h Wt/WWidgetItem.C
  Wt/WWidgetItemImpl.h Wt/WWidgetItemImpl.C

  # -- Auth --
  Wt/Auth/AbstractPasswordService.h Wt/Auth/AbstractPasswordService.C
  Wt/Auth/AbstractUserDatabase.h Wt/Auth/AbstractUserDatabase.C
  Wt/Auth/AuthModel.h Wt/Auth/AuthModel.C
  Wt/Auth/AuthService.h Wt/Auth/AuthService.C
  Wt/Auth/AuthWidget.h Wt/Auth/AuthWidget.C
  Wt/Auth/FacebookService.h Wt/Auth/FacebookService.C
  Wt/Auth/FormBaseModel.h Wt/Auth/FormBaseModel.C
  Wt/Auth/GoogleService.h Wt/Auth/GoogleService.C
  Wt/Auth/HashFunction.h Wt/Auth/HashFunction.C
  Wt/Auth/Identity.h Wt/Auth/Identity.C
  Wt/Auth/Login.h Wt/Auth/Login.C
  Wt/Auth/LostPasswordWidget.h Wt/Auth/LostPasswordWidget.C
  Wt/Auth/OAuthService.h Wt/Auth/OAuthService.C
  Wt/Auth/OAuthWidget.h Wt/Auth/OAuthWidget.C
  Wt/Auth/passwdqc.h Wt/Auth/passwdqc_check.c
  Wt/Auth/PasswordHash.h Wt/Auth/PasswordHash.C
  Wt/Auth/PasswordPromptDialog.h Wt/Auth/PasswordPromptDialog.C
  Wt/Auth/PasswordService.h Wt/Auth/PasswordService.C
  Wt/Auth/PasswordStrengthValidator.h Wt/Auth/PasswordStrengthValidator.C
  Wt/Auth/PasswordVerifier.h Wt/Auth/PasswordVerifier.C
  Wt/Auth/RegistrationModel.h Wt/Auth/RegistrationModel.C
  Wt/Auth/RegistrationWidget.h Wt/Auth/RegistrationWidget.C
  Wt/Auth/ResendEmailVerificationWidget.h Wt/Auth/ResendEmailVerificationWidget.C
  Wt/Auth/Token.h Wt/Auth/Token.C
  Wt/Auth/UpdatePasswordWidget.h Wt/Auth/UpdatePasswordWidget.C
  Wt/Auth/User.h Wt/Auth/User.C
  Wt/Auth/AuthUtils.h Wt/Auth/AuthUtils.C
  Wt/Auth/MailUtils.h Wt/Auth/MailUtils.C
  Wt/Auth/OAuthClient.h Wt/Auth/OAuthClient.C
  Wt/Auth/OAuthTokenEndpoint.h Wt/Auth/OAuthTokenEndpoint.C
  Wt/Auth/OidcUserInfoEndpoint.h Wt/Auth/OidcUserInfoEndpoint.C
  Wt/Auth/OAuthAuthorizationEndpointProcess.h Wt/Auth/OAuthAuthorizationEndpointProcess.C
  Wt/Auth/IssuedToken.h Wt/Auth/IssuedToken.C
  Wt/Auth/OidcService.h Wt/Auth/OidcService.C

  # -- Chart --
  Wt/Chart/WAbstractChart.h Wt/Chart/WAbstractChart.C
  Wt/Chart/WAbstractChartModel.h Wt/Chart/WAbstractChartModel.C
  Wt/Chart/WAxis.h Wt/Chart/WAxis.C
  Wt/Chart/WAxisSliderWidget.h Wt/Chart/WAxisSliderWidget.C
  Wt/Chart/WDataSeries.h Wt/Chart/WDataSeries.C
  Wt/Chart/WPieChart.h Wt/Chart/WPieChart.C
  Wt/Chart/WCartesianChart.h Wt/Chart/WCartesianChart.C
  Wt/Chart/WCartesian3DChart.h Wt/Chart/WCartesian3DChart.C
  Wt/Chart/WAbstractChartImplementation.h Wt/Chart/WAbstractChartImplementation.C
  Wt/Chart/WChart2DImplementation.h Wt/Chart/WChart2DImplementation.C
  Wt/Chart/WChart3DImplementation.h Wt/Chart/WChart3DImplementation.C
  Wt/Chart/WChartPalette.h Wt/Chart/WChartPalette.C
  Wt/Chart/WStandardChartProxyModel.h Wt/Chart/WStandardChartProxyModel.C
  Wt/Chart/WStandardPalette.h Wt/Chart/WStandardPalette.C
  Wt/Chart/WStandardColorMap.h Wt/Chart/WStandardColorMap.C
  Wt/Chart/WLegend.h Wt/Chart/WLegend.C
  Wt/Chart/WLegend3D.h Wt/Chart/WLegend3D.C
  Wt/Chart/WAbstractDataSeries3D.h Wt/Chart/WAbstractDataSeries3D.C
  Wt/Chart/WAbstractGridData.h Wt/Chart/WAbstractGridData.C
  Wt/Chart/WGridData.h Wt/Chart/WGridData.C
  Wt/Chart/WEquidistantGridData.h Wt/Chart/WEquidistantGridData.C
  Wt/Chart/WScatterData.h Wt/Chart/WScatterData.C
  Wt/Chart/WSelection.h Wt/Chart/WSelection.C

  # -- Form delegates --
  Wt/Form/WAbstractFormDelegate.h Wt/Form/WAbstractFormDelegate.C
  Wt/Form/WFormDelegate.h Wt/Form/WFormDelegate.C

  # -- Payment --
  Wt/Payment/Address.h Wt/Payment/Address.C
  Wt/Payment/PayPal.h Wt/Payment/PayPal.C
  Wt/Payment/Customer.h Wt/Payment/Customer.C
  Wt/Payment/Money.h Wt/Payment/Money.C
  Wt/Payment/Order.h Wt/Payment/Order.C
  Wt/Payment/OrderItem.h Wt/Payment/OrderItem.C
  Wt/Payment/Result.h Wt/Payment/Result.C
)

# ---------------------------------------------------------------------------
# Conditional widget sources
# ---------------------------------------------------------------------------

# -- PDF (Haru) --
IF(HAVE_HARU)
  list(APPEND WT_WIDGETS_SOURCES
    Wt/WPdfImage.h Wt/WPdfImage.C
    Wt/Render/WPdfRenderer.h Wt/Render/WPdfRenderer.C)
ENDIF()

# -- Raster image --
IF("${WT_WRASTERIMAGE_IMPLEMENTATION}" STREQUAL "GraphicsMagick")
  list(APPEND WT_WIDGETS_SOURCES Wt/WRasterImage.h Wt/WRasterImage-gm.C)
ELSEIF("${WT_WRASTERIMAGE_IMPLEMENTATION}" STREQUAL "Direct2D")
  list(APPEND WT_WIDGETS_SOURCES Wt/WRasterImage.h Wt/WRasterImage-d2d1.C)
ENDIF()

# -- OpenGL (server-side) --
if(WT_USE_OPENGL)
  list(APPEND WT_WIDGETS_SOURCES Wt/WServerGLWidget.h Wt/WServerGLWidget.C)
endif()
IF(HAVE_GL)
  # Guard against double-adding when WT_USE_OPENGL is also set
  list(FIND WT_WIDGETS_SOURCES "Wt/WServerGLWidget.h" _gl_idx)
  if(_gl_idx EQUAL -1)
    list(APPEND WT_WIDGETS_SOURCES Wt/WServerGLWidget.h Wt/WServerGLWidget.C)
  endif()
  IF(NOT USE_SYSTEM_GLEW)
    list(APPEND WT_WIDGETS_SOURCES
      3rdparty/glew-1.10.0/include/GL/glew.h
      3rdparty/glew-1.10.0/include/GL/glxew.h
      3rdparty/glew-1.10.0/include/GL/wglew.h
      3rdparty/glew-1.10.0/src/glew.c)
  ENDIF()
ENDIF()

# -- Font support (Pango / DirectWrite / Simple) --
SET(WT_FONTSUPPORT_SIMPLE false)
SET(WT_FONTSUPPORT_PANGO false)
SET(WT_FONTSUPPORT_DIRECTWRITE false)
SET(HAVE_DIRECTWRITE false)
IF(WIN32)
  SET(HAVE_DIRECTWRITE true)
ENDIF()

IF(HAVE_HARU OR "${WT_WRASTERIMAGE_IMPLEMENTATION}" STREQUAL "GraphicsMagick" OR "${WT_WRASTERIMAGE_IMPLEMENTATION}" STREQUAL "Direct2D")
  IF(HAVE_PANGO AND NOT "${WT_WRASTERIMAGE_IMPLEMENTATION}" STREQUAL "Direct2D")
    list(APPEND WT_WIDGETS_SOURCES Wt/FontSupport.h Wt/FontSupportPango.C)
    SET(WT_FONTSUPPORT_PANGO true)
  ELSEIF(HAVE_DIRECTWRITE)
    list(APPEND WT_WIDGETS_SOURCES Wt/FontSupport.h Wt/FontSupportDirectWrite.C)
    SET(WT_FONTSUPPORT_DIRECTWRITE true)
  ELSE()
    list(APPEND WT_WIDGETS_SOURCES Wt/FontSupport.h Wt/FontSupportSimple.C)
    SET(WT_FONTSUPPORT_SIMPLE true)
  ENDIF()
ENDIF()

# -- Windows version resource --
IF(WIN32 AND SHARED_LIBS)
  CONFIGURE_FILE(Wt/wt-version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/wt-widgets-version.rc)
  list(APPEND WT_WIDGETS_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/wt-widgets-version.rc)
ENDIF()

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
ADD_LIBRARY(wt-widgets ${WT_WIDGETS_SOURCES})

target_compile_definitions(wt-widgets PRIVATE wt_EXPORTS)

SET_PROPERTY(TARGET wt-widgets PROPERTY C_VISIBILITY_PRESET hidden)
SET_PROPERTY(TARGET wt-widgets PROPERTY CXX_VISIBILITY_PRESET hidden)
SET_PROPERTY(TARGET wt-widgets PROPERTY VISIBILITY_INLINES_HIDDEN YES)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
TARGET_INCLUDE_DIRECTORIES(wt-widgets
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/web>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------

# wt-core provides the entire core + whttp chain
TARGET_LINK_LIBRARIES(wt-widgets PUBLIC wt-core)

# -- Haru (PDF) --
IF(HAVE_HARU)
  TARGET_LINK_LIBRARIES(wt-widgets PRIVATE ${HARU_LIBRARIES})
  TARGET_INCLUDE_DIRECTORIES(wt-widgets PRIVATE ${HARU_INCLUDE_DIRS})
ENDIF()

# -- GraphicsMagick / Direct2D (raster) --
IF("${WT_WRASTERIMAGE_IMPLEMENTATION}" STREQUAL "GraphicsMagick")
  TARGET_LINK_LIBRARIES(wt-widgets PRIVATE ${GM_LIBRARIES})
  TARGET_INCLUDE_DIRECTORIES(wt-widgets PRIVATE ${GM_INCLUDE_DIRS})
ELSEIF("${WT_WRASTERIMAGE_IMPLEMENTATION}" STREQUAL "Direct2D")
  TARGET_LINK_LIBRARIES(wt-widgets PRIVATE d2d1 dwrite windowscodecs shlwapi)
ENDIF()

# -- OpenGL / GLEW --
if(WT_USE_OPENGL)
  target_link_libraries(wt-widgets PRIVATE OpenGL::GL GLEW::GLEW)
endif()

# -- Font support link libraries --
IF(WT_FONTSUPPORT_SIMPLE)
  ADD_DEFINITIONS(-DWT_FONTSUPPORT_SIMPLE)
ELSEIF(WT_FONTSUPPORT_PANGO)
  TARGET_LINK_LIBRARIES(wt-widgets PRIVATE ${PANGO_FT2_LIBRARIES})
  TARGET_INCLUDE_DIRECTORIES(wt-widgets PRIVATE ${PANGO_FT2_INCLUDE_DIRS})
  ADD_DEFINITIONS(-DWT_FONTSUPPORT_PANGO)
ELSEIF(WT_FONTSUPPORT_DIRECTWRITE)
  TARGET_LINK_LIBRARIES(wt-widgets PRIVATE dwrite)
  ADD_DEFINITIONS(-DWT_FONTSUPPORT_DIRECTWRITE)
ENDIF()

# -- MSVC specifics --
IF(MSVC)
  SET_TARGET_PROPERTIES(wt-widgets PROPERTIES
    COMPILE_FLAGS "${BUILD_PARALLEL} /wd4251 /wd4275 /wd4355 /wd4800 /wd4996 /wd4101 /wd4267")
ENDIF()

# ---------------------------------------------------------------------------
# Version / install
# ---------------------------------------------------------------------------
SET_TARGET_PROPERTIES(wt-widgets
  PROPERTIES
    EXPORT_NAME WtWidgets
    VERSION ${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}
    DEBUG_POSTFIX ${DEBUG_LIB_POSTFIX}
)

INSTALL(TARGETS wt-widgets
    EXPORT wt-target-wt-widgets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

INSTALL(EXPORT wt-target-wt-widgets
        DESTINATION ${CMAKE_INSTALL_DIR}/wt
        NAMESPACE Wt::)

# ---------------------------------------------------------------------------
# Backward-compatible INTERFACE alias
# ---------------------------------------------------------------------------
# Existing code that links "wt" now transparently gets both wt-widgets and
# wt-core (and whttp) through the PUBLIC dependency chain.
# ---------------------------------------------------------------------------
ADD_LIBRARY(wt INTERFACE)
TARGET_LINK_LIBRARIES(wt INTERFACE wt-widgets)

SET_TARGET_PROPERTIES(wt
  PROPERTIES
    EXPORT_NAME Wt
)

INSTALL(TARGETS wt
    EXPORT wt-target-wt
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

INSTALL(EXPORT wt-target-wt
        DESTINATION ${CMAKE_INSTALL_DIR}/wt
        NAMESPACE Wt::)
