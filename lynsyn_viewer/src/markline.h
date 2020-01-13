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

#ifndef MARKLINE_H
#define MARKLINE_H

#include <QGraphicsScene>
#include <QtWidgets>

#include "profile.h"

class MarkLine : public QGraphicsItem {

  int64_t timeStart;
  int64_t timeEnd;
  unsigned lineHeight;
  unsigned lineDepth;
  QColor color;

public:
  MarkLine(int64_t timeStart, int64_t timeEnd, unsigned lineHeight, unsigned lineDepth, QColor color, QGraphicsItem *parent = NULL) : QGraphicsItem(parent) {
    this->timeStart = timeStart;
    this->timeEnd = timeEnd;
    this->lineHeight = lineHeight;
    this->lineDepth = lineDepth;
    this->color = color;
  }

  ~MarkLine() {}

  QRectF boundingRect() const {
    return QRectF(timeStart, -(int)lineHeight, timeEnd - timeStart, lineHeight + lineDepth);
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    painter->setPen(QPen(color));
    painter->setBrush(QBrush(color, Qt::SolidPattern));
    painter->drawRect(timeStart, -(int)lineHeight, timeEnd - timeStart, lineHeight + lineDepth);
  }

};

#endif
