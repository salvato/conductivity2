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
}


void
Hp3478::onGpibCallback(int LocalUd, unsigned long LocalIbsta, unsigned long LocalIberr, long LocalIbcntl) {
    Q_UNUSED(LocalIbsta)
    Q_UNUSED(LocalIberr)
    Q_UNUSED(LocalIbcntl)
/*
    if(msk == 0) {
        errmsg("SRQService(HP34401): Stale Interrupt", Ibsta, Iberr, Ibcntl);
        GpibError(LastError);
        *RearmMask = msk;
        return;
    }
    if(Ibsta & ERR) {
        if(!(Iberr & ESRQ)) {
            errmsg("SRQService(HP34401): Ibsta Error", Ibsta, Iberr, Ibcntl);
            GpibError(LastError);
        }
    }
    CHAR spb;
    ibrsp(ud, &spb);
    if (ThreadIbsta() & ERR) {
        if(ThreadIberr() & ESTB) {
            GpibError("HP34401 Data Lost!");
        } else {
            errmsg("SRQService(HP34401): ibrsp() Failed", ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
            GpibError(LastError);
        }
        *RearmMask = msk;
        return;
    }
    strcpy(LastError, "NoError");
    ibrd(ud, buf, sizeof(buf)-1);
    if (ThreadIbsta() & ERR) {
        errmsg("SRQService(HP34401): ibrd() Failed", ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        GpibError(LastError);
    } else {
        buf[ThreadIbcnt()] = '\0';
        x = atof(buf);
        if (x > 1.0e12) {
            x = 1.0e12;
        }
        sprintf(cmd, "%g;%g", t-time0, x);
        SendData(cmd);
    }
*/
    hp3478::rearmMask = RQS;
}


/*
bool
Hp3478::myInit() {
    ibclr(myUd);
    if (ThreadIbsta() & ERR) {
        errmsg("Connect(Hp3478): ibclr() Failed", ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        GpibError(LastError);
        return false;
    }
    sprintf(command, "F4R2N5Z1D1T4\r\n");
    gpibwrite(command);
    if (ThreadIbsta() & ERR) {
        errmsg("Connect(Hp3478): Configuration Failed", ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        GpibError(LastError);
        return false;
    }
    return true;
}


bool
Hp3478::AskVal(){
    sprintf(command, "KM01T3\r\n");
    gpibwrite(command);
    if (ThreadIbsta() & ERR) {
        errmsg("Connect(Hp3478): AskVal() Failed", ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        GpibError(LastError);
        return false;
    }
    msk = RQS;
    return true;
}


bool
Hp3478::myDisconnect() {
    sprintf(command, "F4R2N5Z1D1T1\r\n");
    gpibwrite(command);
    if (ThreadIbsta() & ERR) {
        errmsg("Connect(Hp3478): myDisconnect() Failed", ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        GpibError(LastError);
        return false;
    }
    return true;
}


void 
Hp3478::SendData(char *message) {
    pParent->SendMessage(HP3478_DATA, WPARAM(strlen(message)), LPARAM(message));
}


bool
Hp3478::myConfigure(CString sConf){
    sprintf(command, sConf);
    gpibwrite(command);
    if (ThreadIbsta() & ERR) {
        errmsg("Connect(Hp3478): myConfigure() Failed", ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        GpibError(LastError);
        return false;
    }
    return true;
}
*/
