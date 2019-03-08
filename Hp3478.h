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

#include <QtGlobal>

#include <QObject>
#include <QDateTime>
#include <QTimer>
#include "gpibdevice.h"

class Hp3478: public GpibDevice
{
    Q_OBJECT

public:
    explicit Hp3478(int gpio, int address, QObject *parent = Q_NULLPTR);
    virtual ~Hp3478();

signals:
    void     newReading(QDateTime currentTime, QString sReading);

public slots:
    void     checkNotify();

public:
    int      init();
    void     onGpibCallback(int LocalUd, unsigned long LocalIbsta, unsigned long LocalIberr, long LocalIbcntl);
    int      endRvsTime();
    int      initRvsTime();
    bool     sendTrigger();
    bool     isReadyForTrigger();

private:
};

