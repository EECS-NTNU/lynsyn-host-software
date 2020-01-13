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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "lynsyn.h"
#include "mainwindow.h"
#include "config.h"
#include "profile.h"
#include "profiledialog.h"
#include "importdialog.h"
#include "ui_importdialog.h"

void MainWindow::updateComboboxes() {
  sensorBox->clear();
  for(unsigned i = 0; i < profile->numSensors; i++) {
    sensorBox->addItem(QString("Sensor ") + QString::number(i+1));
  }
  if(Config::sensor >= (unsigned)sensorBox->count()) Config::sensor = 0;
  if(sensorBox->count() != 0) sensorBox->setCurrentIndex(Config::sensor);
  
  coreBox->clear();
  for(unsigned i = 0; i < profile->numCores; i++) {
    coreBox->addItem(QString("Core ") + QString::number(i));
  }
  if(Config::core >= (unsigned)coreBox->count()) Config::core = 0;
  if(coreBox->count() != 0) coreBox->setCurrentIndex(Config::core);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);

  profile = new Profile("lynsyn_profile.db");
  connect(profile, SIGNAL(advance(int, QString)), this, SLOT(advance(int, QString)), Qt::BlockingQueuedConnection);

  profile->connect();

  progDialog = NULL;
  profDialog = NULL;

  measurementBox = new QComboBox();
  measurementBox->addItem("Current");
  measurementBox->addItem("Voltage");
  measurementBox->addItem("Power");
  connect(measurementBox, SIGNAL(activated(int)), this, SLOT(changeMeasurement(int)));
  ui->toolBar->addWidget(measurementBox);

  sensorBox = new QComboBox();
  connect(sensorBox, SIGNAL(activated(int)), this, SLOT(changeSensor(int)));
  ui->toolBar->addWidget(sensorBox);

  coreBox = new QComboBox();
  connect(coreBox, SIGNAL(activated(int)), this, SLOT(changeCore(int)));
  ui->toolBar->addWidget(coreBox);

  graphScene = new GraphScene(this);
  graphView = new GraphView(graphScene, statusBar());
  ui->tabWidget->addTab(graphView, "Profile Graph");

  tableView = new QTableView();
  tableView->setSortingEnabled(true);
  ui->tabWidget->addTab(tableView, "Profile Table");

  // statusbar
  statusBar()->showMessage("Ready");

  // title
  setWindowTitle(APP_NAME);

  // settings
  QSettings settings;
  restoreGeometry(settings.value("geometry").toByteArray());
  restoreState(settings.value("windowState").toByteArray(), VERSION);

  Config::core = settings.value("core", 0).toUInt();
  Config::sensor = settings.value("sensor", 0).toUInt();
  Config::measurement = settings.value("measurement", 0).toUInt();

  updateComboboxes();

  measurementBox->setCurrentIndex(Config::measurement);

  //---------------------------------------------------------------------------

  profModel = new ProfModel(profile);
  profModel->recalc(Config::core, Config::sensor);
  tableView->setModel(profModel);
  tableView->sortByColumn(0, Qt::AscendingOrder);
  tableView->horizontalHeader()->restoreState(settings.value("tableViewState").toByteArray());

  graphScene->drawProfile(Config::core, Config::sensor, (MeasurementType)Config::measurement, profile);
}

MainWindow::~MainWindow() {
  delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
  QSettings settings;
  settings.setValue("geometry", saveGeometry());
  settings.setValue("windowState", saveState(VERSION));
  settings.setValue("core", Config::core);
  settings.setValue("sensor", Config::sensor);
  settings.setValue("measurement", Config::measurement);
  if(tableView->model()) {
    settings.setValue("tableViewState", tableView->horizontalHeader()->saveState());
  }

  QMainWindow::closeEvent(event);
}

void MainWindow::about() {
  QMessageBox::about(this, "About Lynsyn Viewer",
                     QString("Lynsyn Viewer V") + QString::number(VERSION) + "\n"
                     "A performance measurement and analysis utility\n"
                     "Copyright 2020 NTNU\n"
                     "License: GPL Version 3");
}

void MainWindow::importCsv() {
  ImportDialog importDialog;
  if(importDialog.exec()) {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    if(profile->importCsv(importDialog.ui->csvEdit->text(),
                          importDialog.ui->elfEdit->text().split(','),
                          importDialog.ui->kallsymsEdit->text())) {

      graphScene->clearScene();

      tableView->setModel(NULL);
      profModel->recalc(Config::core, Config::sensor);
      tableView->setModel(profModel);
      QSettings settings;
      tableView->horizontalHeader()->restoreState(settings.value("tableViewState").toByteArray());

      graphScene->drawProfile(Config::core, Config::sensor, (MeasurementType)Config::measurement, profile);

      updateComboboxes();

      QApplication::restoreOverrideCursor();
      QMessageBox msgBox;
      msgBox.setText("Import done");
      msgBox.exec();
    } else {
      QApplication::restoreOverrideCursor();
      QMessageBox msgBox;
      msgBox.setText("Can't import");
      msgBox.exec();
    }
  }
}

