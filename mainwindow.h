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

#include <QMainWindow>
#include <QDateTime>
#include <QTimer>

#include "configuredialog.h"
#include "pigpiod_if2.h" // The header for using GPIO pins on Raspberry



namespace Ui {
    class MainWindow;
}


QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(Keithley236)
QT_FORWARD_DECLARE_CLASS(LakeShore330)
QT_FORWARD_DECLARE_CLASS(CornerStone130)
QT_FORWARD_DECLARE_CLASS(Hp3478)
QT_FORWARD_DECLARE_CLASS(Plot2D)


typedef struct {
    int transitionCounter;
    int iHumidity;
    int iTemperature;
    int callBackId;
    void* pMainWindow = Q_NULLPTR;
} callbackData;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(int iBoard, QWidget *parent = Q_NULLPTR);
    ~MainWindow() Q_DECL_OVERRIDE;
    bool checkInstruments();
    void updateUserInterface();

public:
    static const int iConfIvsV    = 1;
    static const int iConfRvsT    = 2;
    static const int iConfLScan   = 3;
    static const int iConfRvsTime = 4;

public:
    bool             bUseMonochromator;
    bool             bUseKeithley236;
    bool             bUseLakeShore330;
    bool             bUseHp3478;
    bool             bUseGpio;
    bool             bDHT22Present;

signals:
    void dhtMeasureDone();

protected:
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
    void stopTimers();
    bool getNewK236Measure();
    bool getNewHp3478Measure();
    void initTemperaturePlot();
    void writeRvsTHeader();
    void initRvsTPlots();
    void stopRvsT();
    void writeRvsTimeHeader();
    void initRvsTimePlots();
    void stopRvsTime();
    void writeIvsVHeader();
    void startI_Vscan(bool bSourceI);
    void initIvsVPlots();
    void stopIvsV();
    void writeLambdaScanHeader();
    void initSvsLPlots();
    void goNextLambda();
    void stopLambdaScan();
    bool prepareOutputFile(QString sBaseDir, QString sFileName);
    void switchLampOn();
    void switchLampOff();
    bool prepareLogFile();
    void logMessage(QString sMessage);
    bool DecodeReadings(QString sDataRead, double *current, double *voltage);
    void initRHvsTimePlot();
    bool read_dht22();

private slots:
    void on_startRvsTButton_clicked();
    void on_startRvsTimeButton_clicked();
    void on_startIvsVButton_clicked();
    void onTimeToCheckReachedT();
    void onTimeToCheckT();
    void onTimerStabilizeT();
    void onSteadyTReached();
    void onTimeToReadT();
    void onTimeToReadHumidity();
    void onTimeToGetNewK236Measure();
    void onComplianceEvent();
    void onClearComplianceEvent();
    void onKeithleyReadyForTrigger();
    void onNewRvsTKeithleyReading(QDateTime dataTime, QString sDataRead);
    void onNewRvsTimeKeithleyReading(QDateTime dataTime, QString sDataRead);
    void onNewRvsTimeHp3478Reading(QDateTime, QString);
    void onHp3478ReadyForTrigger();
    void onTimeToGetNewHp3478Measure();
    void onNewLambdaScanKeithleyReading(QDateTime dataTime, QString sDataRead);
    bool onKeithleyReadyForSweepTrigger();
    void onKeithleySweepDone(QDateTime dataTime, QString sData);
    void onIForwardSweepDone(QDateTime, QString sData);
    void onVReverseSweepDone(QDateTime, QString sData);
    void on_lampButton_clicked();
    void onLogMessage(QString sMessage);
    void on_lambdaScanButton_clicked();
    void on_logoButton_clicked();
    void onNewRHdata();


private:
    Ui::MainWindow *ui;

    enum measure {
        NoMeasure      = 0,
        RvsTSourceI    = 1,
        RvsTSourceV    = 2,
        IvsVSourceI    = 3,
        IvsVSourceV    = 4,
        IvsV           = 5,
        LambdaScanI    = 6,
        LambdaScanV    = 7,
        RvsTimeSourceI = 8,
        RvsTimeSourceV = 9
    };
    measure          presentMeasure;

    QFile           *pOutputFile;
    QFile           *pLogFile;
    Keithley236     *pKeithley236;
    LakeShore330    *pLakeShore330;
    CornerStone130  *pCornerStone130;
    Hp3478          *pHp3478;
    Plot2D          *pPlotMeasurements;
    Plot2D          *pPlotTemperature;
    Plot2D          *pPlotRH;
    ConfigureDialog *pConfigureDialog;

    QString          sNormalStyle;
    QString          sErrorStyle;
    QString          sDarkStyle;
    QString          sPhotoStyle;

    QDateTime        currentTime;
    QDateTime        waitingTStartTime;
    QDateTime        startReadingTTime;
    QDateTime        startMeasuringTime;
    QDateTime        endMeasureTime;
    QDateTime        dateStart;

    QTimer           waitingTStartTimer;
    QTimer           stabilizingTimer;
    QTimer           readingTTimer;
    QTimer           measuringTimer;
    QTimer           readingDHT22Timer;

    const quint8     LAMP_ON    = 1;
    const quint8     LAMP_OFF   = 0;
    const int        iPlotDark  = 1;
    const int        iPlotPhoto = 2;

    double           currentTemperature;
    double           setPointT;
    int              iCurrentTPlot;
    int              gpibBoardID;
    quint8           currentLampStatus;
    QString          sMeasurementPlotLabel;
    QString          sTemperaturePlotLabel;
    int              maxPlotPoints;
    volatile bool    isK236ReadyForTrigger;
    volatile bool    isHp3478ReadyForTrigger;
    bool             bRunning;
    int              junctionDirection;
    int              gpioHostHandle;
    uint             gpioLEDpin;
    uint             gpioDHT22pin;
    double           sigmaDark;
    double           sigmaIll;
    double           wlResolution;
    callbackData     userData;

    QString          sLogFileName;
    QString          sLogDir;
};


#ifdef __cplusplus
extern "C" {
#endif

CBFuncEx_t dht22Callback(int handle,
                         unsigned user_gpio,
                         uint8_t level,
                         uint32_t currentTick,
                         void *userdata);

#ifdef __cplusplus
}
#endif

