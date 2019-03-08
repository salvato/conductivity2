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
#include "keithley236.h"

#include <QtMath>
#include <QDateTime>
#include <QThread>
//#include <QDebug>

#define MAX_COMPLIANCE_EVENTS 5

namespace keithley236 {
static int  rearmMask;
#if !defined(Q_OS_LINUX)
int __stdcall
myCallback(int LocalUd, unsigned long LocalIbsta, unsigned long LocalIberr, long LocalIbcntl, void* callbackData) {
    reinterpret_cast<Keithley236*>(callbackData)->onGpibCallback(LocalUd, LocalIbsta, LocalIberr, LocalIbcntl);
    return Keithley236::rearmMask;
}
#endif
}


Keithley236::Keithley236(int gpio, int address, QObject *parent)
    : GpibDevice(gpio, address, parent)
    //
    , ERROR_JUNCTION(-99999)
    //
    , SRQ_DISABLED(0)
    , WARNING(1)
    , SWEEP_DONE(2)
    , TRIGGER_OUT(4)
    , READING_DONE(8)
    , READY_FOR_TRIGGER(16)
    , K236_ERROR(32)
    , COMPLIANCE(128)
    //
    , isSweeping(false)
{
    iComplianceEvents = 0;
    pollInterval = 569;
}


Keithley236::~Keithley236() {
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
Keithley236::init() {
    gpibId = ibdev(gpibNumber, gpibAddress, 0, T10s, 1, 0);
    if(gpibId < 0) {
        QString sError = ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(Q_FUNC_INFO + sError);
        return GPIB_DEVICE_NOT_PRESENT;
    }
    short listen;
    ibln(gpibNumber, gpibAddress, NO_SAD, &listen);
    if(isGpibError(QString(Q_FUNC_INFO) + "Keithley 236 Not Respondig"))
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
             (GpibNotifyCallback_t) keithley236::myCallback,
             this);
    if(isGpibError(QString(Q_FUNC_INFO) + "ibnotify call failed."))
        return -1;
#endif
    ibclr(gpibId);
    QThread::sleep(1);
    return NO_ERROR;
}


int
Keithley236::initVvsTSourceI(double dAppliedCurrent, double dCompliance) {
    iComplianceEvents = 0;
    uint iErr = 0;
    iErr |= gpibWrite(gpibId, "M0,0");      // SRQ Disabled, SRQ on Compliance
    iErr |= gpibWrite(gpibId, "R0");        // Disarm Trigger
    iErr |= gpibWrite(gpibId, "O1");        // Remote Sense
    iErr |= gpibWrite(gpibId, "T1,1,0,0");  // Trigger on GET ^SRC DLY MSR
    iErr |= gpibWrite(gpibId, "F1,1X");     // Place for a moment in Source I Measure V Sweep Mode
    // For some reason the Compliance command does not
    // works when in Source I Measure V dc condition
    sCommand = QString("L%1,0X").arg(dCompliance);
    iErr |= gpibWrite(gpibId, sCommand);    // Set Compliance, Autorange Measure
    iErr |= gpibWrite(gpibId, "G5,2,0");    // Output Source, Measure, No Prefix, DC
    iErr |= gpibWrite(gpibId, "Z0");        // Disable suppression
    iErr |= gpibWrite(gpibId, "P5");        // 32 Reading Filter
    iErr |= gpibWrite(gpibId, "S3");        // 20ms integration time
    iErr |= gpibWrite(gpibId, "F1,0");      // Place in Source I Measure V
    sCommand = QString("B%1,0,0X").arg(dAppliedCurrent);
    iErr |= gpibWrite(gpibId, sCommand);    // Set Applied Current
    iErr |= gpibWrite(gpibId, "R1");        // Arm Trigger
    iErr |= gpibWrite(gpibId, "N1");        // Operate !
    if(iErr & ERR) {
        QString sError;
        sError = QString(Q_FUNC_INFO) + QString("GPIB Error in gpibWrite(): - Status= %1")
                .arg(ThreadIbsta(), 4, 16, QChar('0'));
        sError += ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(sError);
        return -1;
    }
    // Give the instrument time to execute commands
    QThread::sleep(1);
    int srqMask =
            COMPLIANCE +
            K236_ERROR +
            READY_FOR_TRIGGER +
            READING_DONE +
            WARNING;
    sCommand = QString("M%1,0X").arg(srqMask);
    gpibWrite(gpibId, sCommand);   // SRQ Mask, Interrupt on Compliance
    if(isGpibError(QString(QString(Q_FUNC_INFO) + "%1").arg(sCommand)))
        exit(-1);
    return NO_ERROR;
}


