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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "k236tab.h"
#include "hp3478tab.h"
#include "ls330tab.h"
#include "cs130tab.h"
#include "filetab.h"

#include "keithley236.h"
#include "lakeshore330.h"
#include "cornerstone130.h"
#include "Hp3478.h"

#include "plot2d.h"

#include "EasterDlg.h"
#if defined(Q_PROCESSOR_ARM)
    #include "pigpiod_if2.h"// The header for using GPIO pins on Raspberry
#endif


#include <qmath.h>
#include <QMessageBox>
#include <QSettings>
#include <QFile>
#include <QThread>
#include <QLayout>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

//#define MY_DEBUG
#define MAXTIMINGS 83


static uint32_t tick0;
static uint8_t ticks[MAXTIMINGS];
static uint8_t levels[MAXTIMINGS];
static uint8_t dht22_dat[5];


void dht22Callback(int handle,
                         unsigned user_gpio,
                         unsigned level,
                         uint32_t currentTick,
                         void *userdata)
{
    // _tick is the number of microseconds since boot
    //       WARNING: this wraps around from
    //       4294967295 to 0 roughly every 72 minutes
    Q_UNUSED(handle);
    Q_UNUSED(user_gpio);
    callbackData* pUserData = reinterpret_cast<callbackData*>(userdata);
    if(pUserData->transitionCounter == 0)
        ticks[0] = 0;
    else
        ticks[pUserData->transitionCounter] = uint8_t(currentTick-tick0);
    tick0 = currentTick;
    levels[pUserData->transitionCounter] = level;
    pUserData->transitionCounter++;
    if(pUserData->transitionCounter >= MAXTIMINGS) {
        // When AM2302 is sending data to MCU, every bit's
        // transmission begin with low-voltage-level that last 50us
        uint8_t j = 0;
        for(uint8_t i=3; i<MAXTIMINGS; i+=2) {
            dht22_dat[j/8] <<= 1;
            if(ticks[i+1] > ticks[i])
                dht22_dat[j/8] |= 1;
            j++;
        }

        // verify checksum
        uint8_t sum = ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF);
        bool bChecksum = (dht22_dat[4] == sum);
        if(bChecksum) {
            pUserData->iHumidity    = dht22_dat[0]*256 + dht22_dat[1];
            pUserData->iTemperature = (dht22_dat[2] & 0x7F)*256 + dht22_dat[3];
            if((dht22_dat[2] & 0x80) != 0)
                pUserData->iTemperature *= -1;
            emit((reinterpret_cast<MainWindow*>(pUserData->pMainWindow))->dhtMeasureDone());
        }
    }
}


MainWindow::MainWindow(int iBoard, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , pOutputFile(Q_NULLPTR)
    , pLogFile(Q_NULLPTR)
    , pKeithley236(Q_NULLPTR)
    , pLakeShore330(Q_NULLPTR)
    , pCornerStone130(Q_NULLPTR)
    , pHp3478(Q_NULLPTR)
    , pPlotMeasurements(Q_NULLPTR)
    , pPlotTemperature(Q_NULLPTR)
    , pPlotRH(Q_NULLPTR)
    , pConfigureDialog(Q_NULLPTR)
    , gpioLEDpin(23) // BCM 23: pin 16 in the 40 pins GPIO connector
    , gpioDHT22pin(26) // BCM 26: pin 37 in the 40 pins GPIO connector
    // GPIO Numbers are Broadcom (BCM) numbers
    // For Raspberry Pi GPIO pin numbering see
    // https://pinout.xyz/
    //
    // +5V on pins 2 or 4 in the 40 pin GPIO connector.
    // GND on pins 6, 9, 14, 20, 25, 30, 34 or 39
    // in the 40 pin GPIO connector.
{
    // Init internal variables
    gpibBoardID                = iBoard;
    bUseMonochromator          = false;
    gpioHostHandle             =-1;
    presentMeasure             = NoMeasure;
    bRunning                   = false;
    isK236ReadyForTrigger      = false;
    isHp3478ReadyForTrigger    = false;
    maxPlotPoints              = 3000;
    wlResolution               = 5; // Wavelength steps. To be changed
    userData.pMainWindow       = reinterpret_cast<void*>(this);
    userData.iTemperature      = 0;
    userData.iHumidity         = 0;
    userData.transitionCounter = 0;
    userData.callBackId        = pigif_bad_callback;

    // Prepare message logging
    sLogFileName = QString("gpibLog.txt");
    sLogDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if(!sLogDir.endsWith(QString("/"))) sLogDir+= QString("/");
    sLogFileName = sLogDir+sLogFileName;
#ifndef MY_DEBUG
    prepareLogFile();
#endif

    // Setup User Interface
    ui->setupUi(this);
    // Remove the resize-handle in the lower right corner
    ui->statusBar->setSizeGripEnabled(false);
    // Make the size of the window fixed
    setFixedSize(size());
    setWindowIcon(QIcon("qrc:/myLogoT.png"));

    // Setup the QLineEdit styles
    sNormalStyle = ui->labelCompliance->styleSheet();
    sErrorStyle  = "QLabel { color: rgb(255, 255, 255); background: rgb(255, 0, 0); selection-background-color: rgb(128, 128, 255); }";
    sDarkStyle   = "QLabel { color: rgb(255, 255, 255); background: rgb(0, 0, 0); selection-background-color: rgb(128, 128, 255); }";
    sPhotoStyle  = "QLabel { color: rgb(0, 0, 0); background: rgb(255, 255, 0); selection-background-color: rgb(128, 128, 255); }";

    // Restore Geometry and State of the window
    QSettings settings;
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());
    thread()->setPriority(QThread::TimeCriticalPriority);
}


MainWindow::~MainWindow() {
    if(pKeithley236      != Q_NULLPTR) delete pKeithley236;
    if(pHp3478           != Q_NULLPTR) delete pHp3478;
    if(pLakeShore330     != Q_NULLPTR) delete pLakeShore330;
    if(pCornerStone130   != Q_NULLPTR) delete pCornerStone130;
    if(pPlotMeasurements != Q_NULLPTR) delete pPlotMeasurements;
    if(pPlotTemperature  != Q_NULLPTR) delete pPlotTemperature;
    if(pConfigureDialog  != Q_NULLPTR) delete pConfigureDialog;
    if(pOutputFile       != Q_NULLPTR) delete pOutputFile;
    if(pLogFile          != Q_NULLPTR) delete pLogFile;
    delete ui;
}


// ToDo: Improve !!!
void
MainWindow::closeEvent(QCloseEvent *event) {
    Q_UNUSED(event)
    QSettings settings;
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());
    if(bRunning) {
        stopTimers();
        if(pOutputFile) {
            if(pOutputFile->isOpen())
                pOutputFile->close();
            pOutputFile->deleteLater();
            pOutputFile = Q_NULLPTR;
        }
        if(pKeithley236) {
            pKeithley236->endVvsT();
        }
        if(pHp3478) {
            pHp3478->endRvsTime();
        }
        if(pLakeShore330) {
            if(pLakeShore330->isRamping())
                pLakeShore330->stopRamp();
            pLakeShore330->switchPowerOff();
        }
    }
    if(pKeithley236)    delete pKeithley236;
    if(pHp3478)         delete pHp3478;
    if(pLakeShore330)   delete pLakeShore330;
    if(pCornerStone130) delete pCornerStone130;

    pKeithley236    = Q_NULLPTR;
    pHp3478         = Q_NULLPTR;
    pLakeShore330   = Q_NULLPTR;
    pCornerStone130 = Q_NULLPTR;

#if defined(Q_PROCESSOR_ARM)
    if(gpioHostHandle >= 0)
        pigpio_stop(gpioHostHandle);
#endif
    if(pLogFile) {
        if(pLogFile->isOpen()) {
            pLogFile->flush();
        }
        pLogFile->deleteLater();
        pLogFile = Q_NULLPTR;
    }
}


void
MainWindow::stopTimers() {
    waitingTStartTimer.stop();
    stabilizingTimer.stop();
    readingTTimer.stop();
    measuringTimer.stop();
    readingDHT22Timer.stop();
    waitingTStartTimer.disconnect();
    stabilizingTimer.disconnect();
    readingTTimer.disconnect();
    measuringTimer.disconnect();
    readingDHT22Timer.disconnect();
}


/*!
 * \brief MyApplication::prepareLogFile Prepare a log file for the session log
 * \return true
 */
bool
MainWindow::prepareLogFile() {
    // Rotate 5 previous logs, removing the oldest, to avoid data loss
    QFileInfo checkFile(sLogFileName);
    if(checkFile.exists() && checkFile.isFile()) {
        QDir renamed;
        renamed.remove(sLogFileName+QString("_4.txt"));
        for(int i=4; i>0; i--) {
            renamed.rename(sLogFileName+QString("_%1.txt").arg(i-1),
                           sLogFileName+QString("_%1.txt").arg(i));
        }
        renamed.rename(sLogFileName,
                       sLogFileName+QString("_0.txt"));
    }
    // Open the new log file
    pLogFile = new QFile(sLogFileName);
    if (!pLogFile->open(QIODevice::WriteOnly)) {
        QMessageBox::information(Q_NULLPTR, "Conductivity",
                                 QString("Unable to open file %1: %2.")
                                 .arg(sLogFileName).arg(pLogFile->errorString()));
        delete pLogFile;
        pLogFile = Q_NULLPTR;
    }
    return true;
}


/*!
 * \brief logMessage Log messages on a file (if enabled) or on stdout
 * \param logFile The file where to write the log
 * \param sFunctionName The Function which requested to write the message
 * \param sMessage The informative message
 */
void
MainWindow::logMessage(QString sMessage) {
    QDateTime dateTime;
    QString sDebugMessage = dateTime.currentDateTime().toString() +
                            QString(" - ") +
                            sMessage;
    if(pLogFile) {
        if(pLogFile->isOpen()) {
            pLogFile->write(sDebugMessage.toUtf8().data());
            pLogFile->write("\n");
            pLogFile->flush();
        }
        else
            qDebug() << sDebugMessage;
    }
    else
        qDebug() << sDebugMessage;
}


