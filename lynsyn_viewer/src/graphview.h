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

#ifndef GRAPHVIEW_H
#define GRAPHVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QMouseEvent>
#include <QScrollBar>
#include <QApplication>

#include "graphscene.h"

class GraphView : public QGraphicsView {
  Q_OBJECT

protected:
  GraphScene *scene;
  int64_t beginTime;
  QStatusBar *statusBar;

  void mousePressEvent(QMouseEvent *mouseEvent) {
    if(mouseEvent->button() == Qt::LeftButton) {
      QPointF pos = mapToScene(mouseEvent->pos());
      beginTime = scene->posToTime(pos.x());

      QString statusMessage = QString::asprintf("Time: %.2fs Measurement: %.2f\n", scene->posToSeconds(pos.x()), scene->posToMeasurement(pos.x()));
      statusBar->showMessage(statusMessage);

    } else if(mouseEvent->button() == Qt::RightButton) {
      scene->redrawFull();
      viewport()->update();
    }
  }

  void mouseReleaseEvent(QMouseEvent *mouseEvent) {
    if(mouseEvent->button() == Qt::LeftButton) {
      QPointF pos = mapToScene(mouseEvent->pos());
      int64_t endTime = scene->posToTime(pos.x());

      if(beginTime < endTime) {
        scene->minTime = beginTime;
        scene->maxTime = endTime;
      } else if(beginTime > endTime) {
        scene->minTime = endTime;
        scene->maxTime = beginTime;
      } 

      scene->redraw();
      viewport()->update();
    }
  }

  void wheelEvent(QWheelEvent *event) {
    if(event->modifiers() & Qt::ControlModifier) {
      if(event->delta() > 0) zoomInEvent();
      else zoomOutEvent();
      event->accept();
    } else if(event->modifiers() & Qt::ShiftModifier) {
      if(event->delta() > 0) measurementMaxDecEvent();
      else measurementMaxIncEvent();
      event->accept();
    } else if(event->modifiers() & Qt::AltModifier) {
      if(event->delta() > 0) measurementMinDecEvent();
      else measurementMinIncEvent();
      event->accept();
    } else {
      QGraphicsView::wheelEvent(event);
    }
  }

  void keyPressEvent(QKeyEvent *event) {
    if(event->modifiers() & Qt::ControlModifier) {
      if(event->key() == Qt::Key_Plus) {
        zoomInEvent();
      }
      if(event->key() == Qt::Key_Minus) {
        zoomOutEvent();
      }
    }
    event->accept();
  }

public:
  GraphView(GraphScene *scene, QStatusBar *statusBar) : QGraphicsView(scene) {
    this->scene = scene;
    this->statusBar = statusBar;
    setBackgroundBrush(QBrush(BACKGROUND_COLOR, Qt::SolidPattern));
  }

public slots:
  void zoomInEvent() {
    if(scene->profile) {
      QPointF centerPoint = mapToScene(viewport()->width()/2, viewport()->height()/2);
    
      scene->scaleFactorTime *= SCALE_IN_FACTOR;
      if(scene->scaleFactorTime == 0) scene->scaleFactorTime = 10;
      scene->redraw();
      viewport()->update();

      centerOn(centerPoint.x() * SCALE_IN_FACTOR, centerPoint.y());
    }
  }
  void zoomOutEvent() {
    if(scene->profile) {
      QPointF centerPoint = mapToScene(viewport()->width()/2, viewport()->height()/2);

      scene->scaleFactorTime *= SCALE_OUT_FACTOR;
      scene->redraw();
      viewport()->update();

      centerOn(centerPoint.x() * SCALE_OUT_FACTOR, centerPoint.y());
    }
  }
  void measurementMaxDecEvent() {
    if(scene->profile) {
      double increment = (scene->maxMeasurement - scene->minMeasurement) / 10;
      scene->maxMeasurementIncrement -= increment;
      scene->redraw();
      viewport()->update();
    }
  }
  void measurementMaxIncEvent() {
    if(scene->profile) {
      double increment = (scene->maxMeasurement - scene->minMeasurement) / 10;
      scene->maxMeasurementIncrement += increment;
      scene->redraw();
      viewport()->update();
    }
  }
  void measurementMinDecEvent() {
    if(scene->profile) {
      double increment = (scene->maxMeasurement - scene->minMeasurement) / 10;
      scene->minMeasurementIncrement -= increment;
      scene->redraw();
      viewport()->update();
    }
  }
  void measurementMinIncEvent() {
    if(scene->profile) {
      double increment = (scene->maxMeasurement - scene->minMeasurement) / 10;
      scene->minMeasurementIncrement += increment;
      scene->redraw();
      viewport()->update();
    }
  }
};

#endif