int
Keithley236::initVvsTSourceV(double dAppliedVoltage, double dCompliance) {
    iComplianceEvents = 0;
    uint iErr = 0;
    iErr |= gpibWrite(gpibId, "M0,0");      // SRQ Disabled, SRQ on Compliance
    iErr |= gpibWrite(gpibId, "R0");        // Disarm Trigger
    iErr |= gpibWrite(gpibId, "O1");        // Remote Sense
    iErr |= gpibWrite(gpibId, "T1,1,0,0");  // Trigger on GET ^SRC DLY MSR
    iErr |= gpibWrite(gpibId, "F0,1X");     // Place for a moment in Source V Measure I Sweep Mode
    // For some reason the Compliance command does not
    // works when in Source I Measure V dc condition
    sCommand = QString("L%1,0X").arg(dCompliance);
    iErr |= gpibWrite(gpibId, sCommand);    // Set Compliance, Autorange Measure
    iErr |= gpibWrite(gpibId, "F0,0");      // Source V Measure I dc
    iErr |= gpibWrite(gpibId, "G5,2,0");    // Output Source, Measure, No Prefix, DC
    iErr |= gpibWrite(gpibId, "Z0");        // Disable Zero suppression
    iErr |= gpibWrite(gpibId, "P5");        // 32 Reading Filter
    iErr |= gpibWrite(gpibId, "S3");        // 20ms integration time
    sCommand = QString("B%1,0,0X").arg(dAppliedVoltage);
    iErr |= gpibWrite(gpibId, sCommand);    // Set Applied Current
    iErr |= gpibWrite(gpibId, "R1");        // Arm Trigger
    iErr |= gpibWrite(gpibId, "N1");        // Operate !
    if(iErr & ERR) {
        QString sError;
        sError = QString(Q_FUNC_INFO) + QString("GPIB Error in gpibWrite(): - Status= %1")
                .arg(ThreadIbsta(), 4, 16, QChar('0'));
        sError += ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(sError);
        return -1;
    }
    // Give the instrument time to execute commands
    QThread::sleep(1);
    int srqMask =
            COMPLIANCE +
            K236_ERROR +
            READY_FOR_TRIGGER +
            READING_DONE +
            WARNING;
    sCommand = QString("M%1,0X").arg(srqMask);
    gpibWrite(gpibId, sCommand);   // SRQ Mask, Interrupt on Compliance
    if(isGpibError(QString(QString(Q_FUNC_INFO) + "%1").arg(sCommand)))
        exit(-1);
    return NO_ERROR;
}


int
Keithley236::endVvsT() {
#if defined(Q_OS_LINUX)
    pollTimer.stop();
    pollTimer.disconnect();
#else
    ibnotify (gpibId, 0, NULL, NULL);// disable notification
#endif
    gpibWrite(gpibId, "M0,0X");      // SRQ Disabled, SRQ on Compliance
    gpibWrite(gpibId, "R0");         // Disarm Trigger
    gpibWrite(gpibId, "N0X");        // Place in Stand By
    return NO_ERROR;
}


