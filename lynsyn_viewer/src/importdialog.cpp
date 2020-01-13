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

#include <QFileDialog>
#include <QSettings>

#include "importdialog.h"
#include "ui_importdialog.h"

ImportDialog::ImportDialog(QWidget *parent) : QDialog(parent), ui(new Ui::ImportDialog) {
  QSettings settings;

  ui->setupUi(this);

  ui->csvEdit->setText(settings.value("csv", "output.csv").toString());
  ui->elfEdit->setText(settings.value("elf", "").toString());
  ui->kallsymsEdit->setText(settings.value("kallsyms", "").toString());
}

ImportDialog::~ImportDialog() {
  QSettings settings;

  settings.setValue("csv", ui->csvEdit->text());
  settings.setValue("elf", ui->elfEdit->text());
  settings.setValue("kallsyms", ui->kallsymsEdit->text());

  delete ui;
}

void ImportDialog::updateCsv() {
  QFileDialog dialog(this, "Select CSV file to import");
  dialog.setNameFilter(tr("CSV (*.csv)"));
  dialog.setAcceptMode(QFileDialog::AcceptOpen);
  if(dialog.exec()) {
    ui->csvEdit->setText(dialog.selectedFiles()[0]);
  }
}