bool
MainWindow::checkInstruments() {
    ui->statusBar->showMessage("Checking for the GPIB Instruments");
    Addr4882_t padlist[31];
    Addr4882_t resultlist[31];
    for(uint16_t i=0; i<30; i++) padlist[i] = i+1;
    padlist[30] = NOADDR;
    // Resets the GPIB bus by asserting the 'interface clear' bus line
    SendIFC(gpibBoardID);
    if(ThreadIbsta() & ERR) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(QString(Q_FUNC_INFO));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QString("SendIFC() Error"));
        msgBox.setInformativeText(QString("Is the GPIB Interface connected ? "));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        int ret = msgBox.exec();
        Q_UNUSED(ret)
        return false;
    }
    // Enable assertion of REN when System Controller
    // Required by the Keithley 236
    ibconfig(gpibBoardID, IbcSRE, 1);
    if(ThreadIbsta() & ERR) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(QString(Q_FUNC_INFO));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QString("ibconfig() Error"));
        msgBox.setInformativeText(QString("Unable to set REN When SC"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        int ret = msgBox.exec();
        Q_UNUSED(ret)
        return false;
    }
    // If addrlist contains only the constant NOADDR,
    // the Universal Device Clear (DCL) message is sent
    // to all the devices on the bus
    Addr4882_t addrlist;
    addrlist = NOADDR;
    DevClearList(gpibBoardID, &addrlist);
    if(ThreadIbsta() & ERR) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(QString(Q_FUNC_INFO));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QString("DevClearList() failed"));
        msgBox.setInformativeText(QString("Are the Instruments Connected and Switched On ?"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        int ret = msgBox.exec();
        Q_UNUSED(ret)
        return false;
    }
    // Find all the instruments connected to the GPIB Bus
    FindLstn(gpibBoardID, padlist, resultlist, 30);
    if(ThreadIbsta() & ERR) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(QString(Q_FUNC_INFO));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(QString("FindLstn() failed"));
        msgBox.setInformativeText(QString("Are the Instruments Connected and Switched On ?"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        int ret = msgBox.exec();
        Q_UNUSED(ret)
        return false;
    }
    int nDevices = ThreadIbcnt();
#if defined(MY_DEBUG)
    logMessage(QString("Found %1 Instruments connected to the GPIB Bus").arg(nDevices));
#endif
    // Identify the instruments connected to the GPIB Bus
    QString sCommand, sInstrumentID;
    // Identify the instruments connected to the GPIB Bus
    char readBuf[257];
    int cornerstoneId = 0;
    // Check for the monochromator...
    sCommand = "INFO?\r\n";
    for(int i=0; i<nDevices; i++) {
        Send(gpibBoardID, resultlist[i], sCommand.toUtf8().constData(), sCommand.length(), DABend);
        Receive(gpibBoardID, resultlist[i], readBuf, 256, 0x0A);
        readBuf[ThreadIbcnt()] = '\0';
        sInstrumentID = QString(readBuf);
#if defined(MY_DEBUG)
        logMessage(QString("Address= %1 - InstrumentID= %2")
                            .arg(resultlist[i])
                            .arg(sInstrumentID));
#endif
        if(sInstrumentID.contains("Cornerstone 130", Qt::CaseInsensitive)) {
            cornerstoneId = resultlist[i];
            if(pCornerStone130 == Q_NULLPTR) {
                pCornerStone130 = new CornerStone130(gpibBoardID, resultlist[i], this);
                connect(pCornerStone130, SIGNAL(sendMessage(QString)),
                        this, SLOT(onLogMessage(QString)));
            }
            break;
        }
    }
    bUseMonochromator = pCornerStone130 != Q_NULLPTR;

    // Check for the temperature controller...
    sCommand = "*IDN?\r\n";
    int lakeShoreID = 0;
    for(int i=0; i<nDevices; i++) {
        if(resultlist[i] == cornerstoneId) continue;
        DevClear(gpibBoardID, resultlist[i]);
        Send(gpibBoardID, resultlist[i], sCommand.toUtf8().constData(), sCommand.length(), DABend);
        Receive(gpibBoardID, resultlist[i], readBuf, 256, STOPend);
        readBuf[ThreadIbcnt()] = '\0';
        sInstrumentID = QString(readBuf);
#if defined(MY_DEBUG)
        logMessage(QString("Address= %1 - InstrumentID= %2")
                    .arg(resultlist[i])
                    .arg(sInstrumentID));
#endif
        if(sInstrumentID.contains("MODEL330", Qt::CaseInsensitive)) {
            lakeShoreID = resultlist[i];
            if(pLakeShore330 == Q_NULLPTR) {
                pLakeShore330 = new LakeShore330(gpibBoardID, resultlist[i], this);
                connect(pLakeShore330, SIGNAL(sendMessage(QString)),
                        this, SLOT(onLogMessage(QString)));
            }
            break;
        }
    }
    bUseLakeShore330 = pLakeShore330 != Q_NULLPTR;

    // Check for the Keithley 236
    sCommand = "U0X";
    int Keithley236ID = 0;
    for(int i=0; i<nDevices; i++) {
        if(resultlist[i] == cornerstoneId) continue;
        if(resultlist[i] == lakeShoreID) continue;
        DevClear(gpibBoardID, resultlist[i]);
        Send(gpibBoardID, resultlist[i], sCommand.toUtf8().constData(), sCommand.length(), DABend);
        Receive(gpibBoardID, resultlist[i], readBuf, 256, STOPend);
        readBuf[ThreadIbcnt()] = '\0';
        sInstrumentID = QString(readBuf);
#if defined(MY_DEBUG)
        logMessage(QString("Address= %1 - InstrumentID= %2")
                   .arg(resultlist[i])
                   .arg(sInstrumentID));
#endif
        if(sInstrumentID.contains("236", Qt::CaseInsensitive)) {
            Keithley236ID = resultlist[i];
            if(pKeithley236 == Q_NULLPTR) {
                pKeithley236 = new Keithley236(gpibBoardID, resultlist[i], this);
                connect(pKeithley236, SIGNAL(sendMessage(QString)),
                        this, SLOT(onLogMessage(QString)));
            }
            break;
        }
    }
    bUseKeithley236 = pKeithley236 != Q_NULLPTR;
    if(!bUseKeithley236) {// If connected, it is the instrument we will use !
        // Check for the Hp 3478A
        sCommand = "S";// Read the Front/Rear switch status (hopefully understood only by the HP3478A)
        for(int i=0; i<nDevices; i++) {
            if(resultlist[i] == cornerstoneId) continue;
            if(resultlist[i] == lakeShoreID) continue;
            if(resultlist[i] == Keithley236ID) continue;
            DevClear(gpibBoardID, resultlist[i]);
            Send(gpibBoardID, resultlist[i], sCommand.toUtf8().constData(), sCommand.length(), DABend);
            Receive(gpibBoardID, resultlist[i], readBuf, 256, STOPend);
            readBuf[ThreadIbcnt()] = '\0';
            sInstrumentID = QString(readBuf);
    #if defined(MY_DEBUG)
            logMessage(QString("Address= %1 - InstrumentID= %2")
                       .arg(resultlist[i])
                       .arg(sInstrumentID));
    #endif
            if((sInstrumentID.left(1) == QString("0")) ||
               (sInstrumentID.left(1) == QString("1")))
            {
                if(pHp3478 == Q_NULLPTR) {
                    pHp3478 = new Hp3478(gpibBoardID, resultlist[i], this);
                    connect(pHp3478, SIGNAL(sendMessage(QString)),
                            this, SLOT(onLogMessage(QString)));
                }
                break;
            }
        }
    }// if(!bUseKeithley236)
    bUseHp3478 = pHp3478 != Q_NULLPTR;

    // Initialize the GPIO handler
#if defined(Q_PROCESSOR_ARM)
    gpioHostHandle = pigpio_start((char*)"localhost", (char*)"8888");
    if(gpioHostHandle >= 0) {
        if(set_mode(gpioHostHandle, gpioLEDpin, PI_OUTPUT) < 0) {
            ui->statusBar->showMessage(QString("Unable to initialize GPIO%1 as Output")
                                       .arg(gpioLEDpin));
            gpioHostHandle = -1;
        }
        else if(set_pull_up_down(gpioHostHandle, gpioLEDpin, PI_PUD_UP) < 0) {
            ui->statusBar->showMessage(QString("Unable to set GPIO%1 Pull-Up")
                                       .arg(gpioLEDpin));
            gpioHostHandle = -1;
        }
    }
    else {
        ui->statusBar->showMessage(QString("Unable to initialize the Pi GPIO."));
    }
#endif
    bUseGpio = gpioHostHandle >= 0;
    bDHT22Present = bUseGpio;

    switchLampOff();
    ui->statusBar->showMessage("GPIB Instruments Found! Ready to Start");
    return true;
}


void
MainWindow::updateUserInterface() {
    if(!bUseKeithley236) {
        ui->startIvsVButton->hide();
        ui->labelCompliance->hide();
        ui->labelVoltage->hide();
        ui->voltageEdit->hide();
        ui->labelV->hide();
        if(bUseHp3478) {
            ui->labelA->setText(QString("Ω"));
            ui->labelCurrent->setText(QString("Resistance"));
        }
        else {
            ui->startRvsTimeButton->hide();
            ui->labelCurrent->hide();
            ui->currentEdit->hide();
            ui->labelA->hide();
        }
    }
    if(!bUseMonochromator) {
        ui->lambdaScanButton->hide();
        ui->labelLambda->hide();
        ui->labelWavelength->hide();
        ui->wavelengthEdit->hide();
    }
    if(!bUseLakeShore330) {
        ui->startRvsTButton->hide();
        ui->temperatureEdit->hide();
        ui->labelTemperature->hide();
        ui->labelK->hide();
    }
    if(!bUseGpio && !bUseMonochromator) {
        ui->lampButton->hide();
        ui->labelPhoto->hide();
    }
}


void
MainWindow::switchLampOn() {
    ui->labelPhoto->setStyleSheet(sPhotoStyle);
    ui->labelPhoto->setText("Photo");
// For the moment don't use the Monochromator internal shutter
//    if(bUseMonochromator)
//        pCornerStone130->openShutter();
#if defined(Q_PROCESSOR_ARM)
    if(gpioHostHandle >= 0)
        gpio_write(gpioHostHandle, gpioLEDpin, 1);
    else
        ui->statusBar->showMessage(QString("Unable to set GPIO%1 On")
                                   .arg(gpioLEDpin));
#endif
    currentLampStatus = LAMP_ON;
    ui->lampButton->setText(QString("Lamp Off"));
}


void
MainWindow::switchLampOff() {
    ui->labelPhoto->setStyleSheet(sDarkStyle);
    ui->labelPhoto->setText("Dark");
// For the moment don't use the Monochromator internal shutter
//    if(bUseMonochromator)
//        pCornerStone130->closeShutter();
#if defined(Q_PROCESSOR_ARM)
    if(gpioHostHandle >= 0)
        gpio_write(gpioHostHandle, gpioLEDpin, 0);
    else
        ui->statusBar->showMessage(QString("Unable to set GPIO%1 Off")
                                   .arg(gpioLEDpin));
#endif
    currentLampStatus = LAMP_OFF;
    ui->lampButton->setText(QString("Lamp On"));
}


void
MainWindow::stopRvsT() {
    bRunning = false;
    presentMeasure = NoMeasure;
    stopTimers();
    if(pOutputFile) {
        pOutputFile->close();
        pOutputFile->deleteLater();
        pOutputFile = Q_NULLPTR;
    }
    if(pKeithley236 != Q_NULLPTR) {
        pKeithley236->disconnect();
        pKeithley236->endVvsT();
    }
    if(pLakeShore330 != Q_NULLPTR) {
        if(pLakeShore330->isRamping())
            pLakeShore330->stopRamp();
        pLakeShore330->disconnect();
        pLakeShore330->switchPowerOff();
    }
    switchLampOff();

    ui->endTimeEdit->clear();
    ui->startRvsTButton->setText("Start R vs T");
    ui->startRvsTimeButton->setEnabled(true);
    ui->startIvsVButton->setEnabled(true);
    ui->lambdaScanButton->setEnabled(true);
    ui->lampButton->setEnabled(true);
    QApplication::restoreOverrideCursor();
}


void
MainWindow::on_startRvsTButton_clicked() {
    if(ui->startRvsTButton->text().contains("Stop")) {
        stopRvsT();
        ui->statusBar->showMessage("Measure (R vs T) Halted");
        return;
    }
    // else
    if(pConfigureDialog) delete pConfigureDialog;
    pConfigureDialog = new ConfigureDialog(iConfRvsT, this);
    if(pConfigureDialog->exec() == QDialog::Rejected)
        return;
    QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
    if(bUseMonochromator) {
        //Initializing Corner Stone 130
        ui->statusBar->showMessage("Initializing Corner Stone 130...");
        if(pCornerStone130->init() != pCornerStone130->NO_ERROR){
            ui->statusBar->showMessage("Unable to Initialize Corner Stone 130...");
            QApplication::restoreOverrideCursor();
            return;
        }
        pCornerStone130->setGrating(pConfigureDialog->pTabCS130->iGratingNumber);
        pCornerStone130->setWavelength(pConfigureDialog->pTabCS130->dWavelength);
        ui->wavelengthEdit->setText(QString("%1")
                                    .arg(pConfigureDialog->pTabCS130->dWavelength, 10, 'f', 1, ' '));
    }
    switchLampOff();
    // Initializing Keithley 236
    ui->statusBar->showMessage("Initializing Keithley 236...");
    if(pKeithley236->init()) {
        ui->statusBar->showMessage("Unable to Initialize Keithley 236...");
        QApplication::restoreOverrideCursor();
        return;
    }
    isK236ReadyForTrigger = false;
    connect(pKeithley236, SIGNAL(complianceEvent()),
            this, SLOT(onComplianceEvent()));
    connect(pKeithley236, SIGNAL(clearCompliance()),
            this, SLOT(onClearComplianceEvent()));
    connect(pKeithley236, SIGNAL(readyForTrigger()),
            this, SLOT(onKeithleyReadyForTrigger()));
    connect(pKeithley236, SIGNAL(newReading(QDateTime, QString)),
            this, SLOT(onNewRvsTKeithleyReading(QDateTime, QString)));
    // Initializing LakeShore 330
    ui->statusBar->showMessage("Initializing LakeShore 330...");
    if(pLakeShore330->init()) {
        ui->statusBar->showMessage("Unable to Initialize LakeShore 330...");
        pKeithley236->disconnect();
        QApplication::restoreOverrideCursor();
        return;
    }
    // Open the Output file
    ui->statusBar->showMessage("Opening Output file...");
    if(!prepareOutputFile(pConfigureDialog->pTabFile->sBaseDir,
                          pConfigureDialog->pTabFile->sOutFileName))
    {
        ui->statusBar->showMessage("Unable to Open the Output file...");
        pKeithley236->disconnect();
        QApplication::restoreOverrideCursor();
        return;
    }
    writeRvsTHeader();
    // Init the Plots
    initRvsTPlots();
    // Configure Thermostat
    pLakeShore330->setTemperature(pConfigureDialog->pTabLS330->dTStart);
    pLakeShore330->switchPowerOn(3);
    // Configure Source-Measure Unit
    double dCompliance = pConfigureDialog->pTabK236->dCompliance;
    if(pConfigureDialog->pTabK236->bSourceI) {
        presentMeasure = RvsTSourceI;
        double dAppliedCurrent = pConfigureDialog->pTabK236->dStart;
        pKeithley236->initVvsTSourceI(dAppliedCurrent, dCompliance);
    }
    else {
        presentMeasure = RvsTSourceV;
        double dAppliedVoltage = pConfigureDialog->pTabK236->dStart;
        pKeithley236->initVvsTSourceV(dAppliedVoltage, dCompliance);
    }
    // Configure the needed timers
    connect(&waitingTStartTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToCheckReachedT()));
    connect(&readingTTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToReadT()));
    if(bDHT22Present) {
        read_dht22();
        connect(&readingDHT22Timer, SIGNAL(timeout()),
                this, SLOT(onTimeToReadHumidity()));
        readingDHT22Timer.start(30000);
    }
    waitingTStartTime = QDateTime::currentDateTime();
    // Read and plot initial value of Temperature
    startReadingTTime = waitingTStartTime;
    onTimeToReadT();
    readingTTimer.start(30000);
    // All done... compute the time needed for the measurement:
    startMeasuringTime = QDateTime::currentDateTime();
    double deltaT, expectedMinutes;
    deltaT = fabs(pConfigureDialog->pTabLS330->dTStop -
             pConfigureDialog->pTabLS330->dTStart);
    expectedMinutes = deltaT / pConfigureDialog->pTabLS330->dTRate +
                      pConfigureDialog->pTabLS330->iReachingTStart +
                      pConfigureDialog->pTabLS330->iTimeToSteadyT;
    endMeasureTime = startMeasuringTime.addSecs(qint64(expectedMinutes*60.0));
    QString sString = endMeasureTime.toString("hh:mm dd-MM-yyyy");
    ui->endTimeEdit->setText(sString);
    // now we are waiting for reaching the initial temperature
    ui->startRvsTimeButton->setDisabled(true);
    ui->startIvsVButton->setDisabled(true);
    ui->startRvsTButton->setText("Stop R vs T");
    ui->lambdaScanButton->setDisabled(true);
    ui->lampButton->setDisabled(true);
    ui->statusBar->showMessage(QString("%1 Waiting Initial T [%2K]")
                               .arg(waitingTStartTime.toString())
                               .arg(pConfigureDialog->pTabLS330->dTStart));
    // Start the reaching of the Initial Temperature
    waitingTStartTimer.start(5000);
    dateStart = QDateTime::currentDateTime();
}


