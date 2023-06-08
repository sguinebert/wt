// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2016 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WTIME_PICKER_H_
#define WTIME_PICKER_H_

#include <Wt/WComboBox.h>
#include <Wt/WCompositeWidget.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WJavaScriptSlot.h>
#include <Wt/WSpinBox.h>
#include <Wt/WStringStream.h>
#include <Wt/WTime.h>

namespace Wt {

class WTimeEdit;

class WT_API WTimePicker : public WCompositeWidget
{
    friend class WTimeEdit;
public:
    WTimePicker(WTimeEdit *timeEdit);

    WTime time() const;

    void setTime(const WTime& time);

    Signal<awaitable<void>()>& selectionChanged() { return selectionChanged_; }

    void setHourStep(int step);
    int hourStep() const;

    void setMinuteStep(int step);
    int minuteStep() const;

    void setSecondStep(int step);
    int secondStep() const;

    void setMillisecondStep(int step);
    int millisecondStep() const;

    void setWrapAroundEnabled(bool enabled);
    bool wrapAroundEnabled() const;

    void init(const WTime &time = WTime());

private:

    void configure();

    WT_USTRING format_;

    WSpinBox *sbhour_;
    WSpinBox *sbminute_;
    WSpinBox *sbsecond_;
    WSpinBox *sbmillisecond_;
    WComboBox *cbAP_;

    WTimeEdit *timeEdit_;

    awaitable<void> hourValueChanged();
    awaitable<void> minuteValueChanged();
    awaitable<void> secondValueChanged();
    awaitable<void> msecValueChanged();
    awaitable<void> ampmValueChanged();
    bool formatAp() const;
    bool formatMs() const;
    bool formatS() const;

    Signal<awaitable<void>()> selectionChanged_;
    JSlot toggleAmPm_;
};


} // end namespace Wt

#endif // WTIME_PICKER_H_
