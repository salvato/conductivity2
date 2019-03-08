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
#include "cs130tab.h"
#include "mainwindow.h"

#include <QGridLayout>
#include <QSettings>
#include <QLabel>
#include <QtDebug>


CS130Tab::CS130Tab(int iConfiguration, bool enableMonochromator, QWidget *parent)
    : QWidget(parent)
    , wavelengthMin(200.0)
    , wavelengthMax(1000.0)
    , myConfiguration(iConfiguration)
    , bUseMonochromator(enableMonochromator)
{
    radioButtonGrating1.setText("Grating #1");
    radioButtonGrating2.setText("Grating #2");
    darkPhotoCheck.setText("Photo");
    // Build the Tab layout
    QGridLayout* pLayout = new QGridLayout();
    pLayout->addWidget(&radioButtonGrating1, 0, 0, 1, 1);
    pLayout->addWidget(&radioButtonGrating2, 1, 0, 1, 1);
    pLayout->addWidget(&darkPhotoCheck,      0, 1, 1, 1);
    if(iConfiguration==MainWindow::iConfLScan) {
        pLayout->addWidget(new QLabel("Start Wavelength [nm]"), 2, 0, 1, 1);
        pLayout->addWidget(&StartWlEdit,                        2, 1, 1, 1);
        pLayout->addWidget(new QLabel("Stop Wavelength [nm]"),   3, 0, 1, 1);
        pLayout->addWidget(&StopWlEdit,                         3, 1, 1, 1);
    }
    else {
        pLayout->addWidget(new QLabel("Wavelength [nm]"), 2, 0, 1, 1);
        pLayout->addWidget(&WavelengthEdit, 2, 1, 1, 1);
        WavelengthEdit.setEnabled(bUseMonochromator);
    }
    radioButtonGrating1.setEnabled(bUseMonochromator);
    radioButtonGrating2.setEnabled(bUseMonochromator);
    setLayout(pLayout);

    sNormalStyle = WavelengthEdit.styleSheet();

    sErrorStyle  = "QLineEdit { ";
    sErrorStyle += "color: rgb(255, 255, 255);";
    sErrorStyle += "background: rgb(255, 0, 0);";
    sErrorStyle += "selection-background-color: rgb(128, 128, 255);";
    sErrorStyle += "}";

    connectSignals();
    restoreSettings();
    setToolTips();
    initUI();
}


void
CS130Tab::restoreSettings() {
    QSettings settings;
    dWavelength      = settings.value("CS130TabWavelength", wavelengthMax).toDouble();
    dStartWavelength = settings.value("CS130TabStartWavelength", wavelengthMax).toDouble();
    dStopWavelength  = settings.value("CS130TabStopWavelength", wavelengthMax).toDouble();
    iGratingNumber   = settings.value("CS130TabGrating", 1).toInt();
    bPhoto           = settings.value("CS130TabPhoto", false).toBool();
    enableMonochromator(bPhoto);
}


void
CS130Tab::saveSettings() {
    QSettings settings;
    settings.setValue("CS130TabWavelength", dWavelength);
    settings.setValue("CS130TabStartWavelength", dStartWavelength);
    settings.setValue("CS130TabStopWavelength", dStopWavelength);
    settings.setValue("CS130TabGrating", iGratingNumber);
    settings.setValue("CS130TabPhoto", bPhoto);
}


void
CS130Tab::setToolTips() {
    QString sHeader = QString("Enter values in range [%1 : %2]");
    WavelengthEdit.setToolTip(sHeader.arg(wavelengthMin).arg(wavelengthMax));
    StartWlEdit.setToolTip(sHeader.arg(wavelengthMin).arg(wavelengthMax));
    StopWlEdit.setToolTip(sHeader.arg(wavelengthMin).arg(wavelengthMax));
    darkPhotoCheck.setToolTip("Choose a Photo or Dark Measurement");
}


void
CS130Tab::initUI() {
    if(iGratingNumber == 1) {
        radioButtonGrating1.setChecked(true);
        radioButtonGrating2.setChecked(false);
    }
    else {
        iGratingNumber = 2;
        radioButtonGrating1.setChecked(false);
        radioButtonGrating2.setChecked(true);
    }
    darkPhotoCheck.setChecked(bPhoto);
    if(!isWavelengthValid(dWavelength))
        dWavelength = wavelengthMax;
    WavelengthEdit.setText(QString("%1").arg(dWavelength, 0, 'f', 2));
    if(!isWavelengthValid(dStartWavelength))
        dStartWavelength = wavelengthMax;
    StartWlEdit.setText(QString("%1").arg(dStartWavelength, 0, 'f', 2));
    if(!isWavelengthValid(dStopWavelength))
        dStopWavelength = wavelengthMax;
    StopWlEdit.setText(QString("%1").arg(dStopWavelength, 0, 'f', 2));
}


void
CS130Tab::connectSignals() {
    connect(&WavelengthEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(on_WavelengthEdit_textChanged(const QString)));
    connect(&StartWlEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(on_StartWlEdit_textChanged(const QString)));
    connect(&StopWlEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(on_StopWlEdit_textChanged(const QString)));
    connect(&radioButtonGrating1, SIGNAL(clicked()),
            this, SLOT(on_grating1_Selected()));
    connect(&radioButtonGrating2, SIGNAL(clicked()),
            this, SLOT(on_grating2_Selected()));
    connect(&darkPhotoCheck, SIGNAL(stateChanged(int)),
            this, SLOT(on_darkPhotoCheck_Clicked(int)));
}


void
CS130Tab::on_WavelengthEdit_textChanged(const QString &arg1) {
    if(isWavelengthValid(arg1.toDouble())) {
        dWavelength = arg1.toDouble();
        WavelengthEdit.setStyleSheet(sNormalStyle);
    }
    else {
        WavelengthEdit.setStyleSheet(sErrorStyle);
    }
}


void
CS130Tab::on_StartWlEdit_textChanged(const QString &arg1) {
    if(isWavelengthValid(arg1.toDouble())) {
        dStartWavelength = arg1.toDouble();
        StartWlEdit.setStyleSheet(sNormalStyle);
    }
    else {
        StartWlEdit.setStyleSheet(sErrorStyle);
    }
}


void
CS130Tab::on_StopWlEdit_textChanged(const QString &arg1) {
    if(isWavelengthValid(arg1.toDouble())) {
        dStopWavelength = arg1.toDouble();
        StopWlEdit.setStyleSheet(sNormalStyle);
    }
    else {
        StopWlEdit.setStyleSheet(sErrorStyle);
    }
}


bool
CS130Tab::isWavelengthValid(double wavelength) {
    return (wavelength >= wavelengthMin) &&
           (wavelength <= wavelengthMax);
}



void
CS130Tab::on_darkPhotoCheck_Clicked(int newState) {
    bPhoto = (newState==Qt::Checked);
    enableMonochromator(bPhoto);
}


void
CS130Tab::on_grating1_Selected() {
    iGratingNumber = 1;
}


void
CS130Tab::on_grating2_Selected() {
    iGratingNumber = 2;
}


void
CS130Tab::enableMonochromator(bool bEnable) {
    if(!bUseMonochromator) return;
    radioButtonGrating1.setEnabled(bEnable);
    radioButtonGrating2.setEnabled(bEnable);
    WavelengthEdit.setEnabled(bEnable);
}