void
MainWindow::writeRvsTHeader() {
    // Write the header
    // To cope with the GnuPlot way to handle the comment lines
    // we need a # as a first chraracter in each row.
    pOutputFile->write(QString("%1 %2 %3 %4 %5 %6 %7")
                       .arg("#T-Dark[K]", 12)
                       .arg("V-Dark[V]", 12)
                       .arg("I-Dark[A]", 12)
                       .arg("T-Photo[K]", 12)
                       .arg("V-Photo[V]", 12)
                       .arg("I-Photo[A]\n", 12)
                       .arg("RH[%]\n", 12)
                       .toLocal8Bit());
    QStringList HeaderLines = pConfigureDialog->pTabFile->sSampleInfo.split("\n");
    for(int i=0; i<HeaderLines.count(); i++) {
        pOutputFile->write("# ");
        pOutputFile->write(HeaderLines.at(i).toLocal8Bit());
        pOutputFile->write("\n");
    }
    if(bUseMonochromator) {
        pOutputFile->write(QString("# Grating #= %1 Wavelength = %2 nm\n")
                                   .arg(pConfigureDialog->pTabCS130->iGratingNumber)
                                   .arg(pConfigureDialog->pTabCS130->dWavelength).toLocal8Bit());
    }
    if(pConfigureDialog->pTabK236->bSourceI) {
        pOutputFile->write(QString("# Current=%1[A] Compliance=%2[V]\n")
                           .arg(pConfigureDialog->pTabK236->dStart)
                           .arg(pConfigureDialog->pTabK236->dCompliance).toLocal8Bit());
    }
    else {
        pOutputFile->write(QString("# Voltage=%1[V] Compliance=%2[A]\n")
                           .arg(pConfigureDialog->pTabK236->dStart)
                           .arg(pConfigureDialog->pTabK236->dCompliance).toLocal8Bit());
    }
    pOutputFile->write(QString("# T_Start=%1[K] T_Stop=%2[K] T_Rate=%3[K/min]\n")
                       .arg(pConfigureDialog->pTabLS330->dTStart)
                       .arg(pConfigureDialog->pTabLS330->dTStop)
                       .arg(pConfigureDialog->pTabLS330->dTRate).toLocal8Bit());
    pOutputFile->write(QString("# Max_T_Start_Wait=%1[min] T_Stabilize_Time=%2[min]\n")
                       .arg(pConfigureDialog->pTabLS330->iReachingTStart)
                       .arg(pConfigureDialog->pTabLS330->iTimeToSteadyT).toLocal8Bit());
    pOutputFile->flush();
}


void
MainWindow::stopRvsTime() {
    bRunning = false;
    presentMeasure = NoMeasure;
    stopTimers();
    if(pOutputFile) {
        pOutputFile->close();
        pOutputFile->deleteLater();
        pOutputFile = Q_NULLPTR;
    }
    if(pKeithley236 != Q_NULLPTR) {
        pKeithley236->disconnect();
        pKeithley236->endVvsTime();
    }
    if(pHp3478 != Q_NULLPTR) {
        pHp3478->disconnect();
        pHp3478->endRvsTime();
    }
    if(pLakeShore330 != Q_NULLPTR) {
        if(pLakeShore330->isRamping())
            pLakeShore330->stopRamp();
        pLakeShore330->disconnect();
        pLakeShore330->switchPowerOff();
    }
    switchLampOff();

    ui->endTimeEdit->clear();
    ui->startRvsTimeButton->setText("Start R vs Time");
    ui->startRvsTButton->setEnabled(true);
    ui->startIvsVButton->setEnabled(true);
    ui->lambdaScanButton->setEnabled(true);
    ui->lampButton->setEnabled(true);
    QApplication::restoreOverrideCursor();
}


void
MainWindow::on_startRvsTimeButton_clicked() {
    if(ui->startRvsTimeButton->text().contains("Stop")) {
        stopRvsTime();
        ui->statusBar->showMessage("Measure (R vs Time) Halted");
        return;
    }
    // else
    if(pConfigureDialog) delete pConfigureDialog;
    pConfigureDialog = new ConfigureDialog(iConfRvsTime, this);
    if(pConfigureDialog->exec() == QDialog::Rejected)
        return;
    QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
//    switchLampOff();

    if(pKeithley236) {// Initializing Keithley 236
        ui->statusBar->showMessage("Initializing Keithley 236...");
        if(pKeithley236->init()) {
            ui->statusBar->showMessage("Unable to Initialize Keithley 236...");
            QApplication::restoreOverrideCursor();
            return;
        }
        isK236ReadyForTrigger = false;
        connect(pKeithley236, SIGNAL(complianceEvent()),
                this, SLOT(onComplianceEvent()));
        connect(pKeithley236, SIGNAL(clearCompliance()),
                this, SLOT(onClearComplianceEvent()));
        connect(pKeithley236, SIGNAL(readyForTrigger()),
                this, SLOT(onKeithleyReadyForTrigger()));
        connect(pKeithley236, SIGNAL(newReading(QDateTime, QString)),
                this, SLOT(onNewRvsTimeKeithleyReading(QDateTime, QString)));
    }// if(pKeithley236)

    if(pHp3478) {// Initializing HP3478A
        ui->statusBar->showMessage("Initializing HP 3478A...");
        if(pHp3478->init()) {
            ui->statusBar->showMessage("Unable to Initialize HP 3478A...");
            QApplication::restoreOverrideCursor();
            return;
        }
        isHp3478ReadyForTrigger = false;
        connect(pHp3478, SIGNAL(readyForTrigger()),
                this, SLOT(onHp3478ReadyForTrigger()));
        connect(pHp3478, SIGNAL(newReading(QDateTime, QString)),
                this, SLOT(onNewRvsTimeHp3478Reading(QDateTime, QString)));
    }
    if(pLakeShore330) {
        // Initializing LakeShore 330
        ui->statusBar->showMessage("Initializing LakeShore 330...");
        if(pLakeShore330->init()) {
            ui->statusBar->showMessage("Unable to Initialize LakeShore 330...");
            pKeithley236->disconnect();
            QApplication::restoreOverrideCursor();
            return;
        }
    }

    // Open the Output file
    ui->statusBar->showMessage("Opening Output file...");
    if(!prepareOutputFile(pConfigureDialog->pTabFile->sBaseDir,
                          pConfigureDialog->pTabFile->sOutFileName))
    {
        ui->statusBar->showMessage("Unable to Open the Output file...");
        pKeithley236->disconnect();
        QApplication::restoreOverrideCursor();
        return;
    }
    writeRvsTimeHeader();

    // Init the Plots
    initRvsTimePlots();

    double timeBetweenMeasurements = 10.0;
    if(pKeithley236) {
        // Configure Source-Measure Unit
        double dCompliance = pConfigureDialog->pTabK236->dCompliance;
        if(pConfigureDialog->pTabK236->bSourceI) {
            presentMeasure = RvsTimeSourceI;
            double dAppliedCurrent = pConfigureDialog->pTabK236->dStart;
            pKeithley236->initVvsTSourceI(dAppliedCurrent, dCompliance);
        }
        else {
            presentMeasure = RvsTimeSourceV;
            double dAppliedVoltage = pConfigureDialog->pTabK236->dStart;
            pKeithley236->initVvsTSourceV(dAppliedVoltage, dCompliance);
        }
        timeBetweenMeasurements = pConfigureDialog->pTabK236->dInterval*1000.0;
        connect(&measuringTimer, SIGNAL(timeout()),
                this, SLOT(onTimeToGetNewK236Measure()));
    }

    if(pHp3478) {
        // Configure Multimeter
        presentMeasure = RvsTimeSourceI;
        pHp3478->initRvsTime();
        timeBetweenMeasurements = pConfigureDialog->pTabHp3478->dInterval*1000.0;
        connect(&measuringTimer, SIGNAL(timeout()),
                this, SLOT(onTimeToGetNewHp3478Measure()));
    }

    // Configure the needed timers
    if(bDHT22Present) {
        connect(this, SIGNAL(dhtMeasureDone()),
                this, SLOT(onNewRHdata()));
        connect(&readingDHT22Timer, SIGNAL(timeout()),
                this, SLOT(onTimeToReadHumidity()));
        read_dht22();
        readingDHT22Timer.start(30000);
    }
    if(pLakeShore330) {
//        connect(&readingTTimer, SIGNAL(timeout()),
//                this, SLOT(onTimeToReadT()));
//        // Read and plot initial value of Temperature
//        startReadingTTime = QDateTime::currentDateTime();
//        onTimeToReadT();
//        readingTTimer.start(30000);
        if(pConfigureDialog->pTabLS330->bUseThermostat) {
            pLakeShore330->setTemperature(pConfigureDialog->pTabLS330->dTStart);
            pLakeShore330->switchPowerOn(3);
            if(!pLakeShore330->startRamp(pConfigureDialog->pTabLS330->dTStop, pConfigureDialog->pTabLS330->dTRate)) {
                ui->statusBar->showMessage(QString("Error Starting the Measure"));
                return;
            }
        }
    }
    ui->startRvsTButton->setDisabled(true);
    ui->startIvsVButton->setDisabled(true);
    ui->startRvsTimeButton->setText("Stop R vs Time");
    ui->lambdaScanButton->setDisabled(true);
    ui->lampButton->setDisabled(true);
    bRunning = true;
    // Start the measuring cycle
    measuringTimer.start(int(timeBetweenMeasurements));
    dateStart = QDateTime::currentDateTime();
    ui->statusBar->showMessage(QString("%1 Measure started")
                               .arg(dateStart.toString()));
}


