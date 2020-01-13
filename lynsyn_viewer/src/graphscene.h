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

#ifndef GRAPHSCENE_H
#define GRAPHSCENE_H

#include <QGraphicsScene>
#include <QtWidgets>

#include "profile.h"
#include "graph.h"
#include "ganttline.h"
#include "markline.h"
#include "profline.h"
#include "measurement.h"

#define LINE_SPACING 30

/* class MovingAverage { */

/* private: */
/*   double *buf; */
/*   unsigned head; */
/*   unsigned size; */

/* public: */
/*   MovingAverage(unsigned size) { */
/*     this->size = size; */
/*     buf = new double[size+1]; */
/*     head = 0; */
/*   } */
/*   ~MovingAverage() { */
/*     delete[] buf; */
/*   } */
/*   void initialize(double d) { */
/*     buf[0] = 0; */
/*     for(unsigned i = 1; i < size+1; i++) { */
/*       buf[i] = buf[i-1] + d; */
/*     } */
/*   } */
/*   double next(double d) { */
/*     if(head == 0) { */
/*       buf[0] = buf[size] + d; */
/*     } else { */
/*       buf[head] = buf[head-1] + d; */
/*     } */

/*     head++; */
/*     if(head > size) head = 0; */

/*     if(head == 0) { */
/*       return (buf[size] - buf[0]) / size; */
/*     } else { */
/*       return (buf[head-1] - buf[head]) / size; */
/*     } */
/*   } */
/* }; */

class Point {
public:
  int64_t time;
  double measurement;
  Point() {}
  Point(int64_t t, double p) {
    time = t;
    measurement = p;
  }
};

class GraphScene : public QGraphicsScene {
  Q_OBJECT

private:
  Graph *graph;
  QVector<GanttLine*> ganttLines;
  unsigned currentCore;
  unsigned currentSensor;
  MeasurementType currentMeasurement;

  void addGanttLineSegments(int line, QVector<Measurement> *measurements);
  int64_t scaleTime(int64_t time);
  double scaleMeasurement(double measurement);
  int addGanttLine(QString id, QColor color);
  void addGanttLineSegment(unsigned lineNum, int64_t start, int64_t stop);
  void addPoint(int64_t time, double value);
  void addMarkLine(int64_t timeStart, int64_t timeEnd, unsigned depth, QColor color);
  void buildProfTable(QVector<Measurement> *measurements, std::vector<ProfLine*> &table);

public:
  int64_t minTime;
  int64_t maxTime;
  int64_t minTimeDb;
  int64_t maxTimeDb;
  double minMeasurement;
  double maxMeasurement;

  Profile *profile;
  unsigned scaleFactorTime;
  unsigned scaleFactorMeasurement;
  double minMeasurementIncrement;
  double maxMeasurementIncrement;

  GraphScene(QObject *parent = 0);
  ~GraphScene() {}

  void drawProfile(unsigned core, unsigned sensor, MeasurementType measurement, Profile *profile, int64_t beginTime = -1, int64_t endTime = -1);
  void redraw();
  void redrawFull();
  int64_t posToTime(double pos);
  double posToSeconds(double pos);
  double posToMeasurement(double pos);
  void clearScene() {
    profile = NULL;
    clear();
    ganttLines.clear();
    update();
  }
};

#endif
