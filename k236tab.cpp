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
#include "k236tab.h"
#include "mainwindow.h"

#include <QLineEdit>
#include <QLabel>
#include <QRadioButton>
#include <QGridLayout>
#include <QSettings>
#include <QtDebug>


K236Tab::K236Tab(int iConfiguration, QWidget *parent)
    : QWidget(parent)
    , bSourceI(true)
    , currentMin(-1.0e-2)
    , currentMax(1.0e-2)
    , voltageMin(-110.0)
    , voltageMax(110.0)
    , waitTimeMin(100)
    , waitTimeMax(65000)
    , nSweepPointsMin(3)
    , nSweepPointsMax(500)
    , intervalMin(0.1)
    , intervalMax(60.0)
    , myConfiguration(iConfiguration)
{
    // Create UI Elements
    SourceIButton.setText(QString("Source I - Measure V"));
    SourceVButton.setText(QString("Source V - Measure I"));

    // Build the Tab layout
    QGridLayout* pLayout = new QGridLayout();
    // Radio Buttons
    pLayout->addWidget(&SourceIButton, 0, 0, 1, 1);
    pLayout->addWidget(&SourceVButton, 0, 1, 1, 1);
    // Labels
    if(myConfiguration == MainWindow::iConfIvsV) {
        pLayout->addWidget(&StopLabel,                   2, 0, 1, 1);
        pLayout->addWidget(new QLabel("Rdgs Intv [ms]"), 4, 0, 1, 1);
        pLayout->addWidget(new QLabel("N°of Points"),    5, 0, 1, 1);
    }
    else {
        pLayout->addWidget(new QLabel("Meas. Intv [s]"), 4, 0, 1, 1);
    }
    pLayout->addWidget(&StartLabel,      1, 0, 1, 1);
    pLayout->addWidget(&ComplianceLabel, 3, 0, 1, 1);
    //Line Edits
    pLayout->addWidget(&StartEdit,      1, 1, 1, 1);
    pLayout->addWidget(&ComplianceEdit, 3, 1, 1, 1);
    if(myConfiguration == 1) {
        pLayout->addWidget(&StopEdit,        2, 1, 1, 1);
        pLayout->addWidget(&WaitTimeEdit,    4, 1, 1, 1);
        pLayout->addWidget(&SweepPointsEdit, 5, 1, 1, 1);
    }
    else {
        pLayout->addWidget(&MeasureIntervalEdit, 4, 1, 1, 1);
    }
    // Set the Layout
    setLayout(pLayout);

    sNormalStyle = StartEdit.styleSheet();

    sErrorStyle  = "QLineEdit { ";
    sErrorStyle += "color: rgb(255, 255, 255);";
    sErrorStyle += "background: rgb(255, 0, 0);";
    sErrorStyle += "selection-background-color: rgb(128, 128, 255);";
    sErrorStyle += "}";

    connectSignals();
    restoreSettings();
    initUI();
}


void
K236Tab::restoreSettings() {
    QSettings settings;
    bSourceI      = settings.value("K236TabSourceI", true).toBool();
    dStart        = settings.value("K236TabStart", 0.0).toDouble();
    dStop         = settings.value("K236TabStop", 0.0).toDouble();
    dCompliance   = settings.value("K236TabCompliance", 0.0).toDouble();
    iWaitTime     = settings.value("K236TabWaitTime", 100).toInt();
    iNSweepPoints = settings.value("K236TabSweepPoints", 100).toInt();
    dInterval     = settings.value("K236TabMeasureInterval", 0.1).toDouble();
}


void
K236Tab::saveSettings() {
    QSettings settings;
    settings.setValue("K236TabSourceI",     bSourceI);
    settings.setValue("K236TabStart",       dStart);
    settings.setValue("K236TabStop",        dStop);
    settings.setValue("K236TabCompliance",  dCompliance);
    settings.setValue("K236TabWaitTime",    iWaitTime);
    settings.setValue("K236TabSweepPoints", iNSweepPoints);
    settings.setValue("K236TabMeasureInterval", dInterval);
}