void
MainWindow::writeRvsTimeHeader() {
    // Write the header
    // To cope with the GnuPlot way to handle the comment lines
    // we need a # as a first chraracter in each row.
    if(bUseKeithley236) {
        pOutputFile->write(QString("%1 %2 %3 %4 %5\n")
                           .arg("#Time[s]", 12)
                           .arg("V[V]", 12)
                           .arg("I[A]", 12)
                           .arg("T[K]", 12)
                           .arg("RH[%]", 12)
                           .toLocal8Bit());
        if(pConfigureDialog->pTabK236->bSourceI) {
            pOutputFile->write(QString("# Current=%1[A] Compliance=%2[V]\n")
                               .arg(pConfigureDialog->pTabK236->dStart)
                               .arg(pConfigureDialog->pTabK236->dCompliance).toLocal8Bit());
        }
        else {
            pOutputFile->write(QString("# Voltage=%1[V] Compliance=%2[A]\n")
                               .arg(pConfigureDialog->pTabK236->dStart)
                               .arg(pConfigureDialog->pTabK236->dCompliance).toLocal8Bit());
        }
    }
    else if(bUseHp3478) {
        pOutputFile->write(QString("%1 %2 %3 %4\n")
                           .arg("#Time[s]", 12)
                           .arg("R[Ohm]", 12)
                           .arg("T[K]", 12)
                           .arg("RH[%]", 12)
                           .toLocal8Bit());
    }

    QStringList HeaderLines = pConfigureDialog->pTabFile->sSampleInfo.split("\n");
    for(int i=0; i<HeaderLines.count(); i++) {
        pOutputFile->write("# ");
        pOutputFile->write(HeaderLines.at(i).toLocal8Bit());
        pOutputFile->write("\n");
    }
    pOutputFile->flush();
}


void
MainWindow::on_startIvsVButton_clicked() {
    if(ui->startIvsVButton->text().contains("Stop")) {
        stopIvsV();
        ui->statusBar->showMessage("Measure (I vs V) Halted");
        return;
    }
    //else
    if(pConfigureDialog) delete pConfigureDialog;
    pConfigureDialog = new ConfigureDialog(iConfIvsV, this);
    if(pConfigureDialog->exec() == QDialog::Rejected)
        return;
    QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
    //Initializing Corner Stone 130
    if(bUseMonochromator) {
        ui->statusBar->showMessage("Initializing Corner Stone 130...");
        if(pCornerStone130->init() != pCornerStone130->NO_ERROR) {
            ui->statusBar->showMessage("Unable to Initialize Corner Stone 130...");
            QApplication::restoreOverrideCursor();
            return;
        }
        pCornerStone130->setGrating(pConfigureDialog->pTabCS130->iGratingNumber);
        pCornerStone130->setWavelength(pConfigureDialog->pTabCS130->dWavelength);
        ui->wavelengthEdit->setText(QString("%1")
                                    .arg(pConfigureDialog->pTabCS130->dWavelength, 10, 'f', 1, ' '));
    }
    if(pConfigureDialog->pTabCS130) {
        if(pConfigureDialog->pTabCS130->bPhoto)
            switchLampOn();
        else
            switchLampOff();
    }
    // Initializing Keithley 236
    ui->statusBar->showMessage("Initializing Keithley 236...");
    if(pKeithley236->init()) {
        ui->statusBar->showMessage("Unable to Initialize Keithley 236...");
        stopIvsV();
        return;
    }
    isK236ReadyForTrigger = false;
    connect(pKeithley236, SIGNAL(complianceEvent()),
            this, SLOT(onComplianceEvent()));
    connect(pKeithley236, SIGNAL(clearCompliance()),
            this, SLOT(onClearComplianceEvent()));
    connect(pKeithley236, SIGNAL(readyForTrigger()),
            this, SLOT(onKeithleyReadyForSweepTrigger()));
    if(pLakeShore330) {
        // Initializing LakeShore 330
        ui->statusBar->showMessage("Initializing LakeShore 330...");
        if(pLakeShore330->init()) {
            ui->statusBar->showMessage("Unable to Initialize LakeShore 330...");
            pKeithley236->disconnect();
            stopIvsV();
            return;
        }
    }
    // Open the Output file
    ui->statusBar->showMessage("Opening Output file...");
    if(!prepareOutputFile(pConfigureDialog->pTabFile->sBaseDir,
                          pConfigureDialog->pTabFile->sOutFileName))
    {
        stopIvsV();
        return;
    }
    // Write IvsV  File Header
    writeIvsVHeader();
    // Init the Plots
    initIvsVPlots();
    isK236ReadyForTrigger = false;
    presentMeasure = IvsV;
    connect(&readingTTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToReadT()));
    readingTTimer.start(30000);
    // Read and plot initial value of Temperature
    startReadingTTime = QDateTime::currentDateTime();
    onTimeToReadT();
    if(bDHT22Present) {
        read_dht22();
        connect(&readingDHT22Timer, SIGNAL(timeout()),
                this, SLOT(onTimeToReadHumidity()));
        readingDHT22Timer.start(30000);
    }
    double expectedSeconds;
    startMeasuringTime = QDateTime::currentDateTime();
    expectedSeconds = 0.32+pConfigureDialog->pTabK236->iWaitTime/1000.0;
    expectedSeconds *= pConfigureDialog->pTabK236->iNSweepPoints;
    if(pConfigureDialog->pTabLS330) {
        if(pConfigureDialog->pTabLS330->bUseThermostat) {
            connect(&waitingTStartTimer, SIGNAL(timeout()),
                    this, SLOT(onTimeToCheckT()));
            waitingTStartTime = QDateTime::currentDateTime();
            // Start the reaching of the Initial Temperature
            // Configure Thermostat
            setPointT = pConfigureDialog->pTabLS330->dTStart;
            pLakeShore330->setTemperature(setPointT);
            pLakeShore330->switchPowerOn(3);
            waitingTStartTimer.start(5000);
            ui->statusBar->showMessage(QString("%1 Waiting Initial T[%2K]")
                                       .arg(waitingTStartTime.toString())
                                       .arg(pConfigureDialog->pTabLS330->dTStart));
            // Adjust the time needed for the measurement:
            double deltaT;
            deltaT = fabs(pConfigureDialog->pTabLS330->dTStop -
                     pConfigureDialog->pTabLS330->dTStart);
            expectedSeconds += 60.0 *(pConfigureDialog->pTabLS330->iReachingTStart +
                                      pConfigureDialog->pTabLS330->iTimeToSteadyT);
            expectedSeconds *= int(deltaT / pConfigureDialog->pTabLS330->dTStep);
        }
    }
    else {
        startI_Vscan(pConfigureDialog->pTabK236->bSourceI);
    }
    endMeasureTime = startMeasuringTime.addSecs(qint64(expectedSeconds));
    QString sString = endMeasureTime.toString("hh:mm:ss dd-MM-yyyy");
    ui->endTimeEdit->setText(sString);
    ui->startRvsTButton->setDisabled(true);
    ui->startRvsTimeButton->setDisabled(true);
    ui->startIvsVButton->setText("Stop I vs V");
    ui->lambdaScanButton->setDisabled(true);
    ui->lampButton->setDisabled(true);
}


void
MainWindow::writeIvsVHeader() {
    // To cope with GnuPlot way to handle the comment lines
    pOutputFile->write(QString("%1 %2 %3 %4\n")
                       .arg("#Voltage[V]", 12)
                       .arg("Current[A]", 12)
                       .arg("Temp.[K]", 12)
                       .arg("RH[%]\n", 12)
                       .toLocal8Bit());
    QStringList HeaderLines = pConfigureDialog->pTabFile->sSampleInfo.split("\n");
    for(int i=0; i<HeaderLines.count(); i++) {
        pOutputFile->write("# ");
        pOutputFile->write(HeaderLines.at(i).toLocal8Bit());
        pOutputFile->write("\n");
    }
    if(pConfigureDialog->pTabK236->bSourceI) {
        pOutputFile->write(QString("# I_Start=%1[A] I_Stop=%2[A] Compliance=%3[V]\n")
                           .arg(pConfigureDialog->pTabK236->dStart)
                           .arg(pConfigureDialog->pTabK236->dStop)
                           .arg(pConfigureDialog->pTabK236->dCompliance).toLocal8Bit());
    }
    else {
        pOutputFile->write(QString("# V_Start=%1[A] V_Stop=%2[V] Compliance=%3[A]\n")
                           .arg(pConfigureDialog->pTabK236->dStart)
                           .arg(pConfigureDialog->pTabK236->dStop)
                           .arg(pConfigureDialog->pTabK236->dCompliance).toLocal8Bit());
    }
    if(pConfigureDialog->pTabLS330) {
        if(pConfigureDialog->pTabLS330->bUseThermostat) {
            pOutputFile->write(QString("# T_Start=%1[K] T_Stop=%2[K] T_Step=%3[K]\n")
                               .arg(pConfigureDialog->pTabLS330->dTStart)
                               .arg(pConfigureDialog->pTabLS330->dTStop)
                               .arg(pConfigureDialog->pTabLS330->dTStep).toLocal8Bit());
            pOutputFile->write(QString("# Max_T_Start_Wait=%1[min] T_Stabilize_Time=%2[min]\n")
                               .arg(pConfigureDialog->pTabLS330->iReachingTStart)
                               .arg(pConfigureDialog->pTabLS330->iTimeToSteadyT).toLocal8Bit());
        }
    }
    if(pConfigureDialog->pTabCS130) {
        if(pConfigureDialog->pTabCS130->bPhoto) {
            pOutputFile->write(QString("# Lamp=On\n").toLocal8Bit());
            if(bUseMonochromator) {
                pOutputFile->write(QString("# Grating #= %1 Wavelength = %2 nm\n")
                                           .arg(pConfigureDialog->pTabCS130->iGratingNumber)
                                           .arg(pConfigureDialog->pTabCS130->dWavelength).toLocal8Bit());
            }
        }
        else {
            pOutputFile->write(QString("# Lamp=Off\n").toLocal8Bit());
        }
    }
    pOutputFile->flush();
}


