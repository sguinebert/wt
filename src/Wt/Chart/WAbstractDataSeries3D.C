/*
 * Copyright (C) 2013 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/Chart/WAbstractDataSeries3D.h"

#include "Wt/WAbstractItemModel.h"
#include "Wt/WApplication.h"
#include "Wt/WCanvasPaintDevice.h"
#include "Wt/WEnvironment.h"
#include "Wt/WPainter.h"
#include "Wt/Chart/WAbstractColorMap.h"
#include "Wt/Chart/WCartesian3DChart.h"

using namespace Wt;

//namespace {
//  void clearConnections(std::vector<Wt::Signals::connection> connections) {
//    for (unsigned i=0; i < connections.size(); i++)
//      connections[i].disconnect();
//    connections.clear();
//  }
//}

namespace Wt {
  namespace Chart {

WAbstractDataSeries3D::WAbstractDataSeries3D(std::shared_ptr<WAbstractItemModel> model)
  : chart_(0),
    rangeCached_(false),
    pointSize_(2.0),
    colormap_(0),
    showColorMap_(false),
    colorMapSide_(Side::Right),
    legendEnabled_(true),
    hidden_(false),
    /* in webGL, the z-direction is out of the screen, in model coordinates
     * it's the vertical direction */
    mvMatrix_(1.0f, 0.0f, 0.0f, 0.0f,
	      0.0f, 0.0f, 1.0f, 0.0f,
	      0.0f, 1.0f, 0.0f, 0.0f,
	      0.0f, 0.0f, 0.0f, 1.0f)
{
  model_ = model;
}

WAbstractDataSeries3D::~WAbstractDataSeries3D()
{
}

void WAbstractDataSeries3D
::setModel(const std::shared_ptr<WAbstractItemModel>& model)
{
  //co_await model->loadAllInCache();
  if (model != model_) {
    // handle previous model
    if (model_ && chart_) {
      //clearConnections(connections_);
        model_->modelReset().disconnect<&WCartesian3DChart::onModelReset>(chart_);
        model_->dataChanged().disconnect<&WCartesian3DChart::onDataChanged>(chart_);
        model_->rowsInserted().disconnect<&WCartesian3DChart::onSourceModelModified>(chart_);
        model_->columnsInserted().disconnect<&WCartesian3DChart::onSourceModelModified>(chart_);
        model_->rowsRemoved().disconnect<&WCartesian3DChart::onSourceModelModified>(chart_);
        model_->columnsRemoved().disconnect<&WCartesian3DChart::onSourceModelModified>(chart_);
    }
    rangeCached_ = false;

    // set new model
    model_ = model;

    if (model_ && chart_) {
      chart_->updateChart(ChartUpdates::GLContext);
      model_->modelReset().connect<&WCartesian3DChart::onModelReset>(chart_);
      model_->dataChanged().connect<&WCartesian3DChart::onDataChanged>(chart_);
      model_->rowsInserted().connect<&WCartesian3DChart::onSourceModelModified>(chart_);
      model_->columnsInserted().connect<&WCartesian3DChart::onSourceModelModified>(chart_);
      model_->rowsRemoved().connect<&WCartesian3DChart::onSourceModelModified>(chart_);
      model_->columnsRemoved().connect<&WCartesian3DChart::onSourceModelModified>(chart_);
    }
  }
}

void WAbstractDataSeries3D::setTitle(const WString& name)
{
  name_ = name;
  if (chart_)
    chart_->updateChart(ChartUpdates::GLTextures);
}

void WAbstractDataSeries3D::setPointSize(double size)
{
  if (size != pointSize_) {
    pointSize_ = size;
    if (chart_)
      chart_->updateChart(ChartUpdates::GLContext | ChartUpdates::GLTextures);
  }
}

void WAbstractDataSeries3D::setPointSprite(const std::string &image)
{
  if (image != pointSprite_) {
    pointSprite_ = image;
    if (chart_)
      chart_->updateChart(ChartUpdates::GLContext | ChartUpdates::GLTextures);
  }
}

void WAbstractDataSeries3D
::setColorMap(const std::shared_ptr<WAbstractColorMap>& colormap)
{
  colormap_ = colormap;
  if (chart_)
    chart_->updateChart(ChartUpdates::GLContext | ChartUpdates::GLTextures);
}

void WAbstractDataSeries3D::setColorMapVisible(bool enabled)
{
  if (showColorMap_ == enabled)
    return;

  showColorMap_ = enabled;
  if (chart_)
    chart_->updateChart(ChartUpdates::GLTextures);
}

void WAbstractDataSeries3D::setColorMapSide(Side side)
{
  if (colorMapSide_ == side)
    return;

  colorMapSide_ = side;
  if (chart_)
    chart_->updateChart(ChartUpdates::GLTextures);
}

