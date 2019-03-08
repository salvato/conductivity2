#pragma once
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

#include <QObject>
#include <QDialog>
#include <QPen>
#include <QTimer>


QT_FORWARD_DECLARE_CLASS(QPainter)


class 
EasterDlg : public QDialog {
    Q_OBJECT
public:
    explicit EasterDlg(QWidget *parent = Q_NULLPTR);
    QSize minimumSizeHint() const;
    QSize sizeHint() const;

protected:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent* pEvent);
    void DoStars(QPainter* pPainter);
    void DoScrollText(QPainter *pPainter);
    QColor RGBFor(int distance);

protected:
    QTimer scrollTimer;
    QPen textPen;
    int m_nStarsSpeed;
    int m_nScrollSpeed;
    int m_nScrollPos;
    struct Star {
        int x;
        int y;
        int z;
    };
    static const int NUM_STARS  = 100;
    Star m_StarArray[NUM_STARS];

    static const int BANNER_LINES = 21;
    const char starBannerText[BANNER_LINES][50] =
    {
       "If this code works,",        //0
       "it was written by",          //1
       "",                           //2
       "Gabriele Salvato ©",         //3
       "gabriele.salvato@cnr.it",    //4
       "",                           //5
       "If not...",                  //6
       ".",                          //7
       ".",                          //8
       ".",                          //9
       "I don't know who wrote it !",//10
       ":-)",                        //11
       "Credits:",                   //12
       "Pablo van der Meer",         //13
       "for this control",           //14
       "",                           //15
       "Paul DiLascia",              //16
       "for \"...if this code...\"", //17
       "",                           //18
       "Hit Esc to exit",            //19
       ".",                          //20
    };
};