void
MainWindow::on_lambdaScanButton_clicked() {
    if(ui->lambdaScanButton->text().contains("Stop")) {
        stopLambdaScan();
        ui->statusBar->showMessage("Measure (Lambda Scan) Halted");
        return;
    }
    // else
    if(pConfigureDialog) delete pConfigureDialog;
    pConfigureDialog = new ConfigureDialog(iConfLScan, this);
    if(pConfigureDialog->exec() == QDialog::Rejected)
        return;
    QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
    //Initializing Corner Stone 130
    ui->statusBar->showMessage("Initializing Corner Stone 130...");
    if(pCornerStone130->init() != pCornerStone130->NO_ERROR){
        ui->statusBar->showMessage("Unable to Initialize Corner Stone 130...");
        QApplication::restoreOverrideCursor();
        return;
    }
    pCornerStone130->setGrating(pConfigureDialog->pTabCS130->iGratingNumber);
    pCornerStone130->setWavelength(pConfigureDialog->pTabCS130->dStartWavelength);
    ui->wavelengthEdit->setText(QString("%1")
                                .arg(pConfigureDialog->pTabCS130->dStartWavelength, 10, 'f', 1, ' '));
    // Switch Off the Lamp
    switchLampOff();
    // Initializing Keithley 236
    ui->statusBar->showMessage("Initializing Keithley 236...");
    if(pKeithley236->init()) {
        ui->statusBar->showMessage("Unable to Initialize Keithley 236...");
        QApplication::restoreOverrideCursor();
        return;
    }
    isK236ReadyForTrigger = false;
    connect(pKeithley236, SIGNAL(complianceEvent()),
            this, SLOT(onComplianceEvent()));
    connect(pKeithley236, SIGNAL(clearCompliance()),
            this, SLOT(onClearComplianceEvent()));
    connect(pKeithley236, SIGNAL(readyForTrigger()),
            this, SLOT(onKeithleyReadyForTrigger()));
    connect(pKeithley236, SIGNAL(newReading(QDateTime, QString)),
            this, SLOT(onNewLambdaScanKeithleyReading(QDateTime, QString)));
    // Initializing LakeShore 330
    ui->statusBar->showMessage("Initializing LakeShore 330...");
    if(pLakeShore330->init()) {
        ui->statusBar->showMessage("Unable to Initialize LakeShore 330...");
        pKeithley236->disconnect();
        QApplication::restoreOverrideCursor();
        return;
    }
    // Open the Output file
    ui->statusBar->showMessage("Opening Output file...");
    if(!prepareOutputFile(pConfigureDialog->pTabFile->sBaseDir,
                          pConfigureDialog->pTabFile->sOutFileName))
    {
        ui->statusBar->showMessage("Unable to Open the Output file...");
        QApplication::restoreOverrideCursor();
        return;
    }
    // Write the File Header
    writeLambdaScanHeader();
    // Init the Plots
    initSvsLPlots();
    // Configure Thermostat (if used)
    if(pConfigureDialog->pTabLS330->bUseThermostat) {
        pLakeShore330->setTemperature(pConfigureDialog->pTabLS330->dTStart);
        pLakeShore330->switchPowerOn(3);
    }
    // Configure Source-Measure Unit
    double dCompliance = pConfigureDialog->pTabK236->dCompliance;
    if(pConfigureDialog->pTabK236->bSourceI) {
        presentMeasure = LambdaScanI;
        double dAppliedCurrent = pConfigureDialog->pTabK236->dStart;
        pKeithley236->initVvsTSourceI(dAppliedCurrent, dCompliance);
    }
    else {
        presentMeasure = LambdaScanV;
        double dAppliedVoltage = pConfigureDialog->pTabK236->dStart;
        pKeithley236->initVvsTSourceV(dAppliedVoltage, dCompliance);
    }
    // Configure the needed timers
    if(pConfigureDialog->pTabLS330->bUseThermostat) {
        connect(&waitingTStartTimer, SIGNAL(timeout()),
                this, SLOT(onTimeToCheckReachedT()));
    }
    connect(&readingTTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToReadT()));
    if(bDHT22Present) {
        read_dht22();
        connect(&readingDHT22Timer, SIGNAL(timeout()),
                this, SLOT(onTimeToReadHumidity()));
        readingDHT22Timer.start(30000);
    }
    waitingTStartTime = QDateTime::currentDateTime();
    // Read and plot initial value of Temperature
    startReadingTTime = waitingTStartTime;
    onTimeToReadT();
    readingTTimer.start(30000);
    // All done... compute the time needed for the measurement:
    startMeasuringTime = QDateTime::currentDateTime();
    int lambdaSteps = int(fabs(pConfigureDialog->pTabCS130->dStartWavelength -
                               pConfigureDialog->pTabCS130->dStopWavelength) /
                          wlResolution+0.5);
    double expectedSeconds = lambdaSteps * (pConfigureDialog->pTabK236->dInterval + 3.0);
    if(pConfigureDialog->pTabLS330->bUseThermostat)
        expectedSeconds += 60.0*(pConfigureDialog->pTabLS330->iReachingTStart +
                                 pConfigureDialog->pTabLS330->iTimeToSteadyT);
    endMeasureTime = startMeasuringTime.addSecs(qint64(expectedSeconds));
    QString sString = endMeasureTime.toString("hh:mm dd-MM-yyyy");
    ui->endTimeEdit->setText(sString);

    ui->lambdaScanButton->setText("Stop");
    ui->startRvsTButton->setDisabled(true);
    ui->startRvsTimeButton->setDisabled(true);
    ui->startIvsVButton->setDisabled(true);
    ui->lampButton->setDisabled(true);
    if(pConfigureDialog->pTabLS330->bUseThermostat) {
        // now we must wait reaching the initial temperature
        ui->statusBar->showMessage(QString("%1 Waiting Initial T [%2K]")
                                   .arg(waitingTStartTime.toString())
                                   .arg(pConfigureDialog->pTabLS330->dTStart));
        // Start the reaching of the Initial Temperature
        waitingTStartTimer.start(5000);
    }
    else {
        double timeBetweenMeasurements = pConfigureDialog->pTabK236->dInterval*1000.0;
        connect(&measuringTimer, SIGNAL(timeout()),
                this, SLOT(onTimeToGetNewK236Measure()));
        measuringTimer.start(int(timeBetweenMeasurements));
        ui->statusBar->showMessage(QString("λ Scan Started: Please wait"));

    }
    bRunning = true;
}


void
MainWindow::writeLambdaScanHeader() {
    // Write the header
    // To cope with the GnuPlot way to handle the comment lines
    // we need a # as a first chraracter in each row.
    pOutputFile->write(QString("%1 %2 %3 %4 %5 %6 %7 %8")
                       .arg("#Wavelen[nm]", 12)
                       .arg("T-Dark[K]", 12)
                       .arg("V-Dark[V]", 12)
                       .arg("I-Dark[A]", 12)
                       .arg("T-Photo[K]", 12)
                       .arg("V-Photo[V]", 12)
                       .arg("I-Photo[A]\n", 12)
                       .arg("RH[%]\n", 12)
                       .toLocal8Bit());
    QStringList HeaderLines = pConfigureDialog->pTabFile->sSampleInfo.split("\n");
    for(int i=0; i<HeaderLines.count(); i++) {
        pOutputFile->write("# ");
        pOutputFile->write(HeaderLines.at(i).toLocal8Bit());
        pOutputFile->write("\n");
    }
    pOutputFile->write(QString("# Grating #= %1 Start Wavelen = %2 [nm] Stop Wavelen = %3 [nm]\n")
                               .arg(pConfigureDialog->pTabCS130->iGratingNumber)
                               .arg(pConfigureDialog->pTabCS130->dStartWavelength)
                               .arg(pConfigureDialog->pTabCS130->dStopWavelength).toLocal8Bit());
    if(pConfigureDialog->pTabK236->bSourceI) {
        pOutputFile->write(QString("# Current=%1[A] Compliance=%2[V]\n")
                           .arg(pConfigureDialog->pTabK236->dStart)
                           .arg(pConfigureDialog->pTabK236->dCompliance).toLocal8Bit());
    }
    else {
        pOutputFile->write(QString("# Voltage=%1[V] Compliance=%2[A]\n")
                           .arg(pConfigureDialog->pTabK236->dStart)
                           .arg(pConfigureDialog->pTabK236->dCompliance).toLocal8Bit());
    }
    pOutputFile->write(QString("# T_Start=%1[K] T_Stop=%2[K] T_Step=%3[K/min]\n")
                       .arg(pConfigureDialog->pTabLS330->dTStart)
                       .arg(pConfigureDialog->pTabLS330->dTStop)
                       .arg(pConfigureDialog->pTabLS330->dTStep).toLocal8Bit());
    pOutputFile->write(QString("# Max_T_Start_Wait=%1[min] T_Stabilize_Time=%2[min]\n")
                       .arg(pConfigureDialog->pTabLS330->iReachingTStart)
                       .arg(pConfigureDialog->pTabLS330->iTimeToSteadyT).toLocal8Bit());
    pOutputFile->flush();
}


void
MainWindow::startI_Vscan(bool bSourceI) {
    ui->statusBar->showMessage("Sweeping...Please Wait");
    double dStart = pConfigureDialog->pTabK236->dStart;
    double dStop = pConfigureDialog->pTabK236->dStop;
    int nSweepPoints = pConfigureDialog->pTabK236->iNSweepPoints;
    double dStep = qAbs(dStop - dStart) / double(nSweepPoints);
    double dDelayms = double(pConfigureDialog->pTabK236->iWaitTime);
    double dCompliance = pConfigureDialog->pTabK236->dCompliance;
    if(bSourceI) {// Source I Measure V
        presentMeasure = IvsVSourceI;
        connect(pKeithley236, SIGNAL(sweepDone(QDateTime,QString)),
                this, SLOT(onKeithleySweepDone(QDateTime,QString)));
        pKeithley236->initISweep(dStart, dStop, dStep, dDelayms, dCompliance);
    }
    else {// Source V Measure I
        presentMeasure = IvsVSourceV;
        connect(pKeithley236, SIGNAL(sweepDone(QDateTime,QString)),
                this, SLOT(onKeithleySweepDone(QDateTime,QString)));
        pKeithley236->initVSweep(dStart, dStop, dStep, dDelayms, dCompliance);
    }
}


void
MainWindow::stopIvsV() {
    presentMeasure = NoMeasure;
    if(pOutputFile) {
        pOutputFile->close();
        pOutputFile->deleteLater();
        pOutputFile = Q_NULLPTR;
    }
    stopTimers();
    if(pKeithley236 != Q_NULLPTR) {
        pKeithley236->disconnect();
        pKeithley236->stopSweep();
    }
    if(pLakeShore330 != Q_NULLPTR) {
        pLakeShore330->disconnect();
        pLakeShore330->switchPowerOff();
    }
    switchLampOff();
    ui->endTimeEdit->clear();
    ui->startIvsVButton->setText("Start I vs V");
    ui->startRvsTButton->setEnabled(true);
    ui->startRvsTimeButton->setEnabled(true);
    ui->lambdaScanButton->setEnabled(true);
    ui->lampButton->setEnabled(true);
    QApplication::restoreOverrideCursor();
}


void
MainWindow::stopLambdaScan() {
    bRunning = false;
    presentMeasure = NoMeasure;
    if(pOutputFile) {
        pOutputFile->close();
        pOutputFile->deleteLater();
        pOutputFile = Q_NULLPTR;
    }
    stopTimers();
    if(pKeithley236 != Q_NULLPTR) {
        pKeithley236->disconnect();
    }
    if(pLakeShore330 != Q_NULLPTR) {
        pLakeShore330->disconnect();
        pLakeShore330->switchPowerOff();
    }
    switchLampOff();
    ui->endTimeEdit->clear();
    ui->lambdaScanButton->setText("λ Scan");
    ui->startIvsVButton->setEnabled(true);
    ui->startRvsTButton->setEnabled(true);
    ui->startRvsTimeButton->setEnabled(true);
    ui->lampButton->setEnabled(true);
    ui->statusBar->showMessage(QString("λ Scan Terminated"));
    QApplication::restoreOverrideCursor();
}


void
MainWindow::goNextLambda() {
    double nextWavelength = pCornerStone130->dPresentWavelength + wlResolution;
#if defined(MY_DEBUG)
    logMessage(QString("new Wavelength= %1").arg(nextWavelength));
#endif
    if(nextWavelength > pConfigureDialog->pTabCS130->dStopWavelength) {
        stopLambdaScan();
    }
    else {
        pCornerStone130->setWavelength(nextWavelength);     
    }
}


bool
MainWindow::prepareOutputFile(QString sBaseDir, QString sFileName) {
    if(pOutputFile) {
        pOutputFile->close();
        pOutputFile->deleteLater();
        pOutputFile = Q_NULLPTR;
    }
    pOutputFile = new QFile(sBaseDir + "/" + sFileName);
    if(!pOutputFile->open(QIODevice::Text|QIODevice::WriteOnly)) {
        QMessageBox::critical(this,
                              "Error: Unable to Open Output File",
                              QString("%1/%2")
                              .arg(sBaseDir)
                              .arg(sFileName));
        ui->statusBar->showMessage("Unable to Open Output file...");
        return false;
    }
    return true;
}


