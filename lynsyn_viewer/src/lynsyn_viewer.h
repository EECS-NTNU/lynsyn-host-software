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

#ifndef LYNSYN_VIEWER_H
#define LYNSYN_VIEWER_H

#include <QVariant>
#include <QVector>
#include <QComboBox>

#define APP_NAME   "Lynsyn Viewer"
#define ORG_NAME   "lynsyn"
#define ORG_DOMAIN "ntnu.no"
#define VERSION    "1.3"

///////////////////////////////////////////////////////////////////////////////
// layout defines

#define NTNU_BLUE         QColor(0,   80,  158)
#define NTNU_YELLOW       QColor(241, 210, 130)
#define NTNU_CYAN         QColor(92,  190, 201)
#define NTNU_PEAR         QColor(213, 209, 14)
#define NTNU_CHICKPEA     QColor(223, 216, 197)
#define NTNU_LIGHT_BLUE   QColor(121, 162, 206)
#define NTNU_GREEN        QColor(201, 212, 178)
#define NTNU_BEIGE        QColor(204, 189, 143)
#define NTNU_PINK         QColor(173, 32,  142)
#define NTNU_LIGHT_GRAY   QColor(221, 231, 238)
#define NTNU_BROWN        QColor(144, 73,  45)
#define NTNU_PURPLE       QColor(85,  41,  136)
#define NTNU_ORANGE       QColor(245, 128, 37)

#define BACKGROUND_COLOR  Qt::white
#define FOREGROUND_COLOR  Qt::black

///////////////////////////////////////////////////////////////////////////////
// gui defines

#define SCALE_IN_FACTOR 1.25
#define SCALE_OUT_FACTOR 0.8

///////////////////////////////////////////////////////////////////////////////

enum MeasurementType {
  CURRENT = 0, VOLTAGE = 1, POWER = 2
};

#endif