// Returns the Order of Magnitude Difference
// between Forward and Reverse Current
int
Keithley236::junctionCheck(double v1, double v2) {
    uint iErr = 0;
    iErr |= gpibWrite(gpibId, "M0,0X");    // SRQ Disabled, SRQ on Compliance
    iErr |= gpibWrite(gpibId, "F0,0");     // Source V Measure I dc
    iErr |= gpibWrite(gpibId, "O1");       // Remote Sense
    iErr |= gpibWrite(gpibId, "P5");       // 32 Reading Filter
    iErr |= gpibWrite(gpibId, "Z0");       // Disable suppression
    iErr |= gpibWrite(gpibId, "S3");       // 20ms integration time
    iErr |= gpibWrite(gpibId, "R0X");      // Disarm Trigger
    iErr |= gpibWrite(gpibId, "T0,1,0,0"); // Trigger on X ^SRC DLY MSR
    iErr |= gpibWrite(gpibId, "L1.0e-4,0");// Set Compliance, Autorange Measure
    iErr |= gpibWrite(gpibId, "G4,2,0");   // Output Only Measure, No Prefix, Single Line
    iErr |= gpibWrite(gpibId, "R1");       // Arm Trigger

    // Get the reverse current value
    sCommand = QString("B%1,0,1000").arg(v1, 6, 'g');
    //  qDebug() << sCommand;
    iErr |= gpibWrite(gpibId, sCommand);   // Source initial Voltage Measure I Autorange
    iErr |= gpibWrite(gpibId, "N1X");      // Operate !
    if(iErr & ERR) {
        QString sError;
        sError = QString(Q_FUNC_INFO) + QString("GPIB Error in gpibWrite(): - Status= %1")
                .arg(ThreadIbsta(), 4, 16, QChar('0'));
        sError += ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(sError);
        return ERROR_JUNCTION;
    }
    // Rischio di loop infinito
    ibrsp(gpibId, &spollByte);
    while(!(spollByte & READY_FOR_TRIGGER)) {// Ready for trigger
        QThread::msleep(100);
        ibrsp(gpibId, &spollByte);
    }
    gpibWrite(gpibId, "H0X");
    if(isGpibError(QString(Q_FUNC_INFO) + "Trigger Error"))
        return ERROR_JUNCTION;
    // Rischio di loop infinito
    ibrsp(gpibId, &spollByte);
    while(!(spollByte & READING_DONE)) {// Reading Done
        QThread::msleep(100);
        ibrsp(gpibId, &spollByte);
    }
    sResponse = gpibRead(gpibId);
    double I_Reverse = sResponse.toDouble();

    // Get the forward current value
    sCommand = QString("B%1,0,1000").arg(v2, 6, 'g');
    //  qDebug() << sCommand;
    iErr |= gpibWrite(gpibId, sCommand);   // Source final Voltage Measure I Autorange
    if(isGpibError(QString(Q_FUNC_INFO) + "Error Changing Output Voltage"))
        return ERROR_JUNCTION;
    // Rischio di loop infinito
    ibrsp(gpibId, &spollByte);
    while(!(spollByte & READY_FOR_TRIGGER)) {// Ready for trigger
        QThread::msleep(100);
        ibrsp(gpibId, &spollByte);
    }
    gpibWrite(gpibId, "H0X");
    if(isGpibError(QString(Q_FUNC_INFO) + "Trigger Error"))
        return ERROR_JUNCTION;
    // Rischio di loop infinito
    ibrsp(gpibId, &spollByte);
    while(!(spollByte & READING_DONE)) {// Reading Done
        QThread::msleep(100);
        ibrsp(gpibId, &spollByte);
    }
    sResponse = gpibRead(gpibId);
    double I_Forward = sResponse.toDouble();
    gpibWrite(gpibId, "B0.0,0,0"); // Source 0.0V Measure I Autorange
    if(isGpibError(QString(Q_FUNC_INFO) + "Zeroing Output Voltage"))
        return ERROR_JUNCTION;
    gpibWrite(gpibId, "N0X");      // Stand By !
    if(isGpibError(QString(Q_FUNC_INFO) + "Placing Keithely 236 in Standby Mode"))
        return ERROR_JUNCTION;
    if((I_Forward == 0.0) || (I_Reverse == 0.0))
        return ERROR_JUNCTION;

    //  qDebug() << I_Forward << I_Reverse;
    //  qDebug() << "Diff="
    //           << qRound(log10(fabs(I_Forward))) -
    //              qRound(log10(fabs(I_Reverse)));

    return qRound(log10(fabs(I_Forward))) -
            qRound(log10(fabs(I_Reverse)));
}