void
MainWindow::initRvsTPlots() {
    if(pPlotMeasurements) delete pPlotMeasurements;
    pPlotMeasurements = Q_NULLPTR;
    if(pPlotTemperature) delete pPlotTemperature;
    pPlotTemperature = Q_NULLPTR;
    // Plot of Conductivity vs Temperature
    sMeasurementPlotLabel = QString("log(S) [Ohm^-1] -vs- 1000/T [K^-1]");
    pPlotMeasurements = new Plot2D(this, sMeasurementPlotLabel);
    pPlotMeasurements->setWindowTitle(pConfigureDialog->pTabFile->sOutFileName);
    pPlotMeasurements->setMaxPoints(maxPlotPoints);
    pPlotMeasurements->SetLimits(0.0, 1.0, 0.1, 1.0, true, true, false, true);
    // Dataset for Dark measurements
    pPlotMeasurements->NewDataSet(iPlotDark,//Id
                                  3, //Pen Width
                                  QColor(255, 0, 0),// Color
                                  Plot2D::ipoint,// Symbol
                                  "Dark"// Title
                                  );
    pPlotMeasurements->SetShowDataSet(iPlotDark, true);
    pPlotMeasurements->SetShowTitle(iPlotDark, true);
    // Dataset for Illuminated measurements
    pPlotMeasurements->NewDataSet(iPlotPhoto,//Id
                                  3, //Pen Width
                                  QColor(255, 255, 0),// Color
                                  Plot2D::ipoint,// Symbol
                                  "Photo"// Title
                                  );
    pPlotMeasurements->SetShowDataSet(iPlotPhoto, true);
    pPlotMeasurements->SetShowTitle(iPlotPhoto, true);
    pPlotMeasurements->UpdatePlot();
    pPlotMeasurements->show();

    initTemperaturePlot();
    if(bDHT22Present) {
        initRHvsTimePlot();
    }
}


void
MainWindow::initRvsTimePlots() {
    if(pPlotMeasurements) delete pPlotMeasurements;
    pPlotMeasurements = Q_NULLPTR;
    if(pPlotTemperature) delete pPlotTemperature;
    pPlotTemperature = Q_NULLPTR;

    if(!pKeithley236) return;
    QString sTitle;
    if(pConfigureDialog->pTabK236->bSourceI &&
       pConfigureDialog->pTabK236->dStart == 0.0)
    {
        // Plot Open Circuit Voltage vs Time
        sMeasurementPlotLabel = QString("Voc [V] -vs- Time [s]");
        sTitle = "V(t)";
    }
    else if(!pConfigureDialog->pTabK236->bSourceI &&
            pConfigureDialog->pTabK236->dStart == 0.0)
    {
        // Plot Short Circuit Crrent vs Time
        sMeasurementPlotLabel = QString("Ish [A] -vs- Time [s]");
        sTitle = "I(t)";
    }
    else {
        // Plot of Resistance vs Time
        sMeasurementPlotLabel = QString("R [Ohm] -vs- Time [s]");
        sTitle = "R(t)";
    }
    pPlotMeasurements = new Plot2D(this, sMeasurementPlotLabel);
    pPlotMeasurements->setWindowTitle(pConfigureDialog->pTabFile->sOutFileName);
    pPlotMeasurements->setMaxPoints(maxPlotPoints);
    pPlotMeasurements->SetLimits(0.0, 1.0, 0.1, 1.0, true, true, false, false);
    // Dataset
    pPlotMeasurements->NewDataSet(iPlotDark,           //Id
                                  3,                   //Pen Width
                                  QColor(255, 255, 64),// Color
                                  Plot2D::ipoint,      // Symbol
                                  sTitle               // Title
                       );
    pPlotMeasurements->SetShowDataSet(iPlotDark, true);
    pPlotMeasurements->SetShowTitle(iPlotDark, true);
    pPlotMeasurements->UpdatePlot();
    pPlotMeasurements->show();

    if(bUseLakeShore330 || bDHT22Present) {
        initTemperaturePlot();
    }
    if(bDHT22Present) {
        initRHvsTimePlot();
    }
}


void
MainWindow::initRHvsTimePlot() {
    if(pPlotRH) delete pPlotRH;
    pPlotRH = Q_NULLPTR;

    // Plot of RH vs Time
    pPlotRH = new Plot2D(this, QString("RH [%] -vs- Time [s]"));
    pPlotRH->setWindowTitle(QString("Relative Humidity [%]"));
    pPlotRH->setMaxPoints(maxPlotPoints);
    pPlotRH->SetLimits(0.0, 1.0, 0.0, 100.0, true, false, false, false);
    // Dataset
    pPlotRH->NewDataSet(1,                   //Id
                        3,                   //Pen Width
                        QColor(255, 255, 64),// Color
                        Plot2D::ipoint,      // Symbol
                        "RH%(t)"             // Title
                       );
    pPlotRH->SetShowDataSet(1, true);
    pPlotRH->SetShowTitle(1, true);
    pPlotRH->UpdatePlot();
    pPlotRH->show();
}


void
MainWindow::initIvsVPlots() {
    if(pPlotMeasurements) delete pPlotMeasurements;
    pPlotMeasurements = Q_NULLPTR;
    if(pPlotTemperature) delete pPlotTemperature;
    pPlotTemperature = Q_NULLPTR;
    // Plot of Current vs Voltage
    sMeasurementPlotLabel = QString("I [A] vs V [V]");
    pPlotMeasurements = new Plot2D(this, sMeasurementPlotLabel);
    pPlotMeasurements->setWindowTitle(pConfigureDialog->pTabFile->sOutFileName);
    pPlotMeasurements->setMaxPoints(maxPlotPoints);
    pPlotMeasurements->NewDataSet(1,//Id
                                  3, //Pen Width
                                  QColor(255, 255, 0),// Color
                                  Plot2D::ipoint,// Symbol
                                  "IvsV"// Title
                                  );
    pPlotMeasurements->SetShowDataSet(1, true);
    pPlotMeasurements->SetShowTitle(1, true);
    pPlotMeasurements->SetLimits(0.0, 1.0, 0.0, 1.0, true, true, false, false);
    pPlotMeasurements->UpdatePlot();
    pPlotMeasurements->show();
    if(pLakeShore330) {
        // Plot of Temperature vs Time
        initTemperaturePlot();
    }
    if(bDHT22Present) {
        initRHvsTimePlot();
    }
}


void
MainWindow::initSvsLPlots() {
    // Remove old plots if any
    if(pPlotMeasurements) delete pPlotMeasurements;
    pPlotMeasurements = Q_NULLPTR;
    if(pPlotTemperature) delete pPlotTemperature;
    pPlotTemperature = Q_NULLPTR;

    // Plot of Sigma vs Wavelength
    sMeasurementPlotLabel = QString("S [Ohm^-1] -vs- Lambda [nm]");
    pPlotMeasurements = new Plot2D(this, sMeasurementPlotLabel);
    pPlotMeasurements->setWindowTitle(pConfigureDialog->pTabFile->sOutFileName);
    pPlotMeasurements->setMaxPoints(maxPlotPoints);
    pPlotMeasurements->NewDataSet(1,//Id
                                  3, //Pen Width
                                  QColor(255, 128, 64),// Color
                                  Plot2D::ipoint,// Symbol
                                  "Grat 1"// Title
                                  );
    pPlotMeasurements->SetShowDataSet(1, true);
    pPlotMeasurements->SetShowTitle(1, true);
    pPlotMeasurements->SetLimits(0.0, 1.0, 0.0, 1.0, true, true, false, false);
    pPlotMeasurements->UpdatePlot();
    pPlotMeasurements->show();

    initTemperaturePlot();
    if(bDHT22Present) {
        initRHvsTimePlot();
    }
}


void
MainWindow::initTemperaturePlot() {
    sTemperaturePlotLabel = QString("T [K] -vs- t [s]");
    pPlotTemperature = new Plot2D(this, sTemperaturePlotLabel);
    pPlotTemperature->setMaxPoints(maxPlotPoints);
    pPlotTemperature->NewDataSet(1,//Id
                                 3, //Pen Width
                                 QColor(255, 128, 64),// Color
                                 Plot2D::ipoint,// Symbol
                                 "T"// Title
                                 );
    pPlotTemperature->SetShowDataSet(1, true);
    pPlotTemperature->SetShowTitle(1, true);
    pPlotTemperature->SetLimits(0.0, 1.0, 0.0, 1.0, true, true, false, false);

    pPlotTemperature->NewDataSet(2,//Id
                                 3, //Pen Width
                                 QColor(255, 255, 0),// Color
                                 Plot2D::ipoint,// Symbol
                                 "Tm"// Title
                                 );
    pPlotTemperature->SetShowDataSet(2, true);
    pPlotTemperature->SetShowTitle(2, true);
    pPlotTemperature->SetLimits(0.0, 1.0, 0.0, 1.0, true, true, false, false);

    pPlotTemperature->UpdatePlot();
    pPlotTemperature->show();

    iCurrentTPlot = 1;
}


// Invoked to check the reaching of the temperature
// Set Point during I-V measurements
// ToDo: Change Name !!!
void
MainWindow::onTimeToCheckT() {
    double T = pLakeShore330->getTemperature();
    if(fabs(T-setPointT) < 0.15) {
        waitingTStartTimer.stop();
        waitingTStartTimer.disconnect();
        connect(&stabilizingTimer, SIGNAL(timeout()),
                this, SLOT(onSteadyTReached()));
        stabilizingTimer.start(pConfigureDialog->pTabLS330->iTimeToSteadyT*60000);
        ui->statusBar->showMessage(QString("Thermal Stabilization for %1 min.")
                                   .arg(pConfigureDialog->pTabLS330->iTimeToSteadyT));
    }
    else {
        currentTime = QDateTime::currentDateTime();
        qint64 elapsedSec = waitingTStartTime.secsTo(currentTime);
        if(elapsedSec > qint64(pConfigureDialog->pTabLS330->iReachingTStart)*60) {
            waitingTStartTimer.stop();
            waitingTStartTimer.disconnect();
            connect(&stabilizingTimer, SIGNAL(timeout()),
                    this, SLOT(onSteadyTReached()));
            stabilizingTimer.start(pConfigureDialog->pTabLS330->iTimeToSteadyT*60000);
            ui->statusBar->showMessage(QString("Thermal Stabilization for %1 min.")
                                       .arg(pConfigureDialog->pTabLS330->iTimeToSteadyT));
        }
    }
}


// Invoked when the thermal stabilization is done
// during I-V measurements
void
MainWindow::onSteadyTReached() {
    stabilizingTimer.stop();
    stabilizingTimer.disconnect();
    // Update the time needed for the measurement:
    double deltaT, expectedMinutes;
    deltaT = fabs(pConfigureDialog->pTabLS330->dTStop -
             pConfigureDialog->pTabLS330->dTStart);
    expectedMinutes = deltaT / pConfigureDialog->pTabLS330->dTRate;
    endMeasureTime = QDateTime::currentDateTime().addSecs(qint64(expectedMinutes*60.0));
    QString sString = endMeasureTime.toString("hh:mm dd-MM-yyyy");
    ui->endTimeEdit->setText(sString);
    startI_Vscan(pConfigureDialog->pTabK236->bSourceI);
}


