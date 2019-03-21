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
#include "hp3478tab.h"
#include "ls330tab.h"
#include "cs130tab.h"
#include "filetab.h"
#include "mainwindow.h"

#include <QTabWidget>
#include <QDialogButtonBox>
#include <QVBoxLayout>


ConfigureDialog::ConfigureDialog(int iConfiguration, MainWindow *parent)
    : QDialog(parent)
    , pTabK236(Q_NULLPTR)
    , pTabHp3478(Q_NULLPTR)
    , pTabLS330(Q_NULLPTR)
    , pTabCS130(Q_NULLPTR)
    , pTabFile(Q_NULLPTR)
    , pParent(parent)
    , configurationType(iConfiguration)
{
    pTabWidget   = new QTabWidget();
    pTabFile     = new FileTab(configurationType, this);
    iFileIndex   = pTabWidget->addTab(pTabFile,  tr("Out File"));
    if(pParent->bUseKeithley236) {
        pTabK236  = new K236Tab(configurationType, this);
        iSourceIndex = pTabWidget->addTab(pTabK236,  tr("K236"));
    }
    if(pParent->bUseHp3478) {
        pTabHp3478  = new hp3478Tab(configurationType, this);
        iSourceIndex = pTabWidget->addTab(pTabHp3478,  tr("Hp3478A"));
    }
    if(pParent->bUseLakeShore330) {
        pTabLS330 = new LS330Tab(configurationType, this);
        iThermIndex  = pTabWidget->addTab(pTabLS330, tr("LS330"));
    }
    if(pParent->bUseMonochromator) {
        pTabCS130 = new CS130Tab(configurationType, pParent->bUseMonochromator, this);
        iMonoIndex   = pTabWidget->addTab(pTabCS130, tr("CS130"));
    }

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
//        if(pTabLS330) pTabLS330->setDisabled(true);
        if(pTabCS130) pTabCS130->setDisabled(true);
        break;
    case MainWindow::iConfLScan:
        setWindowTitle("Lambda Scan");
        break;
    default:
        reject();
    }
    connectSignals();
    setToolTips();
}


void
ConfigureDialog::setToolTips() {
    if(pTabFile)   pTabWidget->setTabToolTip(iFileIndex,   QString("Output File configuration"));
    if(pTabK236)   pTabWidget->setTabToolTip(iSourceIndex, QString("Source-Measure Unit configuration"));
    if(pTabHp3478) pTabWidget->setTabToolTip(iSourceIndex, QString("Hp3478A configuration"));
    if(pTabLS330)  pTabWidget->setTabToolTip(iThermIndex,  QString("Thermostat configuration"));
    if(pTabCS130)  pTabWidget->setTabToolTip(iMonoIndex,   QString("Monochromator configuration"));
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
    if(pTabK236)   pTabK236->restoreSettings();
    if(pTabHp3478) pTabHp3478->restoreSettings();
    if(pTabLS330)  pTabLS330->restoreSettings();
    if(pTabCS130)  pTabCS130->restoreSettings();
    if(pTabFile)   pTabFile->restoreSettings();
    reject();
}


void
ConfigureDialog::onOk() {
    if(pTabFile->checkFileName()) {
        if(pTabK236)   pTabK236->saveSettings();
        if(pTabHp3478) pTabHp3478->saveSettings();
        if(pTabLS330)  pTabLS330->saveSettings();
        if(pTabCS130)  pTabCS130->saveSettings();
        if(pTabFile)   pTabFile->saveSettings();
        accept();
    }
    pTabWidget->setCurrentIndex(iFileIndex);
}



