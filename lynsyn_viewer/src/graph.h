/******************************************************************************
 *
 *  This file is part of the Lynsyn host tools
 *
 *  Copyright 2019 Asbjørn Djupdal, NTNU
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef GRAPH_H
#define GRAPH_H

#include <QGraphicsScene>
#include <QtWidgets>
#include <QtGlobal>

#include "lynsyn_viewer.h"
#include "profile.h"

#define TOP_MARGIN 20

#define GRAPH_TEXT_SPACING 20

class Graph : public QGraphicsItem {
  QVector<QPoint> points;
  unsigned textWidth;
  unsigned textHeight;
  QFont font;

  unsigned lowMeasurement;
  unsigned highMeasurement;
  int64_t lowTime;
  int64_t highTime;

  double lowMeasurementValue;
  double highMeasurementValue;
  double lowTimeValue;
  double highTimeValue;

  MeasurementType measurementType;

public:
  Graph(QFont font, MeasurementType measurementType, 
        unsigned lowMeasurement, unsigned highMeasurement, int64_t lowTime, int64_t highTime,
        double lowMeasurementValue, double highMeasurementValue, double lowTimeValue, double highTimeValue,
        QGraphicsItem *parent = NULL) : QGraphicsItem(parent) {

    this->font = font;
    this->measurementType = measurementType;

    this->lowMeasurement = lowMeasurement;
    this->highMeasurement = highMeasurement;
    this->lowTime = lowTime;
    this->highTime = highTime;

    this->lowMeasurementValue = lowMeasurementValue;
    this->highMeasurementValue = highMeasurementValue;
    this->lowTimeValue = lowTimeValue;
    this->highTimeValue = highTimeValue;

    QFontMetrics fm(font);
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
    unsigned w1 = fm.width(QString::number(lowMeasurementValue) + "W");
    unsigned w2 = fm.width(QString::number(highMeasurementValue) + "W");
#else
    unsigned w1 = fm.horizontalAdvance(QString::number(lowMeasurementValue) + "W");
    unsigned w2 = fm.horizontalAdvance(QString::number(highMeasurementValue) + "W");
#endif
    if(w1 > w2) textWidth = w1;
    else textWidth = w2;
    textHeight = fm.height();
  }
  ~Graph() {}

  void addPoint(int64_t time, unsigned value) {
    points.push_back(QPoint(time, value));
  }

  double getPoint(int64_t time) {
    // binary search for point in sorted points-vector
    if(points.size()) {
      int start = 0;
      int end = points.size() - 1;

      while((end-start) > 1) {
        int middle = start + (end-start)/2;

        if(points[middle].x() > time) {
          end = middle;
        } else {
          start = middle;
        }
      }

      return points[start].y();
    } else {
      return 0;
    }
  }

  QRectF boundingRect() const {
    qreal penWidth = 1;
    return QRectF(-penWidth/2 - textWidth-GRAPH_TEXT_SPACING,
                  -(double)highMeasurement - TOP_MARGIN,
                  highTime + penWidth/2 + textWidth + GRAPH_TEXT_SPACING,
                  highMeasurement + textHeight + TOP_MARGIN);
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QString highTimeText = QString::number(highTimeValue) + "s";

    QFontMetrics fm(font);

    painter->setPen(QPen(NTNU_BLUE));

    // find distance between xticks
    double xStep = 1000;
    while(((highTimeValue - lowTimeValue) / xStep) < 20) {
      //printf("x %f %f %f\n", highTimeValue, lowTimeValue, xStep);
      xStep /= 10;
    }
    xStep *= 10;

    // find distance between yticks
    double yStep = 256;
    while(((highMeasurementValue - lowMeasurementValue) / yStep) < 10) {
      yStep /= 2;
    }
    yStep *= 2;

    // draw measurement axis
    painter->drawLine(0, 0, 0, -highMeasurement);

    // draw measurement ticks
    double measurementScale = highMeasurement / (highMeasurementValue - lowMeasurementValue);

    double low = goUp(lowMeasurementValue, yStep);
    double high = goDown(highMeasurementValue, yStep);
    if(low == high) {
      low -= 1;
      high += 1;
    }

    for(double i = low; i <= high; i += yStep) {
      double val = (double)(i-lowMeasurementValue)*measurementScale;
      painter->drawLine(-10, -val, 0, -val);
      QString unit;
      switch(measurementType) {
        case CURRENT: unit = "A"; break;
        case VOLTAGE: unit = "V"; break;
        case POWER:   unit = "W"; break;
      };
      painter->drawText((int)(-textWidth-GRAPH_TEXT_SPACING), -val + textHeight/2, QString::number(i) + unit);
    }

    // draw time axis
    painter->drawLine(0, 0, highTime, 0);

    // draw time ticks
    double timeScale = highTime / (highTimeValue - lowTimeValue);
    low = goUp(lowTimeValue, xStep);
    high = goDown(highTimeValue, xStep);
    if(low == high) {
      low -= 1;
      high += 1;
    }

    for(double i = low; i <= high; i += xStep) {
      double val = (double)(i-lowTimeValue)*timeScale;
      painter->drawLine(val, 0, val, 10);
      painter->drawText((int)val, textHeight, QString::number(i) + "s");
    }

    // draw graph
    painter->setPen(QPen(FOREGROUND_COLOR));
    QPoint prevPoint = QPoint(0,0);
    for(auto point : points) {
      painter->drawLine(QPoint(prevPoint.x(), -prevPoint.y()), QPoint(point.x(), -point.y()));
      prevPoint = point;
    }
  }

  double goUp(double x, double step) {
    return (double)(ceil(x/step))*step;
  }

  double goDown(double x, double step) {
    return (double)(floor(x/step))*step;
  }

};

#endif