// Invoked to check the reaching of the initial
// temperature Set Point
void
MainWindow::onTimeToCheckReachedT() {
    double T = pLakeShore330->getTemperature();
    if(fabs(T-pConfigureDialog->pTabLS330->dTStart) < 0.15) {
        waitingTStartTimer.disconnect();
        waitingTStartTimer.stop();
        connect(&stabilizingTimer, SIGNAL(timeout()),
                this, SLOT(onTimerStabilizeT()));
        stabilizingTimer.start(pConfigureDialog->pTabLS330->iTimeToSteadyT*60000);
        ui->statusBar->showMessage(QString("Starting T Reached: Thermal Stabilization for %1 min.")
                                   .arg(pConfigureDialog->pTabLS330->iTimeToSteadyT));
        // Compute the new time needed for the measurement:
        startMeasuringTime = QDateTime::currentDateTime();
        double deltaT, expectedMinutes;
        expectedMinutes = pConfigureDialog->pTabLS330->iTimeToSteadyT;
        if((presentMeasure==RvsTSourceI)||
           (presentMeasure==RvsTSourceV))
        {
            deltaT = fabs(pConfigureDialog->pTabLS330->dTStop -
                     pConfigureDialog->pTabLS330->dTStart);
            expectedMinutes += deltaT / pConfigureDialog->pTabLS330->dTRate;
        }
        endMeasureTime = startMeasuringTime.addSecs(qint64(expectedMinutes*60.0));
        QString sString = endMeasureTime.toString("hh:mm dd-MM-yyyy");
        ui->endTimeEdit->setText(sString);
    }
    else {
        currentTime = QDateTime::currentDateTime();
        qint64 elapsedSec = waitingTStartTime.secsTo(currentTime);
        if(elapsedSec >= qint64(pConfigureDialog->pTabLS330->iReachingTStart)*60) {
            waitingTStartTimer.disconnect();
            waitingTStartTimer.stop();
            connect(&stabilizingTimer, SIGNAL(timeout()),
                    this, SLOT(onTimerStabilizeT()));
            stabilizingTimer.start(pConfigureDialog->pTabLS330->iTimeToSteadyT*60000);
            ui->statusBar->showMessage(QString("Max Time Exceeded: Stabilization for %1 min.")
                                       .arg(pConfigureDialog->pTabLS330->iTimeToSteadyT));
        }
    }
}


void
MainWindow::onTimerStabilizeT() {
    // It's time to start measurements
    stabilizingTimer.stop();
    stabilizingTimer.disconnect();
    readingTTimer.stop();
    readingTTimer.disconnect();
    pPlotTemperature->NewDataSet(2,//Id
                                 3, //Pen Width
                                 QColor(255, 255, 0),// Color
                                 Plot2D::ipoint,// Symbol
                                 "Tm"// Title
                                 );
    pPlotTemperature->SetShowDataSet(2, true);
    pPlotTemperature->SetShowTitle(2, true);
    pPlotTemperature->UpdatePlot();
    iCurrentTPlot = 2;
    ui->statusBar->showMessage(QString("Thermal Stabilization Reached: Measure Started"));
    connect(&measuringTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToGetNewK236Measure()));
    if((presentMeasure==RvsTSourceI)||
        (presentMeasure==RvsTSourceV)) {
        if(!pLakeShore330->startRamp(pConfigureDialog->pTabLS330->dTStop, pConfigureDialog->pTabLS330->dTRate)) {
            ui->statusBar->showMessage(QString("Error Starting the Measure"));
            return;
        }
    }
    double timeBetweenMeasurements = pConfigureDialog->pTabK236->dInterval*1000.0;
    measuringTimer.start(int(timeBetweenMeasurements));
    // Update the time needed for the measurement:
    double deltaT, expectedMinutes;
    deltaT = fabs(pConfigureDialog->pTabLS330->dTStop -
             pConfigureDialog->pTabLS330->dTStart);
    expectedMinutes = deltaT / pConfigureDialog->pTabLS330->dTRate;
    endMeasureTime = QDateTime::currentDateTime().addSecs(qint64(expectedMinutes*60.0));
    QString sString = endMeasureTime.toString("hh:mm dd-MM-yyyy");
    ui->endTimeEdit->setText(sString);
    bRunning = true;
}


// ToDo: Change Name (onTimeToGetNewRamp ?)
void
MainWindow::onTimeToGetNewK236Measure() {
    getNewK236Measure();
    if((presentMeasure==RvsTSourceI) ||
       (presentMeasure==RvsTSourceV))
    {
        if(!pLakeShore330->isRamping()) {// Ramp is Done
            stopRvsT();
            ui->statusBar->showMessage(QString("Measurements Completed !"));
            onClearComplianceEvent();
            return;
        }
    }
}


void
MainWindow::onTimeToGetNewHp3478Measure() {
    getNewHp3478Measure();
}


void
MainWindow::onTimeToReadT() {
    if(pLakeShore330) {
        currentTemperature = pLakeShore330->getTemperature();
        currentTime = QDateTime::currentDateTime();
        ui->temperatureEdit->setText(QString("%1").arg(currentTemperature));
        pPlotTemperature->NewPoint(iCurrentTPlot,
                                   double(startReadingTTime.secsTo(currentTime)),
                                   currentTemperature);
        pPlotTemperature->UpdatePlot();
        if(stabilizingTimer.isActive()) {
            ui->statusBar->showMessage(QString("Thermal Stabilization for %1 min.")
                                       .arg(ceil(stabilizingTimer.remainingTime()/(60000.0))));
        }
    }
}


void
MainWindow::onComplianceEvent() {
    ui->labelCompliance->setText("Compliance");
    ui->labelCompliance->setStyleSheet(sErrorStyle);
    logMessage("Compliance Event");
}


void
MainWindow::onClearComplianceEvent() {
    ui->labelCompliance->setText("");
    ui->labelCompliance->setStyleSheet(sNormalStyle);
}


void
MainWindow::onKeithleyReadyForTrigger() {
    isK236ReadyForTrigger = true;
}


void
MainWindow::onHp3478ReadyForTrigger() {
    isHp3478ReadyForTrigger = true;
}


bool
MainWindow::onKeithleyReadyForSweepTrigger() {
    disconnect(pKeithley236, SIGNAL(readyForTrigger()),
               this, SLOT(onKeithleyReadyForSweepTrigger()));
    return pKeithley236->sendTrigger();
}


void
MainWindow::onNewRvsTKeithleyReading(QDateTime dateTime, QString sDataRead) {
    double elapsedTime;
    elapsedTime = double(dateStart.secsTo(dateTime));
    double current, voltage;
    if(!DecodeReadings(sDataRead, &current, &voltage))
        return;
    currentTemperature = pLakeShore330->getTemperature();
    pPlotTemperature->NewPoint(2, elapsedTime, currentTemperature);
    pPlotTemperature->UpdatePlot();

    ui->temperatureEdit->setText(QString("%1").arg(currentTemperature));
    ui->currentEdit->setText(QString("%1").arg(current, 10, 'g', 4, ' '));
    ui->voltageEdit->setText(QString("%1").arg(voltage, 10, 'g', 4, ' '));
    if(bUseMonochromator) {
        double lambda = pCornerStone130->dPresentWavelength;
        ui->wavelengthEdit->setText(QString("%1").arg(lambda, 10, 'f', 1, ' '));
    }

    if(!bRunning) return;

    QString sData = QString("%1 %2 %3")
                            .arg(currentTemperature, 12, 'g', 6, ' ')
                            .arg(voltage, 12, 'g', 6, ' ')
                            .arg(current, 12, 'g', 6, ' ')
                            .arg(userData.iHumidity/10.0, 12, 'i', 6, ' ');
    pOutputFile->write(sData.toLocal8Bit());
    if(currentLampStatus == LAMP_OFF) {
        if(voltage != 0.0) {
            pPlotMeasurements->NewPoint(iPlotDark, 1000.0/currentTemperature, current/voltage);
            pPlotMeasurements->UpdatePlot();
        }
        switchLampOn();
    }
    else {
        if(voltage != 0.0) {
            pPlotMeasurements->NewPoint(iPlotPhoto, 1000.0/currentTemperature, current/voltage);
            pPlotMeasurements->UpdatePlot();
        }
        pOutputFile->write("\n");
        pOutputFile->flush();
        switchLampOff();
    }
}


void
MainWindow::onNewRvsTimeHp3478Reading(QDateTime dateTime, QString sDataRead) {
    double resistance, elapsedTime;
    resistance = sDataRead.toDouble();
    elapsedTime = double(dateStart.secsTo(dateTime));
    if(bUseLakeShore330) {
        currentTemperature = pLakeShore330->getTemperature();
    } else {
        currentTemperature = userData.iTemperature/10.0;
    }
    pPlotTemperature->NewPoint(2, elapsedTime, currentTemperature);
    pPlotTemperature->UpdatePlot();
    ui->temperatureEdit->setText(QString("%1").arg(currentTemperature));

    ui->currentEdit->setText(QString("%1").arg(resistance, 10, 'g', 4, ' '));

    if(!bRunning) return;

    QString sData = QString("%1 %2 %3\n")
                            .arg(elapsedTime, 12, 'g', 6, ' ')
                            .arg(resistance, 12, 'g', 6, ' ')
                            .arg(currentTemperature, 12, 'g', 6, ' ')
                            .arg(userData.iHumidity/10.0, 12, 'i', 6, ' ');
    pOutputFile->write(sData.toLocal8Bit());
    pOutputFile->flush();

    pPlotMeasurements->NewPoint(iPlotDark, elapsedTime, resistance);
    pPlotMeasurements->UpdatePlot();
}


void
MainWindow::onNewRvsTimeKeithleyReading(QDateTime dateTime, QString sDataRead) {
    double current, voltage, elapsedTime;
    if(!DecodeReadings(sDataRead, &current, &voltage))
        return;
    elapsedTime = double(dateStart.secsTo(dateTime));
    if(pLakeShore330) {
        currentTemperature = pLakeShore330->getTemperature();
    }
    else {
        currentTemperature = userData.iTemperature/10.0;
    }
    if(pPlotTemperature) {
        pPlotTemperature->NewPoint(2, elapsedTime, currentTemperature);
        pPlotTemperature->UpdatePlot();
        ui->temperatureEdit->setText(QString("%1").arg(currentTemperature));
    }
    ui->currentEdit->setText(QString("%1").arg(current, 10, 'g', 4, ' '));
    ui->voltageEdit->setText(QString("%1").arg(voltage, 10, 'g', 4, ' '));
    if(bUseMonochromator) {
        double lambda = pCornerStone130->dPresentWavelength;
        ui->wavelengthEdit->setText(QString("%1").arg(lambda, 10, 'f', 1, ' '));
    }

    if(!bRunning) return;

    QString sData = QString("%1 %2 %3 %4 %5\n")
                            .arg(elapsedTime, 12, 'g', 6, ' ')
                            .arg(voltage, 12, 'g', 6, ' ')
                            .arg(current, 12, 'g', 6, ' ')
                            .arg(currentTemperature, 12, 'g', 6, ' ')
                            .arg(userData.iHumidity/10.0, 12, 'i', 6, ' ');
    pOutputFile->write(sData.toLocal8Bit());
    pOutputFile->flush();
    if(presentMeasure == RvsTimeSourceI && fabs(current) < 1.0e-16) {
        pPlotMeasurements->NewPoint(iPlotDark, elapsedTime, voltage);
        pPlotMeasurements->UpdatePlot();
    }
    else if(presentMeasure == RvsTimeSourceV && fabs(voltage) < 1e-16) {
        pPlotMeasurements->NewPoint(iPlotDark, elapsedTime, current);
        pPlotMeasurements->UpdatePlot();
    }
    else if(current != 0.0) {
        pPlotMeasurements->NewPoint(iPlotDark, elapsedTime, voltage/current);
        pPlotMeasurements->UpdatePlot();
    }
}


bool
MainWindow::DecodeReadings(QString sDataRead, double *current, double *voltage) {    // Decode readings
    QStringList sMeasures = QStringList(sDataRead.split(",", Qt::SkipEmptyParts));
    if(sMeasures.count() < 2) {
        logMessage("Measurement Format Error");
        return false;
    }
    if(pLakeShore330) {
        currentTemperature = pLakeShore330->getTemperature();
    }
    if(pConfigureDialog->pTabK236->bSourceI) {
        *current = sMeasures.at(0).toDouble();
        *voltage = sMeasures.at(1).toDouble();
    }
    else {
        *current = sMeasures.at(1).toDouble();
        *voltage = sMeasures.at(0).toDouble();
    }
    return true;
}