bool
Keithley236::initISweep(double startCurrent,
                        double stopCurrent,
                        double currentStep,
                        double delay,
                        double voltageCompliance) {
    uint iErr = 0;
    iErr |= gpibWrite(gpibId, "M0,0X");    // SRQ Disabled, SRQ on Compliance
    iErr |= gpibWrite(gpibId, "F1,1");     // Source I, Sweep mode
    iErr |= gpibWrite(gpibId, "O1");       // Remote Sense
    iErr |= gpibWrite(gpibId, "T1,0,0,0"); // Trigger on GET, Continuous
    sCommand = QString("L%1,0X").arg(voltageCompliance);
    iErr |= gpibWrite(gpibId, sCommand);   // Set Compliance, Autorange Measure
    iErr |= gpibWrite(gpibId, "G5,2,2");   // Output Source and Measure, No Prefix, All Lines Sweep Data
    iErr |= gpibWrite(gpibId, "Z0");       // Disable suppression
    sCommand = QString("Q1,%1,%2,%3,0,%4X")
            .arg(startCurrent)
            .arg(stopCurrent)
            .arg(qMax(currentStep, 1.0e-13))
            .arg(delay);
    iErr |= gpibWrite(gpibId, sCommand);   // Program Sweep
    if(iErr & ERR) {
        QString sError;
        sError = QString(Q_FUNC_INFO) + QString("GPIB Error in gpibWrite(): - Status= %1")
                .arg(ThreadIbsta(), 4, 16, QChar('0'));
        sError += ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(sError);
        return false;
    }
    iErr = gpibWrite(gpibId, "R1");        // Arm Trigger
    iErr |= gpibWrite(gpibId, "N1X");      // Operate !
    if(iErr & ERR) {
        QString sError;
        sError = QString(Q_FUNC_INFO) + QString("GPIB Error in gpibWrite(): - Status= %1")
                .arg(ThreadIbsta(), 4, 16, QChar('0'));
        sError += ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(sError);
        return false;
    }
    sCommand = QString("M%1,0X").arg(COMPLIANCE + SWEEP_DONE + READY_FOR_TRIGGER);
    gpibWrite(gpibId, sCommand);   // SRQ On Sweep Done
    if(isGpibError(QString(Q_FUNC_INFO) + "Error enabling SRQ Mask"))
        return false;
    isSweeping = true;
    return true;
}


bool
Keithley236::initVSweep(double startVoltage,
                        double stopVoltage,
                        double voltageStep,
                        double delay,
                        double currentCompliance) {
    uint iErr = 0;
    iErr |= gpibWrite(gpibId, "M0,0X");    // SRQ Disabled, SRQ on Compliance
    iErr |= gpibWrite(gpibId, "F0,1");     // Source V, Sweep mode
    iErr |= gpibWrite(gpibId, "O1");       // Remote Sense
    iErr |= gpibWrite(gpibId, "T1,0,0,0"); // Trigger on GET, Continuous
    sCommand = QString("L%1,0X").arg(currentCompliance);
    iErr |= gpibWrite(gpibId, sCommand);   // Set Compliance, Autorange Measure
    iErr |= gpibWrite(gpibId, "G5,2,2");   // Output Source and Measure, No Prefix, All Lines Sweep Data
    iErr |= gpibWrite(gpibId, "Z0");       // Disable suppression
    sCommand = QString("Q1,%1,%2,%3,0,%4X")
            .arg(startVoltage)
            .arg(stopVoltage)
            .arg(qMax(voltageStep, 1.0e-4))
            .arg(delay);
    iErr |= gpibWrite(gpibId, sCommand);   // Program Sweep
    if(iErr & ERR) {
        QString sError;
        sError = QString(Q_FUNC_INFO) + QString("GPIB Error in gpibWrite(): - Status= %1")
                .arg(ThreadIbsta(), 4, 16, QChar('0'));
        sError = ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(sError);
        return false;
    }
    iErr  = gpibWrite(gpibId, "R1");       // Arm Trigger
    iErr |= gpibWrite(gpibId, "N1X");      // Operate !
    if(iErr & ERR) {
        QString sError;
        sError = QString(Q_FUNC_INFO) + QString("GPIB Error in gpibWrite(): - Status= %1")
                .arg(ThreadIbsta(), 4, 16, QChar('0'));
        sError = ErrMsg(ThreadIbsta(), ThreadIberr(), ThreadIbcntl());
        emit sendMessage(sError);
        return false;
    }
    sCommand = QString("M%1,0X").arg(COMPLIANCE + SWEEP_DONE + READY_FOR_TRIGGER);
    gpibWrite(gpibId, sCommand);   // SRQ On Sweep Done
    if(isGpibError(QString(Q_FUNC_INFO) + "Error enabling SRQ Mask"))
        return false;
    isSweeping = true;
    return true;
}


