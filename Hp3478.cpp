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
#include "Hp3478.h"

#include <QtMath>
#include <QDateTime>
#include <QThread>
//#include <QDebug>


namespace hp3478 {
static int  rearmMask;
#if !defined(Q_OS_LINUX)
int __stdcall
myCallback(int LocalUd, unsigned long LocalIbsta, unsigned long LocalIberr, long LocalIbcntl, void* callbackData) {
    reinterpret_cast<Keithley236*>(callbackData)->onGpibCallback(LocalUd, LocalIbsta, LocalIberr, LocalIbcntl);
    return Hp3478::rearmMask;
}
#endif
}


Hp3478::Hp3478(int gpio, int address, QObject *parent)
    : GpibDevice(gpio, address, parent)
{
    pollInterval = 569;
}


Hp3478::~Hp3478() {
    if(gpibId != -1) {
#if defined(Q_OS_LINUX)
        pollTimer.stop();
        pollTimer.disconnect();
#else
        ibnotify(gpibId, 0, NULL, NULL);// disable notification
#endif
        ibonl(gpibId, 0);// Disable hardware and software.
    }
}


int
Hp3478::init(){
    gpibId = ibdev(gpibNumber, gpibAddress, 0, T10s, 1, 0);
    if(gpibId < 0) {
        QString sError = ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(Q_FUNC_INFO + sError);
        return GPIB_DEVICE_NOT_PRESENT;
    }
    short listen;
    ibln(gpibNumber, gpibAddress, NO_SAD, &listen);
    if(isGpibError(QString(Q_FUNC_INFO) + "Hp 3478A Not Respondig"))
        return GPIB_DEVICE_NOT_PRESENT;
    if(listen == 0) {
        ibonl(gpibId, 0);
        emit sendMessage("Nolistener at Addr");
        return GPIB_DEVICE_NOT_PRESENT;
    }
    // set up the asynchronous event notification routine on RQS
#if defined(Q_OS_LINUX)
    connect(&pollTimer, SIGNAL(timeout()),
            this, SLOT(checkNotify()));
    pollTimer.start(pollInterval);
#else
    ibnotify(gpibId,
             RQS,
             (GpibNotifyCallback_t) hp3478::myCallback,
             this);
    if(isGpibError(QString(Q_FUNC_INFO) + "ibnotify call failed."))
        return -1;
#endif
    ibclr(gpibId);
    QThread::sleep(1);
    return NO_ERROR;
}


void
Hp3478::checkNotify() {
#if defined(Q_OS_LINUX)
    ibrsp(gpibId, &spollByte);
    if(isGpibError(QString(Q_FUNC_INFO) + "ibrsp() Error"))
    if(!(spollByte & 64))
        return; // SRQ not enabled
    onGpibCallback(gpibId, uint(ThreadIbsta()), uint(ThreadIberr()), ThreadIbcnt());
#endif
}


int
Hp3478::endRvsTime() {
#if defined(Q_OS_LINUX)
    pollTimer.stop();
    pollTimer.disconnect();
#else
    ibnotify(gpibId, 0, NULL, NULL);// disable notification
#endif
    gpibWrite(gpibId, "F4");// 4W Ohm
    gpibWrite(gpibId, "RA");// Autorange
    gpibWrite(gpibId, "N5");// 5 Digits
    gpibWrite(gpibId, "Z1");// Auto zero ON
    gpibWrite(gpibId, "D1");// Normal Display
    gpibWrite(gpibId, "T1");// Internal Trigger
    return NO_ERROR;
}


int
Hp3478::initRvsTime() {
    uint iErr = 0;
    iErr |= gpibWrite(gpibId, "M00");// SRQ Disabled
    iErr |= gpibWrite(gpibId, "F4"); // 4W Ohm
    iErr |= gpibWrite(gpibId, "RA"); // Autorange
    iErr |= gpibWrite(gpibId, "N5"); // 5 Digits
    iErr |= gpibWrite(gpibId, "Z1"); // Auto zero ON
    iErr |= gpibWrite(gpibId, "D1"); // Normal Display
    iErr |= gpibWrite(gpibId, "T3"); // Single Trigger
    if(iErr & ERR) {
        QString sError;
        sError = QString(Q_FUNC_INFO) + QString("GPIB Error in gpibWrite(): - Status= %1")
                .arg(ThreadIbsta(), 4, 16, QChar('0'));
        sError += ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(sError);
        return -1;
    }
    iErr = gpibWrite(gpibId, "M75");// Enable SRQ
    if(iErr & ERR) {
        QString sError;
        sError = QString(Q_FUNC_INFO) + QString("GPIB Error in gpibWrite(): - Status= %1")
                .arg(ThreadIbsta(), 4, 16, QChar('0'));
        sError += ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(sError);
        return -1;
    }
    return NO_ERROR;
}


void
Hp3478::onGpibCallback(int LocalUd, unsigned long LocalIbsta, unsigned long LocalIberr, long LocalIbcntl) {
    Q_UNUSED(LocalIbsta)
    Q_UNUSED(LocalIberr)
    Q_UNUSED(LocalIbcntl)

    if(LocalIbsta & ERR) {
        if(!(LocalIberr & ESRQ)) {
            emit sendMessage(QString(Q_FUNC_INFO) + QString("SRQService(HP3478A): Ibsta Error %1"). arg(LocalIberr));
        }
    }
    if(ibrsp(LocalUd, &spollByte) & ERR) {
        emit sendMessage(QString(Q_FUNC_INFO) + QString("GPIB error %1").arg(LocalIberr));
    }

    if((spollByte & 1)){// Reading Done
        sResponse = gpibRead(LocalUd);
        if(sResponse != QString()) {
            QDateTime currentTime = QDateTime::currentDateTime();
            emit newReading(currentTime, sResponse);
        }
    }
    hp3478::rearmMask = RQS;
}


bool
Hp3478::sendTrigger() {
    ibtrg(gpibId);
    if(isGpibError(QString(Q_FUNC_INFO) + "Trigger Error"))
        return false;
    return true;
}


bool
Hp3478::isReadyForTrigger() {
    return true;
}

