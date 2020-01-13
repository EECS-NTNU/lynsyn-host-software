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

#ifndef PROFLINE_H
#define PROFLINE_H

#include "lynsyn.h"
#include "measurement.h"

class ProfLine {

private:
  double runtime;
  double power;
  double energy;
  unsigned measurementCount;

  static bool lessThan(const Measurement &e1, const Measurement &e2) {
    return e1.time < e2.time;
  }

public:
  QString id;
  QVector<Measurement> measurements;

  ProfLine(QString id) {
    this->id = id;

    this->runtime = 0;
    this->power = 0;
    this->energy= 0;
    this->measurementCount = 0;
  }

  void addMeasurement(Measurement m) {
    double secondsSinceLast = lynsyn_cyclesToSeconds(m.timeSinceLast);

    this->runtime += secondsSinceLast;
    this->power += m.power;
    this->energy += m.power * secondsSinceLast;
    measurements.push_back(m);
    measurementCount++;
  }

  QVector<Measurement> *getMeasurements() {
    std::sort(measurements.begin(), measurements.end(), lessThan);
    return &measurements;
  }

  double getPower() {
    return power / measurementCount;
  }
  double getEnergy() { return energy; }
  double getRuntime() { return runtime; }

};

#endif