void
K236Tab::setToolTips() {
    QString sHeader = QString("Enter values in range [%1 : %2]");
    if(bSourceI) {
        StartEdit.setToolTip(sHeader.arg(currentMin).arg(currentMax));
        StopEdit.setToolTip(sHeader.arg(currentMin).arg(currentMax));
        ComplianceEdit.setToolTip(sHeader.arg(voltageMin).arg(voltageMax));
    }
    else {
        StartEdit.setToolTip(sHeader.arg(voltageMin).arg(voltageMax));
        StopEdit.setToolTip(sHeader.arg(voltageMin).arg(voltageMax));
        ComplianceEdit.setToolTip(sHeader.arg(currentMin).arg(currentMax));
    }
    SourceIButton.setToolTip("Source Current - Measure Voltage");
    SourceVButton.setToolTip("Source Voltage - Measure Current");
    WaitTimeEdit.setToolTip(sHeader.arg(waitTimeMin).arg(waitTimeMax));
    SweepPointsEdit.setToolTip((sHeader.arg(nSweepPointsMin).arg(nSweepPointsMax)));
    MeasureIntervalEdit.setToolTip(sHeader.arg(intervalMin).arg(intervalMax));
}


void
K236Tab::initUI() {
    // Measurement parameters
    if(myConfiguration == MainWindow::iConfIvsV) {
        if(bSourceI) {
            SourceIButton.setChecked(true);
            if(!isCurrentValid(dStart))
                dStart = 0.0;
            if(!isCurrentValid(dStop))
                dStop = 0.0;
            StartLabel.setText("I Start [A]");
            StopLabel.setText("I Stop [A]");
            ComplianceLabel.setText("Compliance [V]");
        }
        else {
            SourceVButton.setChecked(true);
            if(!isVoltageValid(dStart))
                dStart = 0.0;
            if(!isVoltageValid(dStop))
                dStop = 0.0;
            StartLabel.setText("V Start [V]");
            StopLabel.setText("V Stop [V]");
            ComplianceLabel.setText("Compliance [A]");
        }
    }
    else {
        if(bSourceI) {
            SourceIButton.setChecked(true);
            if(!isCurrentValid(dStart))
                dStart = 0.0;
            StartLabel.setText("I(constant) [A]");
            ComplianceLabel.setText("Compliance [V]");
        }
        else {
            SourceVButton.setChecked(true);
            if(!isVoltageValid(dStart))
                dStart = 0.0;
            StartLabel.setText("V(constant) [V]");
            ComplianceLabel.setText("Compliance [A]");
        }
    }
    StartEdit.setText(QString("%1").arg(dStart, 0, 'g', 2));
    StopEdit.setText(QString("%1").arg(dStop, 0, 'g', 2));
    if(!isComplianceValid(dCompliance))
        dCompliance = 0.0;
    ComplianceEdit.setText(QString("%1").arg(dCompliance, 0, 'g', 2));
    if(!isWaitTimeValid(iWaitTime))
        iWaitTime = 100;
    WaitTimeEdit.setText(QString("%1").arg(iWaitTime));
    if(!isSweepPointNumberValid(iNSweepPoints))
        iNSweepPoints = 100;
    SweepPointsEdit.setText(QString("%1").arg(iNSweepPoints));
    if(!isIntervalValid(dInterval)) {
        dInterval = intervalMin;
    }
    MeasureIntervalEdit.setText(QString("%1").arg(dInterval, 0, 'f', 2));
    setToolTips();
}


void
K236Tab::connectSignals() {
    connect(&StartEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(onStartEdit_textChanged(const QString)));
    connect(&StopEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(onStopEdit_textChanged(const QString)));
    connect(&ComplianceEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(onComplianceEdit_textChanged(const QString)));
    connect(&WaitTimeEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(onWaitTimeEdit_textChanged(const QString)));
    connect(&SweepPointsEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(onSweepPointsEdit_textChanged(const QString)));
    connect(&SourceIButton, SIGNAL(clicked()),
            this, SLOT(onSourceIChecked()));
    connect(&SourceVButton, SIGNAL(clicked()),
            this, SLOT(onSourceVChecked()));
    connect(&MeasureIntervalEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(onMeasureIntervalEdit_textChanged(const QString)));
}


