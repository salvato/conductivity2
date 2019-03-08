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
#include "lakeshore330.h"

#include <QtGlobal>
//#include <QDebug>
#include <QThread>


namespace
lakeshore330 {
    static int rearmMask;
#if !defined(Q_OS_LINUX)
    int __stdcall
    myCallback(int LocalUd, unsigned long LocalIbsta, unsigned long LocalIberr, long LocalIbcntl, void* callbackData) {
        reinterpret_cast<LakeShore330*>(callbackData)->onGpibCallback(LocalUd, LocalIbsta, LocalIberr, LocalIbcntl);
        return rearmMask;
    }
#endif
}


LakeShore330::LakeShore330(int gpio, int address, QObject *parent)
    : GpibDevice(gpio, address, parent)
    // Status Byte bits
    , SRQ(64)// Service Request
    , ESB(32)// Standard Event Status
    , OVI(16)// Overload Indicator
    , CLE(4) // Control Limit Ready
    , CDR(2) // Control Data Ready
    , SDR(1) // Sample Data Ready
    // Standard Event Status Register bits
    , PON(128)// Power On
    , CME(32) // Command Error
    , EXE(16) // Execution Error
    , DDE(8)  // Device Dependent Error
    , QYE(4)  // Query Error
    , OPC(1)  // Operation Complete
{
    pollInterval = 659;
    Q_UNUSED(lakeshore330::rearmMask);
}


LakeShore330::~LakeShore330() {
    //  qDebug() << "LakeShore330::~LakeShore330()";
    if(gpibId != -1) {
        switchPowerOff();
#if !defined(Q_OS_LINUX)
        ibnotify (gpibId, 0, NULL, NULL);// disable notification
#endif
        ibonl(gpibId, 0);// Disable hardware and software.
    }
}


int
LakeShore330::init() {
    gpibId = ibdev(gpibNumber, gpibAddress, 0, T10s, 1, 0);
    if(gpibId < 0) {
        QString sError = QString(Q_FUNC_INFO) + ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(sError);
        return GPIB_DEVICE_NOT_PRESENT;
    }
    short listen;
    ibln(gpibNumber, gpibAddress, NO_SAD, &listen);
    if(isGpibError(QString(Q_FUNC_INFO) + "LakeShore 330 Not Respondig"))
        return GPIB_DEVICE_NOT_PRESENT;
    if(listen == 0) {
        ibonl(gpibId, 0);
        emit sendMessage(QString(Q_FUNC_INFO) + "Nolistener at Addr");
        return GPIB_DEVICE_NOT_PRESENT;
    }
#if defined(Q_OS_LINUX)
    connect(&pollTimer, SIGNAL(timeout()),
            this, SLOT(checkNotify()));
    pollTimer.start(pollInterval);
#else
    // set up the asynchronous event notification routine on RQS
    ibnotify(gpibId,
             RQS,
             (GpibNotifyCallback_t) lakeshore330::myCallback,
             this);
    if(isGpibError(QString(Q_FUNC_INFO) + "ibnotify call failed."))
        return -1;
#endif
    ibclr(gpibId);
    QThread::sleep(1);
    gpibWrite(gpibId, "*sre 0\r\n");// Set Service Request Enable
    if(isGpibError(QString(Q_FUNC_INFO) + "*sre Failed"))
        return -1;
    gpibWrite(gpibId, "RANG 0\r\n");// Sets heater status: 0 = off
    if(isGpibError(QString(Q_FUNC_INFO) + "RANG 0 Failed"))
        return -1;
    gpibWrite(gpibId, "CUNI K\r\n");// Set Units (Kelvin) for the Control Channel
    if(isGpibError(QString(Q_FUNC_INFO) + "CUNI Failed"))
        return -1;
    gpibWrite(gpibId, "SUNI K\r\n");// Set Units (Kelvin) for the Sample Channel.
    if(isGpibError(QString(Q_FUNC_INFO) + "SUNI Failed"))
        return -1;
    gpibWrite(gpibId, "RAMP 0\r\n");// Disables the ramping function
    if(isGpibError(QString(Q_FUNC_INFO) + "RAMP 0 Failed"))
        return -1;
    gpibWrite(gpibId, "TUNE 4\r\n");// Sets Autotuning Status to "Zone"
    if(isGpibError(QString(Q_FUNC_INFO) + "TUNE 4 Failed"))
        return -1;
    return 0;
}


