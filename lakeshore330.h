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

#include <QtGlobal>
#include <QObject>
#include <QTimer>
#include "gpibdevice.h"


class LakeShore330 : public GpibDevice
{
    Q_OBJECT

public:
    explicit LakeShore330(int gpio, int address, QObject *parent=Q_NULLPTR);
    virtual ~LakeShore330();
    int      init();
    void     onGpibCallback(int ud, unsigned long ibsta, unsigned long iberr, long ibcntl);
    double   getTemperature();
    bool     setTemperature(double Temperature);
    bool     switchPowerOn(int iRange);
    bool     switchPowerOff();
    bool     startRamp(double targetT, double rate);
    bool     stopRamp();
    bool     isRamping();

signals:

public slots:
    void checkNotify();

protected:
    QTimer pollTimer;

private:
    // Status Byte Register
    const quint8 SRQ;// Service Request
    const quint8 ESB;// Standard Event Status
    const quint8 OVI;// Overload Indicator
    const quint8 CLE;// Control Limit Error
    const quint8 CDR;// Control Data Ready
    const quint8 SDR;// Sample Data Ready
    // Standard Event Status Register
    const quint8 PON;// Power On
    const quint8 CME;// Command Error
    const quint8 EXE;// Execution Error
    const quint8 DDE;// Device Dependent Error
    const quint8 QYE;// Query Error
    const quint8 OPC;// Operation Complete
};
