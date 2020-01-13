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

#include <QSettings>

#include <lynsyn.h>

#include "profiledialog.h"
#include "ui_profiledialog.h"

ProfileDialog::ProfileDialog(unsigned numCores, bool hasVoltageSensors, unsigned numSensors, bool useJtag, QWidget *parent) : QDialog(parent), ui(new Ui::ProfileDialog) {
  this->numCores = numCores;
  this->hasVoltageSensors = hasVoltageSensors;
  this->numSensors = numSensors;
  this->useJtag = useJtag;

  ui->setupUi(this);

  QSettings settings;

  ui->periodSpinBox->setValue(settings.value("period", 10).toDouble());

  if(useJtag) {
    ui->bpCheckBox->setCheckState(settings.value("useBp", false).toBool() ? Qt::Checked : Qt::Unchecked);
    ui->startEdit->setText(settings.value("startbp", "").toString());
    ui->stopEdit->setText(settings.value("stopbp", "").toString());
    ui->markEdit->setText(settings.value("markbp", "").toString());
    ui->elfEdit->setText(settings.value("elf", "").toString());
    ui->kallsymsEdit->setText(settings.value("kallsyms", "").toString());

    ui->coreLabel->setText("Cores (select maximum " + QString::number(LYNSYN_MAX_CORES) + ")");

    for(unsigned i = 0; i < numCores; i++) {
      QCheckBox *coreCheckbox = new QCheckBox(QString::number(i));
      coreCheckbox->setCheckState(settings.value("core" + QString::number(i), true).toBool() ? Qt::Checked : Qt::Unchecked);
      ui->coreLayout->addWidget(coreCheckbox);
      coreCheckboxes.push_back(coreCheckbox);
    }
    ui->coreLayout->addStretch(1);

  } else {
    ui->jtagGroupBox->setVisible(false);
  }

  if(!hasVoltageSensors) {
    QLabel *voltageLabel = new QLabel("Target voltages:");
    ui->sensorGroupBoxLayout->addWidget(voltageLabel);

    QHBoxLayout *layout = new QHBoxLayout;
    for(unsigned i = 0; i < numSensors; i++) {
      QLabel *label = new QLabel("S" + QString::number(i + 1));
      layout->addWidget(label);
      QDoubleSpinBox *spinBox = new QDoubleSpinBox;
      spinBox->setMinimum(0);
      spinBox->setMaximum(25);
      spinBox->setValue(settings.value("voltage" + QString::number(i), 12).toDouble());
      sensorSpinboxes.push_back(spinBox);
      layout->addWidget(spinBox);
    }
    ui->sensorGroupBoxLayout->addLayout(layout);

  } else {
    ui->sensorGroupBox->setVisible(false);
  }
}

ProfileDialog::~ProfileDialog() {
  QSettings settings;

  settings.setValue("period", ui->periodSpinBox->value());

  if(useJtag) {
    settings.setValue("useBp", ui->bpCheckBox->checkState() == Qt::Checked);
    settings.setValue("startbp", ui->startEdit->text());
    settings.setValue("stopbp", ui->stopEdit->text());
    settings.setValue("markbp", ui->markEdit->text());
    settings.setValue("elf", ui->elfEdit->text());
    settings.setValue("kallsyms", ui->kallsymsEdit->text());

    for(unsigned i = 0; i < numCores; i++) {
      settings.setValue("core" + QString::number(i), coreCheckboxes[i]->checkState() == Qt::Checked);
    }
  }

  if(!hasVoltageSensors) {
    for(unsigned i = 0; i < numSensors; i++) {
      settings.setValue("voltage" + QString::number(i), sensorSpinboxes[i]->value());
    }
  }

  delete ui;
}