void MainWindow::exportCsv() {
  QFileDialog dialog(this, "Select CSV file for export");
  dialog.setNameFilter(tr("CSV (*.csv)"));
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  if(dialog.exec()) {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    if(profile->exportCsv(dialog.selectedFiles()[0])) {
      QApplication::restoreOverrideCursor();
      QMessageBox msgBox;
      msgBox.setText("Export done");
      msgBox.exec();
    } else {
      QApplication::restoreOverrideCursor();
      QMessageBox msgBox;
      msgBox.setText("Can't export");
      msgBox.exec();
    }
  }
}

void MainWindow::upgrade() {
  QFileDialog dialog(this, "Select PMU firmware file");
  dialog.setNameFilter(tr("Firmware binary (*.bin)"));
  if(dialog.exec()) {
    QFile file(dialog.selectedFiles()[0]);
    if(!file.open(QIODevice::ReadOnly)) {
      QMessageBox msgBox;
      msgBox.setText("Can't open file");
      msgBox.exec();
      return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QByteArray array = file.readAll();
    if(array.size()) {
      if(!lynsyn_firmwareUpgrade(array.size(), (uint8_t*)array.data())) {
        QApplication::restoreOverrideCursor();
        QMessageBox msgBox;
        msgBox.setText("Can't upgrade firmware");
        msgBox.exec();
      } else {
        QApplication::restoreOverrideCursor();
        QMessageBox msgBox;
        msgBox.setText("Firmware upgraded");
        msgBox.exec();
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void MainWindow::advance(int step, QString message) {
  if(progDialog) {
    progDialog->setValue(step);
    progDialog->setLabelText(message);
  }
}

void MainWindow::profileEvent() {
  bool useJtag;

  if(profile->initProfiler(&useJtag)) {
    double r[LYNSYN_MAX_SENSORS];
    uint8_t hwVersion;
    uint8_t bootVersion;
    uint8_t swVersion;

    if(lynsyn_getInfo(&hwVersion, &bootVersion, &swVersion, r)) {
      profDialog = new ProfileDialog(profile->cores(), hwVersion >= HW_VERSION_3_0, profile->sensors(), useJtag);
      profDialog->exec();

      if(profDialog->result() == QDialog::Accepted) {
        QApplication::setOverrideCursor(Qt::WaitCursor);

        profile->clean();
        profile->setParameters(profDialog);

        progDialog = new QProgressDialog("", QString(), 0, 3, this);
        progDialog->setWindowModality(Qt::WindowModal);
        progDialog->setMinimumDuration(0);
        progDialog->setValue(0);

        thread.wait();
        profile->moveToThread(&thread);
        connect(&thread, SIGNAL (started()), profile, SLOT (runProfiler()));
        connect(profile, SIGNAL(finished(int, QString)), this, SLOT(finishProfile(int, QString)));
        thread.start();
      } else {
        profile->endProfiler();
      }
    } else {
      profile->endProfiler();
      QMessageBox msgBox;
      msgBox.setText("Can't get HW info");
      msgBox.exec();
    }
  } else {
    QMessageBox msgBox;
    msgBox.setText("Can't find or initialise Lynsyn");
    msgBox.exec();
  }
}

void MainWindow::finishProfile(int error, QString msg) {
  delete progDialog;
  progDialog = NULL;
  
  delete profDialog;
  profDialog = NULL;

  profile->endProfiler();

  updateComboboxes();

  QApplication::restoreOverrideCursor();

  if(error) {
    QMessageBox msgBox;
    msgBox.setText(msg);
    msgBox.exec();

  } else {
    QMessageBox msgBox;
    msgBox.setText("Profiling done");
    msgBox.exec();
  }

  disconnect(&thread, SIGNAL (started()), 0, 0);
  disconnect(profile, SIGNAL(finished(int, QString)), 0, 0);

  thread.quit();

  graphScene->clearScene();

  tableView->setModel(NULL);
  profModel->recalc(Config::core, Config::sensor);
  tableView->setModel(profModel);
  QSettings settings;
  tableView->horizontalHeader()->restoreState(settings.value("tableViewState").toByteArray());

  graphScene->drawProfile(Config::core, Config::sensor, (MeasurementType)Config::measurement, profile);
}

void MainWindow::changeCore(int core) {
  Config::core = core;

  tableView->setModel(NULL);
  profModel->recalc(Config::core, Config::sensor);
  tableView->setModel(profModel);
  QSettings settings;
  tableView->horizontalHeader()->restoreState(settings.value("tableViewState").toByteArray());

  graphScene->clearScene();
  graphScene->drawProfile(Config::core, Config::sensor, (MeasurementType)Config::measurement, profile, graphScene->minTime, graphScene->maxTime);
}

void MainWindow::changeSensor(int sensor) {
  Config::sensor = sensor;

  tableView->setModel(NULL);
  profModel->recalc(Config::core, Config::sensor);
  tableView->setModel(profModel);
  QSettings settings;
  tableView->horizontalHeader()->restoreState(settings.value("tableViewState").toByteArray());

  graphScene->clearScene();
  graphScene->drawProfile(Config::core, Config::sensor, (MeasurementType)Config::measurement, profile, graphScene->minTime, graphScene->maxTime);
}

void MainWindow::changeMeasurement(int window) {
  Config::measurement = window;

  graphScene->clearScene();
  graphScene->drawProfile(Config::core, Config::sensor, (MeasurementType)Config::measurement, profile, graphScene->minTime, graphScene->maxTime);
}
