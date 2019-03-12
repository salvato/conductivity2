#include "hp3478tab.h"
#include <QLineEdit>
#include <QLabel>
#include <QGridLayout>
#include <QSettings>
#include <QtDebug>

hp3478Tab::hp3478Tab(int iConfiguration, QWidget *parent)
    : QWidget(parent)
    , intervalMin(2.0)
    , intervalMax(60.0)
    , myConfiguration(iConfiguration)
{
    // Build the Tab layout
    QGridLayout* pLayout = new QGridLayout();

    // Create UI Elements
    pLayout->addWidget(new QLabel("Meas. Intv [s]"), 0, 0, 1, 1);
    pLayout->addWidget(&MeasureIntervalEdit,         0, 1, 1, 1);

    // Set the Layout
    setLayout(pLayout);

    sNormalStyle = MeasureIntervalEdit.styleSheet();

    sErrorStyle  = "QLineEdit { ";
    sErrorStyle += "color: rgb(255, 255, 255);";
    sErrorStyle += "background: rgb(255, 0, 0);";
    sErrorStyle += "selection-background-color: rgb(128, 128, 255);";
    sErrorStyle += "}";

    connectSignals();
    restoreSettings();
    initUI();
}


void
hp3478Tab::restoreSettings() {
    QSettings settings;
    dInterval     = settings.value("Hp3478TabMeasureInterval", 2.0).toDouble();
}


void
hp3478Tab::saveSettings() {
    QSettings settings;
    settings.setValue("Hp3478TabMeasureInterval", dInterval);
}


void
hp3478Tab::setToolTips() {
    QString sHeader = QString("Enter values in range [%1 : %2]");
    MeasureIntervalEdit.setToolTip(sHeader.arg(intervalMin).arg(intervalMax));
}


void
hp3478Tab::initUI() {
    MeasureIntervalEdit.setText(QString("%1").arg(dInterval, 0, 'f', 2));
    setToolTips();
}


void
hp3478Tab::connectSignals() {
    connect(&MeasureIntervalEdit, SIGNAL(textChanged(const QString)),
            this, SLOT(onMeasureIntervalEdit_textChanged(const QString)));
}


bool
hp3478Tab::isIntervalValid(double interval) {
    return (interval >= intervalMin) && (interval <= intervalMax);
}


void
hp3478Tab::onMeasureIntervalEdit_textChanged(const QString &arg1) {
    if(isIntervalValid(arg1.toDouble())){
        dInterval = arg1.toDouble();
        MeasureIntervalEdit.setStyleSheet(sNormalStyle);
    }
    else {
        MeasureIntervalEdit.setStyleSheet(sErrorStyle);
    }
}

