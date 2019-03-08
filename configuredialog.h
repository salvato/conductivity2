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

#include <QDialog>

QT_FORWARD_DECLARE_CLASS(MainWindow)
QT_FORWARD_DECLARE_CLASS(QTabWidget)
QT_FORWARD_DECLARE_CLASS(QDialogButtonBox)
QT_FORWARD_DECLARE_CLASS(K236Tab)
QT_FORWARD_DECLARE_CLASS(LS330Tab)
QT_FORWARD_DECLARE_CLASS(CS130Tab)
QT_FORWARD_DECLARE_CLASS(FileTab)

class ConfigureDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConfigureDialog(int iConfiguration, MainWindow *parent = nullptr);

signals:

public slots:
    void onCancel();
    void onOk();

protected:
    void connectSignals();
    void setToolTips();

public:
    K236Tab  *pTabK236;
    LS330Tab *pTabLS330;
    CS130Tab *pTabCS130;
    FileTab  *pTabFile;

private:
    MainWindow       *pParent;
    QTabWidget       *pTabWidget;
    QDialogButtonBox *pButtonBox;

private:
    int iSourceIndex;
    int iThermIndex;
    int iMonoIndex;
    int iFileIndex;
    int configurationType;
};

