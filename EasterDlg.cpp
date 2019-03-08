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
#include "EasterDlg.h"

#include <QPainter>
#include <QFontMetrics>

#define getrandom(min, max) ((rand()%int((((max)+1)-(min)))+(min)));


EasterDlg::EasterDlg(QWidget* pParent /*=NULL*/)
    : QDialog(pParent)
{
    m_nScrollSpeed = 2;
    m_nStarsSpeed = 30;
    // initialize stars
    for(int i=0; i<NUM_STARS; i++) 	{
        m_StarArray[i].x = getrandom(0, 1024);
        m_StarArray[i].x -= 512;
        m_StarArray[i].y = getrandom(0, 1024);
        m_StarArray[i].y -= 512;
        m_StarArray[i].z = getrandom(0, 512);
        m_StarArray[i].z -= 256;
    }
    m_nScrollPos = height();
    connect(&scrollTimer, SIGNAL(timeout()),
            this, SLOT(update()));
    scrollTimer.start(50);
}


QSize
EasterDlg::minimumSizeHint() const {
   return QSize(350, 350);
}


QSize
EasterDlg::sizeHint() const {
   return QSize(500, 500);
}

void
EasterDlg::resizeEvent(QResizeEvent* pEvent) {
    Q_UNUSED(pEvent)
    m_nScrollPos = height();
}


void
EasterDlg::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPixmap pixmap(width(), height());
    QPainter painter;
    painter.begin(&pixmap);
    painter.fillRect(rect(), QBrush(QColor(0, 0, 0)));
    painter.setFont(QFont("Tahoma", 24, QFont::Normal));
    DoScrollText(&painter);
    QPainter window;
    window.begin(this);
    window.fillRect(rect(), QBrush(QColor(0, 0, 0)));
    // shrink text from bottom to top to create Star Wars effect
    for(int y=0; y<height(); y++) {
        double nScale = double(y)/double(height());
        int nOffset = int(width() - width()*nScale)/2;
        window.drawPixmap(nOffset, y, int(width()*nScale), 1, pixmap,
                          0, y, width(), 1);
    }
    DoStars(&window);
    painter.end();
    window.end();
}


void
EasterDlg::DoStars(QPainter* pPainter) {
    int nFunFactor = 100;
    int x, y, z;
    for(int i=0; i<NUM_STARS; i++) {
        m_StarArray[i].z = m_StarArray[i].z - m_nStarsSpeed;
        if(m_StarArray[i].z > 255)  m_StarArray[i].z =-255;
        if(m_StarArray[i].z < -255) m_StarArray[i].z = 255;
        z = m_StarArray[i].z + 256;
        x = (m_StarArray[i].x * nFunFactor / z) + (width()/2);
        y = (m_StarArray[i].y * nFunFactor / z) + (height()/2);
        // create a white brush which luminosity depends on the z position (for 3D effect!)
        int nColor = (255 - m_StarArray[i].z)/2;
        pPainter->setBrush(QColor(255, 255, 255, nColor));
        // draw star
        pPainter->drawEllipse(x, y, 3, 3);
  }
}


void
EasterDlg::DoScrollText(QPainter* pPainter) {
    int nPosX =0;
    int nPosY =0;
    QFontMetrics fontMetrics = pPainter->fontMetrics();
    // draw Credits
    for(int i=0; i<BANNER_LINES; i++) {
        // set position for this line
        QRect textSize = fontMetrics.boundingRect(starBannerText[i]);
        nPosY = m_nScrollPos + (i * textSize.height());
        if(nPosY > 0) {
            nPosX = (width()/2) - (textSize.width()/2);
            textPen.setColor(RGBFor(nPosY));
            pPainter->setPen(textPen);
            // print text
            pPainter->drawText(nPosX, nPosY, starBannerText[i]);
        }	else {
            // start all over ...
            if(i == (BANNER_LINES-1)) {
                m_nScrollPos = height();
            }
        }
    }
    // move text up one pixel
    m_nScrollPos = m_nScrollPos - m_nScrollSpeed;
}

// ------------------ RGBFor -----------------------------
// return an RGB value for the distance in the range 0-255
QColor
EasterDlg::RGBFor(int distance) {
    int RGcomponent;
   // clip all distances to 0 thru 255.
   if(distance>255) distance = 255;
   if(distance<0)   distance = 0;
   // RED and GREEN are directly related to distance
   RGcomponent = distance;
   // B is always 0
   return (QColor(RGcomponent, RGcomponent, 0, 255));
} // RGBFor

