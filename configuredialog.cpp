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
#include "configuredialog.h"

#include "k236tab.h"
#include "ls330tab.h"
#include "cs130tab.h"
#include "filetab.h"

#include <QTabWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>


ConfigureDialog::ConfigureDialog(int iConfiguration, bool enableMonochromator, QWidget *parent)
    : QDialog(parent)
    , configurationType(iConfiguration)
    , bUseMonochromator(enableMonochromator)
{
    pTabK236  = new K236Tab(configurationType, this);
    pTabLS330 = new LS330Tab(configurationType, this);
    pTabCS130 = new CS130Tab(configurationType, bUseMonochromator, this);
    pTabFile  = new FileTab(configurationType, this);

    pTabWidget = new QTabWidget();

    iFileIndex   = pTabWidget->addTab(pTabFile,  tr("Out File"));
    iSourceIndex = pTabWidget->addTab(pTabK236,  tr("K236"));
    iThermIndex  = pTabWidget->addTab(pTabLS330, tr("LS330"));
    iMonoIndex   = pTabWidget->addTab(pTabCS130, tr("CS130"));

    pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok |
                                      QDialogButtonBox::Cancel);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(pTabWidget);
    mainLayout->addWidget(pButtonBox);
    setLayout(mainLayout);

    switch(configurationType) {
    case MainWindow::iConfIvsV:
        setWindowTitle("I versus V");
        break;
    case MainWindow::iConfRvsT:
        setWindowTitle("R versus T");
        break;
    case MainWindow::iConfRvsTime:
        setWindowTitle("R versus Time");
//        pTabLS330->setDisabled(true);
        pTabCS130->setDisabled(true);
        break;
    case MainWindow::iConfLScan:
        setWindowTitle("Lambda Scan");
        break;
    default:
        setWindowTitle("Unknown Measurement");
        pTabFile->setDisabled(true);
        pTabK236->setDisabled(true);
        pTabLS330->setDisabled(true);
        pTabCS130->setDisabled(true);
        reject();
    }
    connectSignals();
    setToolTips();
}


void
ConfigureDialog::setToolTips() {
    pTabWidget->setTabToolTip(iSourceIndex, QString("Source-Measure Unit configuration"));
    pTabWidget->setTabToolTip(iThermIndex,  QString("Thermostat configuration"));
    pTabWidget->setTabToolTip(iMonoIndex,   QString("Monochromator configuration"));
    pTabWidget->setTabToolTip(iFileIndex,   QString("Output File configuration"));
}


void
ConfigureDialog::connectSignals() {
    connect(pButtonBox, SIGNAL(accepted()),
            this, SLOT(onOk()));
    connect(pButtonBox, SIGNAL(rejected()),
            this, SLOT(onCancel()));
}


void
ConfigureDialog::onCancel() {
    pTabK236->restoreSettings();
    pTabLS330->restoreSettings();
    pTabCS130->restoreSettings();
    pTabFile->restoreSettings();
    reject();
}


void
ConfigureDialog::onOk() {
    if(pTabFile->checkFileName()) {
        pTabK236->saveSettings();
        pTabLS330->saveSettings();
        pTabCS130->saveSettings();
        pTabFile->saveSettings();
        accept();
    }
    pTabWidget->setCurrentIndex(iFileIndex);
}