void WAbstractDataSeries3D::setHidden(bool enabled)
{
  if (enabled != hidden_) {
    hidden_ = enabled;
    if (chart_)
      chart_->updateChart(ChartUpdates::GLContext | ChartUpdates::GLTextures);
  }
}

WGLWidget::Texture WAbstractDataSeries3D::colorTexture()
{
  std::unique_ptr<WPaintDevice> cpd;
  if (!colormap_) {
    cpd = chart_->createPaintDevice(WLength(1),WLength(1));
    WColor seriesColor = chartpaletteColor();
    WPainter painter(cpd.get());
    painter.setPen(WPen(seriesColor));
    painter.drawLine(0,0.5,1,0.5);
    painter.end();
  } else {
    cpd = chart_->createPaintDevice(WLength(1),WLength(1024));
    WPainter painter(cpd.get());
    colormap_->createStrip(&painter);
    painter.end();
  }

  WGLWidget::Texture tex = chart_->createTexture();
  chart_->bindTexture(WGLWidget::TEXTURE_2D, tex);
  chart_->pixelStorei(WGLWidget::UNPACK_FLIP_Y_WEBGL, 1);
  chart_->texImage2D(WGLWidget::TEXTURE_2D, 0,
		     WGLWidget::RGBA, WGLWidget::RGBA,
		     WGLWidget::UNSIGNED_BYTE, cpd.get());

  return tex;
}

WGLWidget::Texture WAbstractDataSeries3D::pointSpriteTexture()
{
  WGLWidget::Texture tex = chart_->createTexture();
  chart_->bindTexture(WGLWidget::TEXTURE_2D, tex);
  if (pointSprite_.empty()) {
    std::unique_ptr<WPaintDevice> cpd
      = chart_->createPaintDevice(WLength(1), WLength(1));
    WColor color = WColor(255, 255, 255, 255);
    WPainter painter(cpd.get());
    painter.setPen(WPen(color));
    painter.drawLine(0,0.5,1,0.5);
    painter.end();
    chart_->texImage2D(WGLWidget::TEXTURE_2D, 0,
		       WGLWidget::RGBA, WGLWidget::RGBA,
		       WGLWidget::UNSIGNED_BYTE, cpd.get());
  }

  return tex;
}

void WAbstractDataSeries3D
::loadPointSpriteTexture(const WGLWidget::Texture &tex) const
{
  chart_->bindTexture(WGLWidget::TEXTURE_2D, tex);
  if (!pointSprite_.empty()) {
    chart_->texImage2D(WGLWidget::TEXTURE_2D, 0, WGLWidget::RGBA,
		       WGLWidget::RGBA, WGLWidget::UNSIGNED_BYTE, pointSprite_);
  }
}

void WAbstractDataSeries3D::setDefaultTitle(int i)
{
  std::string tmp = std::string("dataset ");
  tmp.append(std::to_string(i));
  name_ = WString(tmp);
}

WColor WAbstractDataSeries3D::chartpaletteColor() const
{
  if (colormap_)
    return WColor();

  int index = 0;
  for (unsigned i=0; i < chart_->dataSeries().size(); i++) { // which colorscheme
    if (chart_->dataSeries()[i] == this) {
      break;
    } else if (chart_->dataSeries()[i]->colorMap() == 0) {
      index++;
    }
  }
  return chart_->palette()->brush(index).color();
}

void WAbstractDataSeries3D::setChart(WCartesian3DChart *chart)
{
  if (chart == chart_)
    return;
  else if (chart_)
    chart_->removeDataSeries(this);
//  if (chart_ && model_)
//    clearConnections(connections_);
  model_->modelReset().disconnect<&WCartesian3DChart::onModelReset>(chart_);
  model_->dataChanged().disconnect<&WCartesian3DChart::onDataChanged>(chart_);
  model_->rowsInserted().disconnect<&WCartesian3DChart::onSourceModelModified>(chart_);
  model_->columnsInserted().disconnect<&WCartesian3DChart::onSourceModelModified>(chart_);
  model_->rowsRemoved().disconnect<&WCartesian3DChart::onSourceModelModified>(chart_);
  model_->columnsRemoved().disconnect<&WCartesian3DChart::onSourceModelModified>(chart_);

  chart_ = chart;

  if (chart_ && model_) {
    model_->modelReset().connect<&WCartesian3DChart::onModelReset>(chart_);
    model_->dataChanged().connect<&WCartesian3DChart::onDataChanged>(chart_);
    model_->rowsInserted().connect<&WCartesian3DChart::onSourceModelModified>(chart_);
    model_->columnsInserted().connect<&WCartesian3DChart::onSourceModelModified>(chart_);
    model_->rowsRemoved().connect<&WCartesian3DChart::onSourceModelModified>(chart_);
    model_->columnsRemoved().connect<&WCartesian3DChart::onSourceModelModified>(chart_);
  }
}

std::vector<cpp17::any> WAbstractDataSeries3D::getGlObjects()
{
  return std::vector<cpp17::any>();
}

  }
}
