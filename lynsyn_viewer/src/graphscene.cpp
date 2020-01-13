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

#include "graphscene.h"
#include "config.h"
#include "profmodel.h"

#define GANTT_SPACING 20
#define GRAPH_SIZE (scaleFactorMeasurement + GANTT_SPACING)

GraphScene::GraphScene(QObject *parent) : QGraphicsScene(parent) {
  scaleFactorTime = 1000;
  scaleFactorMeasurement = 200;
  profile = NULL;
  graph = NULL;
  minMeasurementIncrement = 0;
  maxMeasurementIncrement = 0;
  currentMeasurement = POWER;
}

void GraphScene::addGanttLineSegments(int line, QVector<Measurement> *measurements) {
  int64_t startTime = -1;
  int64_t lastTime = -1;
  uint64_t lastSeq = -2;
  for(auto measurement : *measurements) {
    int64_t time = measurement.time;
    uint64_t seq = measurement.sequence;
    if(seq != lastSeq+1) {
      if(startTime != (int64_t)~0) {
        addGanttLineSegment(line, startTime, lastTime);
      }
      startTime = time;
    }
    lastTime = time;
    lastSeq = seq;
  }
  if(measurements->size()) {
    addGanttLineSegment(line, startTime, lastTime);
  }
}

void GraphScene::drawProfile(unsigned core, unsigned sensor, MeasurementType measurement, Profile *profile, int64_t beginTime, int64_t endTime) {
  clear();
  ganttLines.clear();

  this->currentCore = core;
  this->currentSensor = sensor;
  this->currentMeasurement = measurement;

  this->profile = profile;

  QVector<Measurement> *measurements = new QVector<Measurement>;

  if(profile) {
    QSqlDatabase db = QSqlDatabase::database(profile->dbConnection);
    QSqlQuery query(db);

    QString sensorString = QString::number(sensor+1);

    query.exec(QString() + "SELECT mintime,maxtime,mincurrent" + sensorString + ",maxcurrent" + sensorString + ",minvoltage" + sensorString + ",maxvoltage" + sensorString + ",minpower" + sensorString + ",maxpower" + sensorString + ",samples FROM meta");

    if(query.next()) {
      uint64_t samples = query.value("samples").toDouble();

      switch(currentMeasurement) {
        case CURRENT: {
          minMeasurement = query.value("mincurrent" + sensorString).toDouble() + minMeasurementIncrement;
          maxMeasurement = query.value("maxcurrent" + sensorString).toDouble() + maxMeasurementIncrement;
          break;
        }
        case VOLTAGE: {
          minMeasurement = query.value("minvoltage" + sensorString).toDouble() + minMeasurementIncrement;
          maxMeasurement = query.value("maxvoltage" + sensorString).toDouble() + maxMeasurementIncrement;
          break;
        }
        case POWER: {
          minMeasurement = query.value("minpower" + sensorString).toDouble() + minMeasurementIncrement;
          maxMeasurement = query.value("maxpower" + sensorString).toDouble() + maxMeasurementIncrement;
          break;
        }
      }

      minTimeDb = query.value("mintime").toLongLong();
      maxTimeDb = query.value("maxtime").toLongLong();

      if(samples > 1) {
        if(beginTime < minTimeDb) minTime = minTimeDb;
        else minTime = beginTime;

        if(endTime < 0) maxTime = maxTimeDb;
        else maxTime = endTime;

        graph = new Graph(font(), currentMeasurement, 
                          scaleMeasurement(minMeasurement), scaleMeasurement(maxMeasurement),
                          scaleTime(minTime), scaleTime(maxTime),
                          minMeasurement, maxMeasurement, lynsyn_cyclesToSeconds(minTime-minTimeDb), lynsyn_cyclesToSeconds(maxTime-minTimeDb));
        graph->setPos(0, GRAPH_SIZE-GANTT_SPACING);
        addItem(graph);
        graph->setZValue(10);

        unsigned ticksPerSample = (maxTimeDb - minTimeDb) / samples;
        int64_t ticks = maxTime - minTime;
        uint64_t samplesInWindow = ticks / ticksPerSample;
        int stride = samplesInWindow / scaleFactorTime;
        if(stride < 1) stride = 1;

        QString queryString = QString() +
          "SELECT time,timeSinceLast,function" + QString::number(core+1) +
          ",module" + QString::number(core+1) +
          ",current" + QString::number(sensor+1) +
          ",voltage" + QString::number(sensor+1) +
          " FROM measurements" +
          " WHERE time BETWEEN " + QString::number(minTime) + " AND " + QString::number(maxTime) + 
          " AND rowid % " + QString::number(stride) + " = 0";

        query.exec(queryString);

        //MovingAverage ma(Config::window);

        if(query.next()) {
          //bool initialized = false;
        
          do {
            int64_t time = query.value("time").toLongLong();
            int64_t timeSinceLast = query.value("timeSinceLast").toLongLong();

            double current = query.value("current" + QString::number(sensor+1)).toDouble();
            double voltage = query.value("voltage" + QString::number(sensor+1)).toDouble();
            double measurement = current;
            switch(currentMeasurement) {
              case CURRENT: measurement = current; break;
              case VOLTAGE: measurement = voltage; break;
              case POWER:   measurement = current * voltage; break;
            }

            QString module = query.value("module" + QString::number(core+1)).toString();
            QString function = query.value("function" + QString::number(core+1)).toString();

            // if(!initialized) {
            //   ma.initialize(measurement);
            //   initialized = true;
            // }

            // double avg = ma.next(measurement);

            addPoint(time, measurement);

            measurements->push_back(Measurement(time, timeSinceLast, measurement, module, function));
          
          } while(query.next());
        }

        unsigned ganttSize = 0;

        std::vector<ProfLine*> table;
        profile->buildProfTable(measurements, table);
        ProfSort profSort;
        std::sort(table.begin(), table.end(), profSort);

        for(auto profLine : table) {
          if(profLine->measurements.size() > 0) {
            int l = addGanttLine(profLine->id, NTNU_BLUE);
            addGanttLineSegments(l, profLine->getMeasurements());
            ganttSize = GANTT_SPACING + (l+1) * LINE_SPACING;
          }
        }

        queryString = QString() +
          "SELECT time,delay FROM marks" +
          " WHERE time BETWEEN " + QString::number(minTime) + " AND " + QString::number(maxTime);

        query.exec(queryString);

        while(query.next()) {
          int64_t time = query.value("time").toLongLong();
          int64_t delay = query.value("delay").toLongLong();
          addMarkLine(time, time + delay, ganttSize, NTNU_YELLOW);
        }
      }
    }
  }

  update();
}