int
Keithley236::stopSweep() {
#if defined(Q_OS_LINUX)
    if(pollTimer.isActive())
        pollTimer.stop();
    pollTimer.disconnect();
#else
    ibnotify (gpibId, 0, NULL, NULL);// disable notification
#endif
    gpibWrite(gpibId, "M0,0X");      // SRQ Disabled, SRQ on Compliance
    gpibWrite(gpibId, "R0");         // Disarm Trigger
    gpibWrite(gpibId, "N0X");        // Place in Stand By
    ibclr(gpibId);
    isSweeping = false;
    return NO_ERROR;
}


void
Keithley236::onGpibCallback(int LocalUd, unsigned long LocalIbsta, unsigned long LocalIberr, long LocalIbcntl) {
    Q_UNUSED(LocalIbsta)
    Q_UNUSED(LocalIberr)
    Q_UNUSED(LocalIbcntl)

    if(ibrsp(LocalUd, &spollByte) & ERR) {
        emit sendMessage(QString(Q_FUNC_INFO) + QString("GPIB error %1").arg(LocalIberr));
    }

    if(spollByte & COMPLIANCE) {// Compliance
        iComplianceEvents++;
        emit complianceEvent();
        QThread::msleep(300);
    }
    else
        emit clearCompliance();

    if(spollByte & K236_ERROR) {// Error
        gpibWrite(LocalUd, "U1X");
        sCommand = gpibRead(LocalUd);
        QString sError = QString(Q_FUNC_INFO) + QString("Error ")+ sCommand;
        emit sendMessage(sError);
    }

    if(spollByte & WARNING) {// Warning
        gpibWrite(LocalUd, "U9X");
        sCommand = gpibRead(LocalUd);
        QString sError = QString(Q_FUNC_INFO) + QString("Warning ")+ sCommand;
        emit sendMessage(sError);
    }

    if(spollByte & SWEEP_DONE) {// Sweep Done
        QDateTime currentTime = QDateTime::currentDateTime();
        QString sString = gpibRead(LocalUd);
        keithley236::rearmMask = RQS;
        emit sweepDone(currentTime, sString);
        return;
    }

    if(spollByte & TRIGGER_OUT) {// Trigger Out
        QString sError = QString(Q_FUNC_INFO) + QString("Trigger Out ?");
        emit sendMessage(sError);
    }

    if(spollByte & READY_FOR_TRIGGER) {// Ready for trigger
        emit readyForTrigger();
    }

    if((spollByte & READING_DONE) && !isSweeping){// Reading Done
        sResponse = gpibRead(LocalUd);
        if(sResponse != QString()) {
            QDateTime currentTime = QDateTime::currentDateTime();
            emit newReading(currentTime, sResponse);
        }
    }

    keithley236::rearmMask = RQS;
}


bool
Keithley236::isReadyForTrigger() {
    ibrsp(gpibId, &spollByte);
    if(isGpibError(QString(Q_FUNC_INFO) + ": Error in ibrsp()"))
        return false;
    return ((spollByte & READY_FOR_TRIGGER) != 0);
}


bool
Keithley236::sendTrigger() {
    ibtrg(gpibId);
    if(isGpibError(QString(Q_FUNC_INFO) + "Trigger Error"))
        return false;
    return true;
}


void
Keithley236::checkNotify() {
#if defined(Q_OS_LINUX)
    ibrsp(gpibId, &spollByte);
    if(isGpibError(QString(Q_FUNC_INFO) + "ibrsp() Error"))
    if(!(spollByte & 64))
        return; // SRQ not enabled
    onGpibCallback(gpibId, uint(ThreadIbsta()), uint(ThreadIberr()), ThreadIbcnt());
#endif
}
