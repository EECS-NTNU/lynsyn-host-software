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

#ifndef PROFILE_H
#define PROFILE_H

#include <QString>
#include <QFile>
#include <QtSql>

#include "profiledialog.h"
#include "profline.h"

class Profile : public QObject {
  Q_OBJECT

private:
  QString dbFilename;
  ProfileDialog *profDialog;

public:
  QString dbConnection;
  unsigned numSensors;
  unsigned numCores;

  Profile(QString dbFilename);
  ~Profile();
  void connect();
  void disconnect();

  bool importCsv(QString csvFilename, QStringList elfFilenames, QString kallsyms);
  bool exportCsv(QString csvFilename);
  void clean();

  bool initProfiler(bool *useJtag);
  bool endProfiler();
  void setParameters(ProfileDialog *profDialog);
  unsigned cores();
  unsigned sensors();
  void buildProfTable(unsigned core, unsigned sensor, std::vector<ProfLine*> &table);  
  void buildProfTable(QVector<Measurement> *measurements, std::vector<ProfLine*> &table);
  LynsynSample getSample();

public slots:
  bool runProfiler();
    
signals:
  void advance(int step, QString msg);
  void finished(int ret, QString msg);
};

#endif
