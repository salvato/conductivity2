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
#include "cornerstone130.h"

//#include <QDebug>
#include <QThread>


CornerStone130::CornerStone130(int gpio, int address, QObject *parent)
    : GpibDevice(gpio, address, parent)
{
}


CornerStone130::~CornerStone130() {
    if(gpibId != -1) {
        ibonl(gpibId, 0);// Disable hardware and software.
    }
}


int
CornerStone130::init() {
    gpibId = ibdev(gpibNumber, gpibAddress, 0, T30s, 1, REOS|0x000A);
    if(gpibId < 0) {
        QString sError = QString("ibdev() Failed") + ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(Q_FUNC_INFO + sError);
        return GPIB_DEVICE_NOT_PRESENT;
    }
    ibclr(gpibId);
    if(isGpibError(QString(Q_FUNC_INFO) + "CornerStone 130 Not Respondig"))
        return GPIB_DEVICE_NOT_PRESENT;
    QThread::sleep(1);

    // Abort any operation in progress
    gpibWrite(gpibId, "ABORT\r\n");
    if(isGpibError(QString(Q_FUNC_INFO) + "CornerStone 130 ABORT Failed"))
        return GPIB_DEVICE_NOT_PRESENT;

    closeShutter();

    // We express the Wavelengths in nm
    gpibWrite(gpibId, "UNITS NM\r\n");
    if(isGpibError(QString(Q_FUNC_INFO) + "CornerStone 130 UNITS Failed"))
        return GPIB_DEVICE_NOT_PRESENT;
    // Set initial Filter Wavelength
    if(!setGrating(1))
        return GPIB_DEVICE_NOT_PRESENT;
    if(!setWavelength(560.0))
        return GPIB_DEVICE_NOT_PRESENT;
    return NO_ERROR;
}


bool
CornerStone130::openShutter() {
    gpibWrite(gpibId, "SHUTTER O\r\n");
    if(isGpibError(QString(Q_FUNC_INFO) + "CornerStone 130 Open SHUTTER Failed"))
        return false;
    return true;
}


bool
CornerStone130::closeShutter() {
    gpibWrite(gpibId, "SHUTTER C\r\n");
    if(isGpibError(QString(Q_FUNC_INFO) + "CornerStone 130 Close SHUTTER Failed"))
        return false;
    return true;
}


bool
CornerStone130::setWavelength(double waveLength) {
    sCommand = QString("GOWAVE %1\r\n").arg(waveLength);
    gpibWrite(gpibId, sCommand);
    if(isGpibError(QString(Q_FUNC_INFO) + "CornerStone 130 GOWAVE Failed"))
        return false;
    QThread::sleep(1);
    gpibWrite(gpibId, "WAVE?\r\n");
    if(isGpibError(QString(Q_FUNC_INFO) + "CornerStone 130 WAVE? Failed"))
        return false;
    sResponse = gpibRead(gpibId);
    if(isGpibError(QString(Q_FUNC_INFO) + "CornerStone 130 WAVE? readback Failed"))
        return false;
    dPresentWavelength = sResponse.toDouble();
//    qDebug() << "Present Wavelength =" << sResponse << "nm";
    return true;
}


bool
CornerStone130::setGrating(int grating) {
    if(grating <1 || grating > 2)
        return false;
    sCommand = QString("GRAT %1\r\n").arg(grating);
    gpibWrite(gpibId, sCommand);
    if(isGpibError(QString(Q_FUNC_INFO) + "CornerStone 130 GRAT Failed"))
        return false;
    return true;
}