void GraphScene::redraw() {
  drawProfile(currentCore, currentSensor, currentMeasurement, profile, minTime, maxTime);
}

void GraphScene::redrawFull() {
  drawProfile(currentCore, currentSensor, currentMeasurement, profile);
}

int GraphScene::addGanttLine(QString id, QColor color) {
  int lineNum = ganttLines.size();

  GanttLine *line = new GanttLine(id, font(), color);
  line->setPos(0, GRAPH_SIZE + (lineNum+1) * LINE_SPACING);
  addItem(line);
  ganttLines.push_back(line);

  return lineNum;
}

void GraphScene::addGanttLineSegment(unsigned lineNum, int64_t start, int64_t stop) {
  ganttLines[lineNum]->addLine(scaleTime(start), scaleTime(stop));
}

void GraphScene::addPoint(int64_t time, double value) {
  graph->addPoint(scaleTime(time), scaleMeasurement(value));
}

void GraphScene::addMarkLine(int64_t timeStart, int64_t timeEnd, unsigned depth, QColor color) {
  MarkLine *line = new MarkLine(scaleTime(timeStart), scaleTime(timeEnd), scaleFactorMeasurement, depth, color);
  line->setPos(0, GRAPH_SIZE-GANTT_SPACING);
  addItem(line);
}

int64_t GraphScene::scaleTime(int64_t time) {
  return (int64_t)(scaleFactorTime * (double)(time - minTime) / (double)(maxTime - minTime));
}

double GraphScene::scaleMeasurement(double measurement) {
  return scaleFactorMeasurement * (double)(measurement - minMeasurement) / (double)(maxMeasurement - minMeasurement);
}

int64_t GraphScene::posToTime(double pos) {
  if(!graph) return 0;
  return (pos / scaleFactorTime) * (maxTime-minTime) + minTime;
}

double GraphScene::posToSeconds(double pos) {
  if(!graph) return 0;
  return lynsyn_cyclesToSeconds(posToTime(pos) - minTimeDb);
}

double GraphScene::posToMeasurement(double pos) {
  if(!graph) return 0;
  return (graph->getPoint(pos) / scaleFactorMeasurement) * (maxMeasurement-minMeasurement) + minMeasurement;
}
