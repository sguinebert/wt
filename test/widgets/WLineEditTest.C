/*
 * Copyright (C) 2026 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#include <boost/test/unit_test.hpp>

#include <Wt/Test/WTestEnvironment.h>

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WStringStream.h>


BOOST_AUTO_TEST_CASE( WLineEdit_setText_and_mask_after_render )
{
  Wt::Test::WTestEnvironment environment;
  Wt::WApplication app(environment);
  std::stringstream html;
  Wt::WStringStream js;

  auto lineEdit = app.root()->addWidget(std::make_unique<Wt::WLineEdit>());

  // Start by rendering the line edit
  lineEdit->htmlText(html, js);

  html.clear();
  js.clear();

  /*
   * Check that setting the text and the input mask does not generate
   * a setValue() call in the javascript
   */
  lineEdit->setInputMask("9999-999;_");
  lineEdit->setText("3510-010");

  lineEdit->htmlText(html, js);
  BOOST_CHECK(js.str().find(".wtLObj.setValue(") == std::string::npos);

  html.clear();
  js.clear();

  /*
   * Check that setting the text after the input mask update generates
   * a setValue() call in the javascript.
   */
  lineEdit->setText("3510-017");

  lineEdit->htmlText(html, js);
  BOOST_CHECK(js.str().find(".wtLObj.setValue(") != std::string::npos);
}
