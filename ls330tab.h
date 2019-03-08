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
#include <QCheckBox>
#include <QLabel>


class LS330Tab : public QWidget
{
    Q_OBJECT
public:
    explicit LS330Tab(int iConfiguration, QWidget *parent = nullptr);
    void restoreSettings();
    void saveSettings();
    void connectSignals();

public:
    double dTStart;
    double dTStop;
    double dTStep;
    double dTRate;
    int    iReachingTStart;
    int    iTimeToSteadyT;
    bool   bUseThermostat;

signals:

public slots:
    void on_ThermostatCheckBox_stateChanged(int arg1);
    void on_TStartEdit_textChanged(const QString &arg1);
    void on_TStopEdit_textChanged(const QString &arg1);
    void on_TStepEdit_textChanged(const QString &arg1);
    void on_MaxTimeToTStartEdit_textChanged(const QString &arg1);
    void on_TimeToSteadyTEdit_textChanged(const QString &arg1);
    void on_TRateEdit_textChanged(const QString &arg1);

protected:
    void initUI();
    void setToolTips();
    bool isTemperatureValid(double dTemperature);
    bool isTStepValid(double dTStep);
    bool isReachingTimeValid(int iReachingTime);
    bool isTimeToSteadyTValid(int iTime);
    bool isTRateValid(double dTRate);

private:
    // QLineEdit styles
    QString sNormalStyle;
    QString sErrorStyle;

    QLineEdit TStartEdit;
    QLineEdit TStopEdit;
    QLineEdit TStepEdit;
    QLineEdit TRateEdit;
    QLineEdit MaxTimeToTStartEdit;
    QLineEdit TimeToSteadyTEdit;

    QCheckBox ThermostatCheckBox;

    const double temperatureMin;
    const double temperatureMax;
    const double TRateMin;
    const double TRateMax;
    const int    waitTimeMin;
    const int    waitTimeMax;
    const int    reachingTMin;
    const int    reachingTMax;
    const int    timeToSteadyTMin;
    const int    timeToSteadyTMax;

    int          myConfiguration;
};
