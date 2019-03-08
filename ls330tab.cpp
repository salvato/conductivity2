/*
 *
Copyright (C) 2016  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/
#include "ls330tab.h"
#include "mainwindow.h"

#include <QLineEdit>
#include <QLabel>
#include <QRadioButton>
#include <QGridLayout>
#include <QSettings>
#include <QtDebug>


LS330Tab::LS330Tab(int iConfiguration, QWidget *parent)
    : QWidget(parent)
    , temperatureMin(0.0)
    , temperatureMax(475.0)
    , TRateMin(0.1)
    , TRateMax(10.0)
    , waitTimeMin(100)
    , waitTimeMax(65000)
    , reachingTMin(0)// In minutes
    , reachingTMax(60)// In minutes
    , timeToSteadyTMin(0)// In minutes
    , timeToSteadyTMax(INT_MAX/(60*1000))// In minutes
    , myConfiguration(iConfiguration)
{
    sNormalStyle = TStartEdit.styleSheet();

    sErrorStyle  = "QLineEdit { ";
    sErrorStyle += "color: rgb(255, 255, 255);";
    sErrorStyle += "background: rgb(255, 0, 0);";
    sErrorStyle += "selection-background-color: rgb(128, 128, 255);";
    sErrorStyle += "}";

    ThermostatCheckBox.setText("Use Thermostat");
    // Build the Tab layout
    QGridLayout* pLayout = new QGridLayout();
    pLayout->addWidget(&ThermostatCheckBox,                     0, 0, 1, 1);

    pLayout->addWidget(new QLabel("T Start [K]"),               0, 1, 1, 1, Qt::AlignRight);

    if(myConfiguration == MainWindow::iConfIvsV) {
        pLayout->addWidget(new QLabel("T Stop[K]"),             1, 1, 1, 1, Qt::AlignRight);
        pLayout->addWidget(new QLabel("T Step[K]"),             2, 1, 1, 1, Qt::AlignRight);
        pLayout->addWidget(new QLabel("Max wait for T Start[min]"), 3, 0, 1, 2, Qt::AlignRight);
        pLayout->addWidget(new QLabel("Time to steady Temp.[min]"), 4, 0, 1, 2, Qt::AlignRight);
    }
    else if(myConfiguration == MainWindow::iConfRvsT) {
        pLayout->addWidget(new QLabel("T Stop[K]"),             1, 1, 1, 1, Qt::AlignRight);
        pLayout->addWidget(new QLabel("T Rate[K/min]"),         2, 1, 1, 1, Qt::AlignRight);
        pLayout->addWidget(new QLabel("Max wait for T Start[min]"), 3, 0, 1, 2, Qt::AlignRight);
        pLayout->addWidget(new QLabel("Time to steady Temp.[min]"), 4, 0, 1, 2, Qt::AlignRight);
    }
    else if(myConfiguration == MainWindow::iConfRvsTime) {
        pLayout->addWidget(new QLabel("T Steady[K]"),           1, 1, 1, 1, Qt::AlignRight);
        pLayout->addWidget(new QLabel("T Rate[K/min]"),         2, 1, 1, 1, Qt::AlignRight);
    }

    pLayout->addWidget(&TStartEdit,          0, 2, 1, 1);
    if(myConfiguration == MainWindow::iConfIvsV) {
        pLayout->addWidget(&TStopEdit,       1, 2, 1, 1);
        pLayout->addWidget(&TStepEdit,       2, 2, 1, 1);
        pLayout->addWidget(&MaxTimeToTStartEdit, 3, 2, 1, 1);
        pLayout->addWidget(&TimeToSteadyTEdit,   4, 2, 1, 1);
    }
    else if(myConfiguration == MainWindow::iConfRvsT) {
        pLayout->addWidget(&TStopEdit,       1, 2, 1, 1);
        pLayout->addWidget(&TRateEdit,       2, 2, 1, 1);
        pLayout->addWidget(&MaxTimeToTStartEdit, 3, 2, 1, 1);
        pLayout->addWidget(&TimeToSteadyTEdit,   4, 2, 1, 1);
    }
    else if(myConfiguration == MainWindow::iConfRvsTime) {
        pLayout->addWidget(&TStopEdit,       1, 2, 1, 1);
        pLayout->addWidget(&TRateEdit,       2, 2, 1, 1);
    }

    setLayout(pLayout);

    connectSignals();
    restoreSettings();
    setToolTips();
    initUI();
}


void
LS330Tab::restoreSettings() {
    QSettings settings;
    dTStart        = settings.value("LS330TabTStart", 300.0).toDouble();
    dTStop         = settings.value("LS330TabTStop", dTStart).toDouble();
    dTRate         = settings.value("LS330TabTRate", 1.0).toDouble();
    dTStep         = settings.value("LS330TabTStep", 1.0).toDouble();
    iReachingTStart= settings.value("LS330TabReachingTStart", 0).toInt();
    iTimeToSteadyT = settings.value("LS330TabSteadyT", 0).toInt();
    bUseThermostat = settings.value("LS330TabUseThermostat", false).toBool();
}


void
LS330Tab::saveSettings() {
    QSettings settings;
    settings.setValue("LS330TabTStart", dTStart);
    settings.setValue("LS330TabTStop", dTStop);
    settings.setValue("LS330TabTRate", dTRate);
    settings.setValue("LS330TabTStep", dTStep);
    settings.setValue("LS330TabReachingTStart", iReachingTStart);
    settings.setValue("LS330TabSteadyT", iTimeToSteadyT);
    settings.setValue("LS330TabUseThermostat", bUseThermostat);
}


void
LS330Tab::setToolTips() {
    QString sHeader = QString("Enter values in range [%1 : %2]");
    ThermostatCheckBox.setToolTip(QString("Enable/Disable Thermostat Use"));
    TStartEdit.setToolTip(sHeader.arg(temperatureMin).arg(temperatureMax));
    TStopEdit.setToolTip(sHeader.arg(temperatureMin).arg(temperatureMax));
    TRateEdit.setToolTip(sHeader.arg(TRateMin).arg(TRateMax));
    TStepEdit.setToolTip("Enter values greater than 1.0");
    MaxTimeToTStartEdit.setToolTip(sHeader.arg(reachingTMin).arg(reachingTMax));
    TimeToSteadyTEdit.setToolTip(sHeader.arg(timeToSteadyTMin).arg(timeToSteadyTMax));
}


void
LS330Tab::initUI() {
    // Temperature Control
    if(!isTemperatureValid(dTStart))
        dTStart = 300.0;
    TStartEdit.setText(QString("%1").arg(dTStart, 0, 'f', 2));
    if(!isTemperatureValid(dTStop))
        dTStop = dTStart;
    TStopEdit.setText(QString("%1").arg(dTStop, 0, 'f', 2));
    if(!isTRateValid(dTRate)) {
        dTRate = 1.0;
    }
    TRateEdit.setText(QString("%1").arg(dTRate, 0, 'f', 2));
    if(!isTStepValid(dTStep))
        dTStep = 1.0;
    TStepEdit.setText(QString("%1").arg(dTStep, 0, 'f', 2));

    ThermostatCheckBox.setChecked(bUseThermostat);
    TStartEdit.setEnabled(bUseThermostat);
    TStopEdit.setEnabled(bUseThermostat);
    TStepEdit.setEnabled(bUseThermostat);
    MaxTimeToTStartEdit.setEnabled(bUseThermostat);
    TimeToSteadyTEdit.setEnabled(bUseThermostat);

    // Timing parameters
    if(!isReachingTimeValid(iReachingTStart))
        iReachingTStart = reachingTMin;
    MaxTimeToTStartEdit.setText(QString("%1").arg(iReachingTStart));
    if(!isTimeToSteadyTValid(iTimeToSteadyT))
        iTimeToSteadyT = timeToSteadyTMin;
    TimeToSteadyTEdit.setText(QString("%1").arg(iTimeToSteadyT));
}


void
LS330Tab::connectSignals() {
    connect(&TStartEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(on_TStartEdit_textChanged(const QString)));
    connect(&TStopEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(on_TStopEdit_textChanged(const QString)));
    connect(&TRateEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(on_TRateEdit_textChanged(const QString)));
    connect(&TStepEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(on_TStepEdit_textChanged(const QString)));
    connect(&MaxTimeToTStartEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(on_MaxTimeToTStartEdit_textChanged(const QString)));
    connect(&TimeToSteadyTEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(on_TimeToSteadyTEdit_textChanged(const QString)));
    connect(&ThermostatCheckBox, SIGNAL(stateChanged(int)),
            this, SLOT(on_ThermostatCheckBox_stateChanged(int)));
}


bool
LS330Tab::isTemperatureValid(double dTemperature) {
    return (dTemperature >= temperatureMin) &&
            (dTemperature <= temperatureMax);
}


bool
LS330Tab::isTRateValid(double dTRate) {
    return (dTRate >= TRateMin) && (dTRate <= TRateMax);
}


bool
LS330Tab::isTStepValid(double dTStep) {
    return (dTStep >= 1.0);
}


bool
LS330Tab::isReachingTimeValid(int iReachingTime) {
    return (iReachingTime >= reachingTMin) &&
            (iReachingTime <= reachingTMax);
}


bool
LS330Tab::isTimeToSteadyTValid(int iTime) {
    return (iTime >= timeToSteadyTMin) &&
            (iTime <= timeToSteadyTMax);
}


void
LS330Tab::on_ThermostatCheckBox_stateChanged(int arg1) {
    bUseThermostat = arg1;
    TStartEdit.setEnabled(bUseThermostat);
    TStopEdit.setEnabled(bUseThermostat);
    TStepEdit.setEnabled(bUseThermostat);
    MaxTimeToTStartEdit.setEnabled(bUseThermostat);
    TimeToSteadyTEdit.setEnabled(bUseThermostat);
}


void
LS330Tab::on_TStartEdit_textChanged(const QString &arg1) {
    if(isTemperatureValid(arg1.toDouble())){
        dTStart = arg1.toDouble();
        TStartEdit.setStyleSheet(sNormalStyle);
    }
    else {
        TStartEdit.setStyleSheet(sErrorStyle);
    }
}


void
LS330Tab::on_TStopEdit_textChanged(const QString &arg1) {
    if(isTemperatureValid(arg1.toDouble())){
        dTStop = arg1.toDouble();
        TStopEdit.setStyleSheet(sNormalStyle);
    }
    else {
        TStopEdit.setStyleSheet(sErrorStyle);
    }
}


void
LS330Tab::on_TRateEdit_textChanged(const QString &arg1) {
    if(isTRateValid(arg1.toDouble())){
        dTRate = arg1.toDouble();
        TRateEdit.setStyleSheet(sNormalStyle);
    }
    else {
        TRateEdit.setStyleSheet(sErrorStyle);
    }
}


void
LS330Tab::on_TStepEdit_textChanged(const QString &arg1) {
    if(isTStepValid(arg1.toDouble())){
        dTStep = arg1.toDouble();
        TStepEdit.setStyleSheet(sNormalStyle);
    }
    else {
        TStepEdit.setStyleSheet(sErrorStyle);
    }
}


void
LS330Tab::on_MaxTimeToTStartEdit_textChanged(const QString &arg1) {
    if(isReachingTimeValid(arg1.toInt())) {
        iReachingTStart = arg1.toInt();
        MaxTimeToTStartEdit.setStyleSheet(sNormalStyle);
    }
    else {
        MaxTimeToTStartEdit.setStyleSheet(sErrorStyle);
    }
}


void
LS330Tab::on_TimeToSteadyTEdit_textChanged(const QString &arg1) {
    if(isTimeToSteadyTValid(arg1.toInt())) {
        iTimeToSteadyT = arg1.toInt();
        TimeToSteadyTEdit.setStyleSheet(sNormalStyle);
    }
    else {
        TimeToSteadyTEdit.setStyleSheet(sErrorStyle);
    }
}