void
MainWindow::onNewLambdaScanKeithleyReading(QDateTime dataTime, QString sDataRead) {
    Q_UNUSED(dataTime)
    double current, voltage;
    if(!DecodeReadings(sDataRead, &current, &voltage))
        return;
    double lambda = pCornerStone130->dPresentWavelength;
    ui->temperatureEdit->setText(QString("%1").arg(currentTemperature));
    ui->currentEdit->setText(QString("%1").arg(current, 10, 'g', 4, ' '));
    ui->voltageEdit->setText(QString("%1").arg(voltage, 10, 'g', 4, ' '));
    ui->wavelengthEdit->setText(QString("%1").arg(lambda, 10, 'f', 1, ' '));

    if(!bRunning) return;

    if(currentLampStatus == LAMP_OFF) {
        QString sData = QString("%1 %2 %3 %4 %5")
                                .arg(lambda,  12, 'g', 6, ' ')
                                .arg(currentTemperature, 12, 'g', 6, ' ')
                                .arg(voltage, 12, 'g', 6, ' ')
                                .arg(current, 12, 'g', 6, ' ')
                                .arg(userData.iHumidity/10.0, 12, 'i', 6, ' ');
        pOutputFile->write(sData.toLocal8Bit());
        if(voltage != 0.0) {
            sigmaDark = current/voltage;
            switchLampOn();
        }
        else
            goNextLambda();
    }
    else {
        if(voltage != 0.0) {
            sigmaIll = current/voltage;
            pPlotMeasurements->NewPoint(1, lambda, sigmaIll-sigmaDark);
            pPlotMeasurements->UpdatePlot();
        }
        QString sData = QString("%1 %2 %3 %4\n")
                                .arg(currentTemperature, 12, 'g', 6, ' ')
                                .arg(voltage, 12, 'g', 6, ' ')
                                .arg(current, 12, 'g', 6, ' ')
                                .arg(userData.iHumidity/10.0, 12, 'i', 6, ' ');
        pOutputFile->write(sData.toLocal8Bit());
        pOutputFile->flush();
        switchLampOff();
        goNextLambda();
    }
}


void
MainWindow::onKeithleySweepDone(QDateTime dataTime, QString sData) {
    Q_UNUSED(dataTime)
    disconnect(pKeithley236, SIGNAL(sweepDone(QDateTime,QString)), this, Q_NULLPTR);
    ui->statusBar->showMessage("Sweep Done: Decoding readings...Please wait");
    QStringList sMeasures = QStringList(sData.split(",", Qt::SkipEmptyParts));
    if(sMeasures.count() < 2) {
        stopIvsV();
        ui->statusBar->showMessage(QString(Q_FUNC_INFO) + QString(" Error: No Sweep Values"));
        onClearComplianceEvent();
        return;
    }
    ui->statusBar->showMessage("Sweep Done: Updating Plot...Please wait");
    double current, voltage;
    for(int i=0; i<sMeasures.count(); i+=2) {
        if(presentMeasure == IvsVSourceI) {
            current = sMeasures.at(i).toDouble();
            voltage = sMeasures.at(i+1).toDouble();
        }
        else {
            voltage = sMeasures.at(i).toDouble();
            current = sMeasures.at(i+1).toDouble();
        }
        QString sData = QString("%1 %2 %3 %4\n")
                .arg(voltage, 12, 'g', 6, ' ')
                .arg(current, 12, 'g', 6, ' ')
                .arg(currentTemperature, 12, 'g', 6, ' ')
                .arg(userData.iHumidity/10.0, 12, 'i', 6, ' ');
        pOutputFile->write(sData.toLocal8Bit());
        pPlotMeasurements->NewPoint(1, voltage, current);
    }
    pPlotMeasurements->UpdatePlot();
    pOutputFile->flush();
    if(pConfigureDialog->pTabLS330) {
        if(pConfigureDialog->pTabLS330->bUseThermostat) {
            setPointT += pConfigureDialog->pTabLS330->dTStep;
            if(setPointT > pConfigureDialog->pTabLS330->dTStop) {
                stopIvsV();
                ui->statusBar->showMessage("Measure Done");
                onClearComplianceEvent();
                return;
            }
            isK236ReadyForTrigger = false;
            connect(pKeithley236, SIGNAL(complianceEvent()),
                    this, SLOT(onComplianceEvent()));
            connect(pKeithley236, SIGNAL(clearCompliance()),
                    this, SLOT(onClearComplianceEvent()));
            connect(pKeithley236, SIGNAL(readyForTrigger()),
                    this, SLOT(onKeithleyReadyForSweepTrigger()));
            connect(&waitingTStartTimer, SIGNAL(timeout()),
                    this, SLOT(onTimeToCheckT()));
            waitingTStartTime = QDateTime::currentDateTime();
            // Start the reaching of the Next Temperature
            waitingTStartTimer.start(5000);
            // Configure Thermostat
            pLakeShore330->setTemperature(setPointT);
            pLakeShore330->switchPowerOn(3);
            ui->statusBar->showMessage(QString("%1 Waiting Next T [%2K]")
                                       .arg(waitingTStartTime.toString())
                                       .arg(setPointT));
        }
    }
    else {
        stopIvsV();
        ui->statusBar->showMessage("Measure Done");
        onClearComplianceEvent();
    }
}


void
MainWindow::onIForwardSweepDone(QDateTime dataTime, QString sData) {
    Q_UNUSED(dataTime)
    ui->statusBar->showMessage("Reverse Direction: Sweeping...Please Wait");
    disconnect(pKeithley236, SIGNAL(sweepDone(QDateTime,QString)), this, Q_NULLPTR);
    QStringList sMeasures = QStringList(sData.split(",", Qt::SkipEmptyParts));
    if(sMeasures.count() < 2) {
        logMessage(QString(Q_FUNC_INFO) + QString(" No Sweep Values "));
        return;
    }
    double current, voltage;
    for(int i=0; i<sMeasures.count(); i+=2) {
        if(presentMeasure == IvsVSourceI) {
            current = sMeasures.at(i).toDouble();
            voltage = sMeasures.at(i+1).toDouble();
        }
        else {
            voltage = sMeasures.at(i).toDouble();
            current = sMeasures.at(i+1).toDouble();
        }
        pOutputFile->write(QString("%1 %2 %3 %4\n")
                           .arg(voltage, 12, 'g', 6, ' ')
                           .arg(current, 12, 'g', 6, ' ')
                           .arg(setPointT, 12, 'g', 6, ' ')
                           .arg(userData.iHumidity/10.0, 12, 'i', 6, ' ')
                           .toLocal8Bit());
        pPlotMeasurements->NewPoint(1, voltage, current);
    }
    pPlotMeasurements->UpdatePlot();
    pOutputFile->flush();
    double dVStart;
    double dVStop;
    if(junctionDirection > 0) {// Forward junction
        dVStart = pConfigureDialog->pTabK236->dStart;
        dVStop = 0.0;
    }
    else {// Reverse Junction
        dVStart = 0.0;
        dVStop = pConfigureDialog->pTabK236->dStop;
    }
    int nSweepPoints = pConfigureDialog->pTabK236->iNSweepPoints;
    double dVStep = qAbs(dVStop - dVStart) / double(nSweepPoints);
    double dDelayms = double(pConfigureDialog->pTabK236->iWaitTime);
    double dCompliance = qMax(qAbs(pConfigureDialog->pTabK236->dStart),
                              qAbs(pConfigureDialog->pTabK236->dStop));
    presentMeasure = IvsVSourceV;
    connect(pKeithley236, SIGNAL(readyForTrigger()),
            this, SLOT(onKeithleyReadyForSweepTrigger()));
    connect(pKeithley236, SIGNAL(sweepDone(QDateTime,QString)),
            this, SLOT(onKeithleySweepDone(QDateTime,QString)));
    pKeithley236->initVSweep(dVStart, dVStop, dVStep, dDelayms, dCompliance);
}


void
MainWindow::onVReverseSweepDone(QDateTime dataTime, QString sData) {
    Q_UNUSED(dataTime)
    ui->statusBar->showMessage("Forward Direction: Sweeping...Please Wait");
    disconnect(pKeithley236, SIGNAL(sweepDone(QDateTime,QString)), this, Q_NULLPTR);
    QStringList sMeasures = QStringList(sData.split(",", Qt::SkipEmptyParts));
    if(sMeasures.count() < 2) {
        logMessage(QString(Q_FUNC_INFO) + QString(" No Sweep Values "));
        return;
    }
    double current, voltage;
    for(int i=0; i<sMeasures.count(); i+=2) {
        if(presentMeasure == IvsVSourceI) {
            current = sMeasures.at(i).toDouble();
            voltage = sMeasures.at(i+1).toDouble();
        }
        else {
            voltage = sMeasures.at(i).toDouble();
            current = sMeasures.at(i+1).toDouble();
        }
        pOutputFile->write(QString("%1 %2 %3 %4\n")
                           .arg(voltage, 12, 'g', 6, ' ')
                           .arg(current, 12, 'g', 6, ' ')
                           .arg(setPointT, 12, 'g', 6, ' ')
                           .arg(userData.iHumidity/10.0, 12, 'i', 6, ' ')
                           .toLocal8Bit());
        pPlotMeasurements->NewPoint(1, voltage, current);
    }
    pPlotMeasurements->UpdatePlot();
    pOutputFile->flush();
    double dIStart = pConfigureDialog->pTabK236->dStart;
    double dIStop = 0.0;
    int nSweepPoints = pConfigureDialog->pTabK236->iNSweepPoints;
    double dIStep = qAbs(dIStop - dIStart) / double(nSweepPoints);
    double dDelayms = double(pConfigureDialog->pTabK236->iWaitTime);
    double dCompliance = pConfigureDialog->pTabK236->dCompliance;
    presentMeasure = IvsVSourceI;
    connect(pKeithley236, SIGNAL(readyForTrigger()),
            this, SLOT(onKeithleyReadyForSweepTrigger()));
    connect(pKeithley236, SIGNAL(sweepDone(QDateTime,QString)),
            this, SLOT(onKeithleySweepDone(QDateTime,QString)));
    pKeithley236->initISweep(dIStart, dIStop, dIStep, dDelayms, dCompliance);
}


bool
MainWindow::getNewK236Measure() {
    if(!isK236ReadyForTrigger)
        return false;
    isK236ReadyForTrigger = false;
    return pKeithley236->sendTrigger();
}


bool
MainWindow::getNewHp3478Measure() {
    if(!isHp3478ReadyForTrigger)
        return false;
    isHp3478ReadyForTrigger = false;
    return pHp3478->sendTrigger();
}


void
MainWindow::on_lampButton_clicked() {
    if(currentLampStatus==LAMP_ON)
        switchLampOff();
    else
        switchLampOn();
}


void
MainWindow::onLogMessage(QString sMessage) {
    logMessage(sMessage);
}


void
MainWindow::on_logoButton_clicked() {
    EasterDlg easterDialog;
    easterDialog.exec();
}

void
MainWindow::onTimeToReadHumidity() {
    read_dht22();
}


void
MainWindow::onNewRHdata() {
    if(pPlotRH) {
        currentTime = QDateTime::currentDateTime();
        pPlotRH->NewPoint(1,
                          double(dateStart.secsTo(currentTime)),
                          double(userData.iHumidity/10.0));
        pPlotRH->UpdatePlot();
    }
}


bool
MainWindow::read_dht22() {
    userData.transitionCounter = 0;
    if(userData.callBackId != pigif_bad_callback)
        if(callback_cancel(uint(userData.callBackId)))
            qDebug() << "callback_cancel Failed";
    set_pull_up_down(gpioHostHandle, gpioDHT22pin, PI_PUD_UP);
    set_mode(gpioHostHandle, gpioDHT22pin, PI_OUTPUT);

    // pull pin down for 2 milliseconds
    gpio_write(gpioHostHandle, gpioDHT22pin, 0);
    QThread::msleep(2);

    // then pull it up for 40 microseconds by simply let
    // the pulllup to operate and prepare to read the pin
    set_mode(gpioHostHandle, gpioDHT22pin, PI_INPUT);
    QThread::usleep(40);
    userData.callBackId = callback_ex(gpioHostHandle,
                                      gpioDHT22pin,
                                      EITHER_EDGE,
                                      CBFuncEx_t(dht22Callback),
                                      reinterpret_cast<void *>(&userData));
    if(userData.callBackId < 0) {
        if(userData.callBackId == pigif_bad_malloc)
            qDebug() << "pigif_bad_malloc";
        if(userData.callBackId == pigif_duplicate_callback)
            qDebug() << "pigif_duplicate_callback";
        if(userData.callBackId == pigif_bad_callback)
            qDebug() << "pigif_bad_callback";
        return false;
    }

    return true;
}
