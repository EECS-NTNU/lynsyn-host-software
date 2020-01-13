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

#ifndef PROFILEDIALOG_H
#define PROFILEDIALOG_H

#include <QDialog>

#include "ui_profiledialog.h"

namespace Ui {
  class ProfileDialog;
}

class ProfileDialog : public QDialog {
  Q_OBJECT

public:
  Ui::ProfileDialog *ui;

  unsigned numCores;
  unsigned numSensors;
  bool hasVoltageSensors;
  bool useJtag; 

  QVector<QCheckBox*> coreCheckboxes;
  QVector<QDoubleSpinBox*> sensorSpinboxes;

  explicit ProfileDialog(unsigned numCores, bool hasVoltageSensors, unsigned numSensors, bool useJtag, QWidget *parent = 0);
  ~ProfileDialog();
};

#endif // PROFILEDIALOG_H
