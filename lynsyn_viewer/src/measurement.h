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

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <QVector>
#include <QFile>

#include "lynsyn.h"
#include "lynsyn_viewer.h"

class Measurement {
public:
  quint32 sequence;
  quint64 time;
  quint64 timeSinceLast;

  double power;

  QString id;

  static unsigned counter;

  Measurement() {}

  Measurement(uint64_t time, uint64_t timeSinceLast, double power, QString module, QString function) {
    this->id = module + ":" + function;
    this->time = time;
    this->timeSinceLast = timeSinceLast;
    this->power = power;
    sequence = Measurement::counter++;
  }

};

#endif
