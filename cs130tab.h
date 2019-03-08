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
#pragma once

#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QRadioButton>
#include <QCheckBox>


class CS130Tab : public QWidget
{
    Q_OBJECT
public:
    explicit CS130Tab(int iConfiguration, bool enableMonochromator, QWidget *parent = nullptr);
    void restoreSettings();
    void saveSettings();

signals:

public slots:
    void on_WavelengthEdit_textChanged(const QString &arg1);
    void on_StartWlEdit_textChanged(const QString &arg1);
    void on_StopWlEdit_textChanged(const QString &arg1);
    void on_darkPhotoCheck_Clicked(int newState);
    void on_grating1_Selected();
    void on_grating2_Selected();

public:
    double dWavelength;
    double dStartWavelength;
    double dStopWavelength;
    int    iGratingNumber;
    bool   bPhoto;

protected:
    void initUI();
    void setToolTips();
    void connectSignals();
    bool isWavelengthValid(double wavelength);
    void enableMonochromator(bool bEnable);

private:
    // QLineEdit styles
    QString sNormalStyle;
    QString sErrorStyle;
    // UI Elements
    QLineEdit WavelengthEdit;
    QLineEdit StartWlEdit;
    QLineEdit StopWlEdit;
    QRadioButton radioButtonGrating1;
    QRadioButton radioButtonGrating2;
    QCheckBox darkPhotoCheck;

    const double wavelengthMin;
    const double wavelengthMax;

    int          myConfiguration;
    bool         bUseMonochromator;
};