bool
K236Tab::isCurrentValid(double dCurrent) {
    return (dCurrent >= currentMin) &&
            (dCurrent <= currentMax);
}


bool
K236Tab::isVoltageValid(double dVoltage) {
    return (dVoltage >= voltageMin) &&
            (dVoltage <= voltageMax);
}


bool
K236Tab::isComplianceValid(double dCompliance){
    if(bSourceI)
        return isVoltageValid(dCompliance);
    else
        return isCurrentValid(dCompliance);
}


bool
K236Tab::isWaitTimeValid(int iWaitTime) {
    return (iWaitTime >= waitTimeMin) &&
            (iWaitTime <= waitTimeMax);
}


bool
K236Tab::isSweepPointNumberValid(int nSweepPoints) {
    return (nSweepPoints >= nSweepPointsMin) &&
            (nSweepPoints <= nSweepPointsMax);
}


bool
K236Tab::isIntervalValid(double interval) {
    return (interval >= intervalMin) && (interval <= intervalMax);
}


void
K236Tab::onStartEdit_textChanged(const QString &arg1) {
    double dTemp = arg1.toDouble();
    bool bValid = bSourceI ? isCurrentValid(dTemp) : isVoltageValid(dTemp);
    if(bValid) {
        dStart = dTemp;
        StartEdit.setStyleSheet(sNormalStyle);
    }
    else {
        StartEdit.setStyleSheet(sErrorStyle);
    }
}


void
K236Tab::onStopEdit_textChanged(const QString &arg1) {
    double dTemp = arg1.toDouble();
    bool bValid = bSourceI ? isCurrentValid(dTemp) : isVoltageValid(dTemp);
    if(bValid) {
        dStop = dTemp;
        StopEdit.setStyleSheet(sNormalStyle);
    }
    else {
        StopEdit.setStyleSheet(sErrorStyle);
    }
}


void
K236Tab::onComplianceEdit_textChanged(const QString &arg1) {
    double dTemp = arg1.toDouble();
    bool bValid = bSourceI ? isVoltageValid(dTemp) : isCurrentValid(dTemp);
    if(bValid) {
        dCompliance = dTemp;
        ComplianceEdit.setStyleSheet(sNormalStyle);
    }
    else {
        ComplianceEdit.setStyleSheet(sErrorStyle);
    }
}


void
K236Tab::onSourceIChecked() {
    bSourceI = true;
    initUI();
}


void
K236Tab::onSourceVChecked() {
    bSourceI = false;
    initUI();
}


void
K236Tab::onWaitTimeEdit_textChanged(const QString &arg1) {
    int iTemp = arg1.toInt();
    if(isWaitTimeValid(iTemp)) {
        iWaitTime = iTemp;
        WaitTimeEdit.setStyleSheet(sNormalStyle);
    }
    else {
        WaitTimeEdit.setStyleSheet(sErrorStyle);
    }
}


void
K236Tab::onSweepPointsEdit_textChanged(const QString &arg1) {
    int iTemp = arg1.toInt();
    if(isSweepPointNumberValid(iTemp)) {
        iNSweepPoints = iTemp;
        SweepPointsEdit.setStyleSheet(sNormalStyle);
    }
    else {
        SweepPointsEdit.setStyleSheet(sErrorStyle);
    }
}


void
K236Tab::onMeasureIntervalEdit_textChanged(const QString &arg1) {
    if(isIntervalValid(arg1.toDouble())){
        dInterval = arg1.toDouble();
        MeasureIntervalEdit.setStyleSheet(sNormalStyle);
    }
    else {
        MeasureIntervalEdit.setStyleSheet(sErrorStyle);
    }
}