void
LakeShore330::onGpibCallback(int LocalUd, unsigned long LocalIbsta, unsigned long LocalIberr, long LocalIbcntl) {
    Q_UNUSED(LocalUd)
    Q_UNUSED(LocalIbsta)
    Q_UNUSED(LocalIberr)
    Q_UNUSED(LocalIbcntl)
    //qDebug() << "LakeShore330::onGpibCallback()";
    quint8 spollByte;
    ibrsp(gpibId, reinterpret_cast<char*>(&spollByte));
    if(isGpibError(QString(Q_FUNC_INFO) + "ibrsp() Failed"))
        return;
    //qDebug() << "spollByte=" << spollByte;
    if(spollByte & ESB) {
        //qDebug() << "LakeShore330::onGpibCallback(): Standard Event Status";
        gpibWrite(gpibId, "*ESR?\r\n");// Query Std. Event Status Register
        if(isGpibError(QString(Q_FUNC_INFO) + "*ESR? Failed"))
            return;
        quint8 esr = quint8(gpibRead(gpibId).toInt());
        if(isGpibError(QString(Q_FUNC_INFO) + "Read of Std. Event Status Register Failed"))
            return;
        if(esr & PON) {
            //qDebug() << "LakeShore330::onGpibCallback(): Event Status Register PON";
        }
        if(esr & CME) {
            //qDebug() << "LakeShore330::onGpibCallback(): Event Status Register CME";
        }
        if(esr & EXE) {
            //qDebug() << "LakeShore330::onGpibCallback(): Event Status Register EXE";
        }
        if(esr & DDE) {
            //qDebug() << "LakeShore330::onGpibCallback(): Event Status Register DDE";
        }
        if(esr & QYE) {
            //qDebug() << "LakeShore330::onGpibCallback(): Event Status Register QYE";
        }
        if(esr & OPC) {
            //qDebug() << "LakeShore330::onGpibCallback(): Event Status Register OPC";
        }
    }
    if(spollByte & OVI) {
        emit sendMessage(QString(Q_FUNC_INFO) + "Overload Indicator");
    }
    if(spollByte & CLE) {
        emit sendMessage(QString(Q_FUNC_INFO) + "Control Limit Error");
    }
    if(spollByte & CDR) {
        emit sendMessage(QString(Q_FUNC_INFO) + "Control Data Ready");
    }
    if(spollByte & SDR) {
        emit sendMessage(QString(Q_FUNC_INFO) + "Sample Data Ready");
    }
}


double
LakeShore330::getTemperature() {
    gpibWrite(gpibId, "SDAT?\r\n");// Query the Sample Sensor Data.
    if(isGpibError(QString(Q_FUNC_INFO) + "SDAT? Failed"))
        return 0.0;
    sResponse = gpibRead(gpibId);
    if(isGpibError(QString(Q_FUNC_INFO) + "Failed"))
        return 0.0;
    return sResponse.toDouble();
}


bool
LakeShore330::setTemperature(double Temperature) {
    //qDebug() << QString("LakeShore330::setTemperature(%1)").arg(Temperature);
    if(Temperature < 0.0 || Temperature > 900.0) return false;
    sCommand = QString("SETP %1\r\n").arg(Temperature, 0, 'f', 2);
    gpibWrite(gpibId, sCommand);// Sets the Setpoint
    if(isGpibError(QString(Q_FUNC_INFO) + "SETP Failed"))
        return false;
    return true;
}


bool
LakeShore330::switchPowerOn(int iRange) {
    // Sets heater status: 1 = low, 2 = medium, 3 = high.
    gpibWrite(gpibId, QString("RANG %1\r\n").arg(iRange));
    if(isGpibError(QString(Q_FUNC_INFO) + QString("switchPowerOn(%1): Failed").arg(iRange)))
        return false;
    gpibWrite(gpibId, "RANG?\r\n");
    if(iRange != gpibRead(gpibId).toInt()) {
        emit sendMessage("Error setting Heater Range");
    }
    return true;
}


bool
LakeShore330::switchPowerOff() {
    //qDebug() << "LakeShore330::switchPowerOff()";
    gpibWrite(gpibId, "*SRE 0\r\n");// Set Service Request Enable to No SRQ
    // Sets heater status: 0 = off.
    gpibWrite(gpibId, "RANG 0\r\n");
    if(isGpibError(QString(Q_FUNC_INFO) + QString("Failed")) )
        return false;
    return true;
}


bool
LakeShore330::startRamp(double targetT, double rate) {
    sCommand = QString("RAMPR %1\r\n").arg(rate);
    gpibWrite(gpibId, sCommand);
    if(isGpibError(QString(Q_FUNC_INFO) + "Unable to set Ramp Rate"))
        return false;
    if(!setTemperature(targetT))
        return false;
    sCommand = QString("RAMP 1\r\n");
    gpibWrite(gpibId, sCommand);
    QThread::sleep(1);
    if(isGpibError(QString(Q_FUNC_INFO) + "Unable to Start Ramp"))
        return false;
    //qDebug() << QString("LakeShore330::startRamp(%1)").arg(rate);
    return true;
}


bool
LakeShore330::stopRamp() {
    sCommand = QString("RAMP 0\r\n");
    gpibWrite(gpibId, sCommand);
    QThread::sleep(1);
    if(isGpibError(QString(Q_FUNC_INFO) + "Unable to Stop Ramp"))
        return false;
    return true;
}


bool
LakeShore330::isRamping() {
    sCommand = QString("RAMPS?\r\n");
    gpibWrite(gpibId, sCommand);
    if(isGpibError(QString(Q_FUNC_INFO) + "Unable to query Ramp Status"))
// Unable to query Ramp Status...assume it is still ramping
        return true;
    int iRamping = gpibRead(gpibId).toInt();
    if(isGpibError(QString(Q_FUNC_INFO) + "Unable to get Ramp Status"))
// Unable to get Ramp Status...assume it is still ramping
        return true;
    return (iRamping == 1);
}


void
LakeShore330::checkNotify() {
    ibrsp(gpibId, &spollByte);
    if(isGpibError(QString(Q_FUNC_INFO) + "Unable to query Ramp Status"))
        return;
    if(!(spollByte & 64))
        return;// SRQ not enabled
    onGpibCallback(gpibId, ulong(ThreadIbsta()), ulong(ThreadIberr()), ThreadIbcnt());
}
