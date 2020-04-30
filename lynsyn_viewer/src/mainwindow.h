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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "lynsyn_viewer.h"
#include "graphscene.h"
#include "graphview.h"
#include "profmodel.h"

namespace Ui {
  class MainWindow;
}

class MainWindow : public QMainWindow {
  Q_OBJECT

  Profile *profile;
  GraphScene *graphScene;
  GraphView *graphView;
  QComboBox *measurementBox;
  QComboBox *coreBox;
  QComboBox *sensorBox;
  ProfileDialog *profDialog;
  QTableView *tableView;
  ProfModel *profModel;

  QThread thread;
  QProgressDialog *progDialog;

  void updateComboboxes();

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

private slots:
  void about();
  void showHwInfo();
  void importCsv();
  void exportCsv();
  void upgrade();
  void profileEvent();
  void finishProfile(int error, QString msg);
  void advance(int step, QString msg);
  void changeCore(int core);
  void changeSensor(int sensor);
  void changeMeasurement(int measurement);
  void showLive();
  void jtagDiagnostic();

protected:
  void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

private:
  Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
