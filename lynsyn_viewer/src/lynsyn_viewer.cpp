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

#include <QApplication>

#include "lynsyn_viewer.h"
#include "mainwindow.h"

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
  Q_INIT_RESOURCE(application);

  QApplication app(argc, argv);
  app.setOrganizationName(ORG_NAME);
  app.setOrganizationDomain(ORG_DOMAIN);
  app.setApplicationName(APP_NAME);
  app.setApplicationVersion(QString("V") + QString(VERSION));

  MainWindow *mainWin = new MainWindow();
  mainWin->show();

  return app.exec();
}
