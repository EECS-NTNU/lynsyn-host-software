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

#include <assert.h>

#include "profile.h"
#include "lynsyn.h"
#include "elfsupport.h"

Profile::Profile(QString dbFilename) {
  this->dbFilename = dbFilename;
}

Profile::~Profile() {
  disconnect();
}

void Profile::connect() {
  dbConnection = QString("profile");

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", dbConnection);
  db.setDatabaseName(dbFilename);

  bool success = db.open();

  if(!success) {
    QSqlError error = db.lastError();
    printf("Can't open DB: %s\n", error.text().toUtf8().constData());
    assert(0);
  }

  QSqlQuery query(db);

  QString queryString = "CREATE TABLE IF NOT EXISTS measurements (time INT, timeSinceLast INT";
  for(unsigned core = 0; core < LYNSYN_MAX_CORES; core++) {
    queryString += ", pc" + QString::number(core + 1) + " INT";
    queryString += ", function" + QString::number(core + 1) + " TEXT";
    queryString += ", module" + QString::number(core + 1) + " TEXT";
  }
  for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
    queryString += ", current" + QString::number(sensor + 1) + " REAL";
    queryString += ", voltage" + QString::number(sensor + 1) + " REAL";
  }
  queryString += ")";

  success = query.exec(queryString);
  assert(success);

  success = query.exec("CREATE TABLE IF NOT EXISTS marks (time INT, delay INT)");
  assert(success);

  queryString = "CREATE TABLE IF NOT EXISTS meta (sensors INT, cores INT, samples INT, mintime INT, maxtime INT";
  for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
    queryString += ", mincurrent" + QString::number(sensor + 1) + " REAL";
    queryString += ", maxcurrent" + QString::number(sensor + 1) + " REAL";
    queryString += ", minvoltage" + QString::number(sensor + 1) + " REAL";
    queryString += ", maxvoltage" + QString::number(sensor + 1) + " REAL";
    queryString += ", minpower" + QString::number(sensor + 1) + " REAL";
    queryString += ", maxpower" + QString::number(sensor + 1) + " REAL";
  }
  queryString += ")";

  success = query.exec(queryString);
  assert(success);

  success = query.exec(QString() + "SELECT sensors,cores FROM meta");
  if(query.next()) {
    numSensors = query.value("sensors").toUInt();
    numCores = query.value("cores").toUInt();
  } else {
    numSensors = 0;
    numCores = 0;
  }
}

void Profile::disconnect() {
  {
    QSqlDatabase db = QSqlDatabase::database(dbConnection);
    db.close();
  }
  QSqlDatabase::removeDatabase(dbConnection);
}

bool Profile::exportCsv(QString csvFilename) {
  QFile csvFile(csvFilename);
  bool success = csvFile.open(QIODevice::WriteOnly);
  if(!success) return false;

  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);
  
  success = query.exec(QString() + "SELECT sensors,cores,mintime FROM meta");
  if(!query.next()) {
    csvFile.close();
    printf("SQL Error: %s\n", query.lastError().text().toUtf8().constData());
    return false;
  }
  numSensors = query.value("sensors").toUInt();
  numCores = query.value("cores").toUInt();
  int64_t minTime = query.value("mintime").toDouble();

  {
    QString header = "Sensors;Cores\n" + QString::number(numSensors) + ";" + QString::number(numCores) + "\n";
    csvFile.write(header.toUtf8());
  }
  
  {
    QString header = "Time";
    for(int i = 0; i < LYNSYN_MAX_CORES; i++) {
      header += ";pc " + QString::number(i);
    }
    for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
      header += ";current " + QString::number(i);
    }
    for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
      header += ";voltage " + QString::number(i);
    }
    header += "\n";

    csvFile.write(header.toUtf8());
  }
  

  QString queryString = "SELECT time";
  for(int i = 0; i < LYNSYN_MAX_CORES; i++) {
    queryString += ",pc" + QString::number(i + 1);
  }
  for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
    queryString += ",current" + QString::number(i + 1);
    queryString += ",voltage" + QString::number(i + 1);
  }
  queryString += " FROM measurements";

  success = query.exec(queryString);
  if(!success) {
    csvFile.close();
    printf("SQL Error: %s\n", query.lastError().text().toUtf8().constData());
    return false;
  }

  while(query.next()) {
    QString measurement;

    double time = lynsyn_cyclesToSeconds(query.value("time").toLongLong() - minTime);
    measurement = QString::number(time);

    for(unsigned core = 0; core < LYNSYN_MAX_CORES; core++) {
      uint64_t pc = query.value("pc" + QString::number(core + 1)).toULongLong() << 2;
      measurement += ";" + QString::number(pc);
    }

    for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
      double current = query.value("current" + QString::number(sensor+1)).toDouble();
      measurement += ";" + QString::number(current);
    }

    for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
      double voltage = query.value("voltage" + QString::number(sensor+1)).toDouble();
      measurement += ";" + QString::number(voltage);
    }

    measurement += "\n";

    csvFile.write(measurement.toUtf8());
  }

  csvFile.close();

  return true;
}

bool Profile::importCsv(QString csvFilename, QStringList elfFilenames, QString kallsyms) {
  QFile file(csvFilename);
  if(!file.open(QIODevice::ReadOnly)) return false;

  ElfSupport elfSupport;
  for(auto elf : elfFilenames) {
    elfSupport.addElf(elf);
  }
  elfSupport.addKallsyms(kallsyms);

  clean();

  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  db.transaction();

  QSqlQuery query(db);

  QString queryString = "INSERT INTO measurements (time, timeSinceLast";
  for(unsigned core = 0; core < LYNSYN_MAX_CORES; core++) {
    queryString += ", pc" + QString::number(core + 1);
    queryString += ", function" + QString::number(core + 1);
    queryString += ", module" + QString::number(core + 1);
  }
  for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
    queryString += ", current" + QString::number(sensor + 1);
    queryString += ", voltage" + QString::number(sensor + 1);
  }
  queryString += ") VALUES (:time, :timeSinceLast";
  for(unsigned core = 0; core < LYNSYN_MAX_CORES; core++) {
    queryString += ", :pc" + QString::number(core + 1);
    queryString += ", :function" + QString::number(core + 1);
    queryString += ", :module" + QString::number(core + 1);
  }
  for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
    queryString += ", :current" + QString::number(sensor + 1);
    queryString += ", :voltage" + QString::number(sensor + 1);
  }
  queryString += ")";

  query.prepare(queryString);

  { // header
    file.readLine(); // get rid of comment
    QString line = file.readLine();
    QStringList tokens = line.split(';');
    numSensors = tokens[0].toUInt();
    numCores = tokens[1].toUInt();
    file.readLine(); // get rid of comment
  }

  // body
  int samples = 0;
  int64_t minTime = -1;
  int64_t maxTime = 0;
  double mincurrent[LYNSYN_MAX_SENSORS];
  double maxcurrent[LYNSYN_MAX_SENSORS];
  double minvoltage[LYNSYN_MAX_SENSORS];
  double maxvoltage[LYNSYN_MAX_SENSORS];
  double minpower[LYNSYN_MAX_SENSORS];
  double maxpower[LYNSYN_MAX_SENSORS];

  for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
    mincurrent[i] = 1000000;
    maxcurrent[i] = 0;
    minvoltage[i] = 1000000;
    maxvoltage[i] = 0;
    minpower[i] = 1000000;
    maxpower[i] = 0;
  }

  int64_t lastTime = -1;

  while(!file.atEnd()) {
    QString line = file.readLine();
    QStringList tokens = line.split(';');

    int64_t time = lynsyn_secondsToCycles(tokens[0].toDouble());
    int64_t timeSinceLast = (lastTime == -1) ? 0 : time - lastTime;
    uint64_t pc[LYNSYN_MAX_CORES];
    for(int i = 0; i < LYNSYN_MAX_CORES; i++) {
      pc[i] = tokens[1+i].toULongLong();
    }
    double current[LYNSYN_MAX_SENSORS];
    for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
      current[i] = tokens[1 + LYNSYN_MAX_CORES + i].toDouble();
    }
    double voltage[LYNSYN_MAX_SENSORS];
    for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
      voltage[i] = tokens[1 + LYNSYN_MAX_CORES + LYNSYN_MAX_SENSORS + i].toDouble();
    }

    lastTime = time;
    samples++;
    if(minTime == -1) minTime = time;
    maxTime = time;
    for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
      if(mincurrent[i] > current[i]) mincurrent[i] = current[i];
      if(maxcurrent[i] < current[i]) maxcurrent[i] = current[i];
      if(minvoltage[i] > voltage[i]) minvoltage[i] = voltage[i];
      if(maxvoltage[i] < voltage[i]) maxvoltage[i] = voltage[i];
      double power = voltage[i] * current[i];
      if(minpower[i] > power) minpower[i] = power;
      if(maxpower[i] < power) maxpower[i] = power;
    }

    query.bindValue(":time", (qint64)time);

    query.bindValue(":timeSinceLast", (qint64)timeSinceLast);

    for(unsigned core = 0; core < LYNSYN_MAX_CORES; core++) {
      query.bindValue(":pc" + QString::number(core + 1), (qint64)pc[core] >> 2);
      query.bindValue(":function" + QString::number(core + 1), elfSupport.getFunction(pc[core]));
      query.bindValue(":module" + QString::number(core + 1), elfSupport.getFilename(pc[core]));
    }

    for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
      query.bindValue(":current" + QString::number(sensor + 1), current[sensor]);
      query.bindValue(":voltage" + QString::number(sensor + 1), voltage[sensor]);
    }

    bool success = query.exec();
    if(!success) {
      printf("SQL Error: %s\n", query.lastError().text().toUtf8().constData());
      return false;
    }
  }

  {
    QSqlQuery query(db);

    QString queryString = "INSERT INTO meta (sensors, cores, samples, mintime, maxtime";
    for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
      queryString += ", mincurrent" + QString::number(sensor + 1);
      queryString += ", maxcurrent" + QString::number(sensor + 1);
      queryString += ", minvoltage" + QString::number(sensor + 1);
      queryString += ", maxvoltage" + QString::number(sensor + 1);
      queryString += ", minpower" + QString::number(sensor + 1);
      queryString += ", maxpower" + QString::number(sensor + 1);
    }
    queryString += ") VALUES (:sensors, :cores, :samples, :mintime, :maxtime";
    for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
      queryString += ", :mincurrent" + QString::number(sensor + 1);
      queryString += ", :maxcurrent" + QString::number(sensor + 1);
      queryString += ", :minvoltage" + QString::number(sensor + 1);
      queryString += ", :maxvoltage" + QString::number(sensor + 1);
      queryString += ", :minpower" + QString::number(sensor + 1);
      queryString += ", :maxpower" + QString::number(sensor + 1);
    }
    queryString += ")";

    query.prepare(queryString);

    query.bindValue(":sensors", numSensors);
    query.bindValue(":cores", numCores);
    query.bindValue(":samples", samples);
    query.bindValue(":mintime", (qint64)minTime);
    query.bindValue(":maxtime", (qint64)maxTime);
    for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
      query.bindValue(":mincurrent" + QString::number(sensor + 1), mincurrent[sensor]);
      query.bindValue(":maxcurrent" + QString::number(sensor + 1), maxcurrent[sensor]);
      query.bindValue(":minvoltage" + QString::number(sensor + 1), minvoltage[sensor]);
      query.bindValue(":maxvoltage" + QString::number(sensor + 1), maxvoltage[sensor]);
      query.bindValue(":minpower" + QString::number(sensor + 1), minpower[sensor]);
      query.bindValue(":maxpower" + QString::number(sensor + 1), maxpower[sensor]);
    }

    bool success = query.exec();
    if(!success) {
      printf("SQL Error: %s\n", query.lastError().text().toUtf8().constData());
      return false;
    }
  }

  db.commit();

  return true;
}

QString getLastExecutedQuery(const QSqlQuery& query) {
 QString str = query.lastQuery();
 QMapIterator<QString, QVariant> it(query.boundValues());
 while (it.hasNext())
 {
  it.next();
  str.replace(it.key(),it.value().toString());
 }
 return str;
}

void Profile::clean() {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);

  QSqlQuery query = QSqlQuery(db);
  query.exec("DELETE FROM measurements");
  query.exec("DELETE FROM marks");
  query.exec("DELETE FROM meta");
}

bool Profile::runProfiler() {
  clean();

  emit advance(0, "Waiting");

  double period = profDialog->ui->periodSpinBox->value();

  bool useBp = false;
  uint64_t coreMask = 0;
  uint64_t startAddr = 0;
  uint64_t endAddr = 0;
  uint64_t markAddr = 0;

  numSensors = lynsyn_numSensors();
  numCores = 0;

  if(profDialog->useJtag) {
    useBp = profDialog->ui->bpCheckBox->checkState() == Qt::Checked;

    for(unsigned i = 0; i < cores(); i++) {
      if(profDialog->coreCheckboxes[i]->checkState() == Qt::Checked) {
        coreMask |= 1 << i;
        numCores++;
      }
    }

    ElfSupport elfSupport;
    for(auto elf : profDialog->ui->elfEdit->text().split(',')) {
      elfSupport.addElf(elf);
    }
    elfSupport.addKallsyms(profDialog->ui->kallsymsEdit->text());

    QString startBp = profDialog->ui->startEdit->text();
    QString stopBp = profDialog->ui->stopEdit->text();
    QString markBp = profDialog->ui->markEdit->text();
    startAddr = elfSupport.lookupSymbol(startBp);
    endAddr = elfSupport.lookupSymbol(stopBp);
    markAddr = elfSupport.lookupSymbol(markBp);
  }

  if(useBp && startAddr) {
    if(markAddr) lynsyn_setMarkBreakpoint(markAddr);

    if(!coreMask || !endAddr) {
      lynsyn_startBpPeriodSampling(startAddr, period, coreMask);
    } else {
      lynsyn_startBpSampling(startAddr, endAddr, coreMask);
    }
  } else {
    lynsyn_startPeriodSampling(period, coreMask);
  }

  //-----------------------------------------------------------------------------

  int samples = 0;
  int64_t minTime = -1;
  int64_t maxTime = 0;
  double mincurrent[LYNSYN_MAX_SENSORS];
  double maxcurrent[LYNSYN_MAX_SENSORS];
  double minvoltage[LYNSYN_MAX_SENSORS];
  double maxvoltage[LYNSYN_MAX_SENSORS];
  double minpower[LYNSYN_MAX_SENSORS];
  double maxpower[LYNSYN_MAX_SENSORS];

  int64_t lastTime = -1;

  for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
    mincurrent[i] = 1000000;
    maxcurrent[i] = 0;
    minvoltage[i] = 1000000;
    maxvoltage[i] = 0;
    minpower[i] = 1000000;
    maxpower[i] = 0;
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "thread");
  db.setDatabaseName(dbFilename);
  bool success = db.open();
  Q_UNUSED(success);
  assert(success);

  db.transaction();

  QSqlQuery query(db);

  QString queryString = "INSERT INTO measurements (time, timeSinceLast";
  for(unsigned core = 0; core < LYNSYN_MAX_CORES; core++) {
    queryString += ", pc" + QString::number(core + 1);
  }
  for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
    queryString += ", current" + QString::number(sensor + 1);
    queryString += ", voltage" + QString::number(sensor + 1);
  }
  queryString += ") VALUES (:time, :timeSinceLast";
  for(unsigned core = 0; core < LYNSYN_MAX_CORES; core++) {
    queryString += ", :pc" + QString::number(core + 1);
  }
  for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
    queryString += ", :current" + QString::number(sensor + 1);
    queryString += ", :voltage" + QString::number(sensor + 1);
  }
  queryString += ")";

  query.prepare(queryString);

  bool started = false;

  struct LynsynSample sample;
  while(lynsyn_getNextSample(&sample)) {

    if(!started) {
      emit advance(1, "Collecting samples");
      started = true;
    }

    if(!profDialog->hasVoltageSensors) {
      for(unsigned i = 0; i < profDialog->numSensors; i++) {
        sample.voltage[i] = profDialog->sensorSpinboxes[i]->value();
      }
    }

    if(sample.flags & SAMPLE_REPLY_FLAG_MARK) {
      QSqlQuery markQuery(db);

      markQuery.prepare("INSERT INTO marks (time, delay) VALUES (:time, :delay)");

      markQuery.bindValue(":time", (qint64)sample.time);
      markQuery.bindValue(":delay", (qint64)sample.pc[0] - (qint64)sample.time);

      bool success = markQuery.exec();
      Q_UNUSED(success);
      assert(success);

      lastTime = (qint64)sample.pc[0];

    } else {
      int64_t timeSinceLast = 0;
      if(lastTime != -1) {
        timeSinceLast = sample.time - lastTime;
      }
      lastTime = sample.time;
      samples++;
      if(minTime == -1) minTime = sample.time;
      maxTime = sample.time;
      for(int i = 0; i < LYNSYN_MAX_SENSORS; i++) {
        if(mincurrent[i] > sample.current[i]) mincurrent[i] = sample.current[i];
        if(maxcurrent[i] < sample.current[i]) maxcurrent[i] = sample.current[i];
        if(minvoltage[i] > sample.voltage[i]) minvoltage[i] = sample.voltage[i];
        if(maxvoltage[i] < sample.voltage[i]) maxvoltage[i] = sample.voltage[i];
        double power = sample.voltage[i] * sample.current[i];
        if(minpower[i] > power) minpower[i] = power;
        if(maxpower[i] < power) maxpower[i] = power;
      }

      query.bindValue(":time", (qint64)sample.time);

      query.bindValue(":timeSinceLast", (qint64)timeSinceLast);

      for(unsigned core = 0; core < LYNSYN_MAX_CORES; core++) {
        query.bindValue(":pc" + QString::number(core + 1), (qint64)sample.pc[core] >> 2);
      }

      for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
        query.bindValue(":current" + QString::number(sensor + 1), sample.current[sensor]);
        query.bindValue(":voltage" + QString::number(sensor + 1), sample.voltage[sensor]);
      }

      bool success = query.exec();
      if(!success) {
        printf("SQL Error: %s\n", query.lastError().text().toUtf8().constData());
        exit(1);
      }
    }
  }

  {
    QSqlQuery query(db);

    QString queryString = "INSERT INTO meta (sensors, cores, samples, mintime, maxtime";
    for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
      queryString += ", mincurrent" + QString::number(sensor + 1);
      queryString += ", maxcurrent" + QString::number(sensor + 1);
      queryString += ", minvoltage" + QString::number(sensor + 1);
      queryString += ", maxvoltage" + QString::number(sensor + 1);
      queryString += ", minpower" + QString::number(sensor + 1);
      queryString += ", maxpower" + QString::number(sensor + 1);
    }
    queryString += ") VALUES (:sensors, :cores, :samples, :mintime, :maxtime";
    for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
      queryString += ", :mincurrent" + QString::number(sensor + 1);
      queryString += ", :maxcurrent" + QString::number(sensor + 1);
      queryString += ", :minvoltage" + QString::number(sensor + 1);
      queryString += ", :maxvoltage" + QString::number(sensor + 1);
      queryString += ", :minpower" + QString::number(sensor + 1);
      queryString += ", :maxpower" + QString::number(sensor + 1);
    }
    queryString += ")";

    query.prepare(queryString);

    query.bindValue(":sensors", numSensors);
    query.bindValue(":cores", numCores);
    query.bindValue(":samples", samples);
    query.bindValue(":mintime", (qint64)minTime);
    query.bindValue(":maxtime", (qint64)maxTime);
    for(unsigned sensor = 0; sensor < LYNSYN_MAX_SENSORS; sensor++) {
      query.bindValue(":mincurrent" + QString::number(sensor + 1), mincurrent[sensor]);
      query.bindValue(":maxcurrent" + QString::number(sensor + 1), maxcurrent[sensor]);
      query.bindValue(":minvoltage" + QString::number(sensor + 1), minvoltage[sensor]);
      query.bindValue(":maxvoltage" + QString::number(sensor + 1), maxvoltage[sensor]);
      query.bindValue(":minpower" + QString::number(sensor + 1), minpower[sensor]);
      query.bindValue(":maxpower" + QString::number(sensor + 1), maxpower[sensor]);
    }

    bool success = query.exec();
    if(!success) {
      printf("SQL Error: %s\n", query.lastError().text().toUtf8().constData());
      exit(1);
    }
  }

  db.commit();

  {
    emit advance(2, "Processing samples");

    ElfSupport elfSupport;
    for(auto elf : profDialog->ui->elfEdit->text().split(",")) {
      elfSupport.addElf(elf);
    }
    elfSupport.addKallsyms(profDialog->ui->kallsymsEdit->text());

    QSqlQuery query(db);
    query.setForwardOnly(true);
    
    QString queryString = "SELECT rowid";
    for(int core = 0; core < LYNSYN_MAX_CORES; core++) {
      queryString += ", pc" + QString::number(core + 1);
    }
    queryString += " FROM measurements";

    bool success = query.exec(queryString);
    if(!success) {
      printf("SQL Error: %s\n", query.lastError().text().toUtf8().constData());
      exit(1);
    }

    db.transaction();

    queryString = "UPDATE measurements SET function1=:function1, module1=:module1";
    for(int core = 1; core < LYNSYN_MAX_CORES; core++) {
      queryString += ", function" + QString::number(core + 1) + "=:function" + QString::number(core + 1); 
      queryString += ", module" + QString::number(core + 1) + "=:module" + QString::number(core + 1); 
    }
    queryString += " WHERE rowid=:rowid";

    QSqlQuery updateQuery(db);
    updateQuery.prepare(queryString);
    
    while(query.next()) {
      unsigned rowId = query.value("rowid").toUInt();
      uint64_t pc[LYNSYN_MAX_CORES];
      for(int core = 0; core < LYNSYN_MAX_CORES; core++) {
        pc[core] = query.value("pc" + QString::number(core + 1)).toULongLong() << 2;
      }

      updateQuery.bindValue(":rowid", rowId);
      for(int core = 0; core < LYNSYN_MAX_CORES; core++) {
        updateQuery.bindValue(":function" + QString::number(core + 1), elfSupport.getFunction(pc[core]));
        QFileInfo info(elfSupport.getFilename(pc[core]));
        updateQuery.bindValue(":module" + QString::number(core + 1), info.fileName());
      }

      bool success = updateQuery.exec();
      if(!success) {
        printf("SQL Error: %s\n", query.lastError().text().toUtf8().constData());
        exit(1);
      }
    }

    db.commit();
  }

  emit finished(0, "");
  return true;
}

void Profile::setParameters(ProfileDialog *profDialog) {
  this->profDialog = profDialog;
}

bool Profile::initProfiler(bool *useJtag) {
  if(lynsyn_init()) {
    *useJtag = lynsyn_jtagInit(lynsyn_getDefaultJtagDevices());
    return true;
  }

  return false;
}

bool Profile::endProfiler() {
  lynsyn_release();
  return true;
}

unsigned Profile::cores() {
  return lynsyn_numCores();
}

unsigned Profile::sensors() {
  return lynsyn_numSensors();
}

void Profile::buildProfTable(QVector<Measurement> *measurements, std::vector<ProfLine*> &table) {
  QMap<QString,ProfLine*> lineMap;
  for(auto m : *measurements) {
    auto it = lineMap.find(m.id);
    ProfLine *profLine = NULL;
    if(it != lineMap.end()) {
      profLine = *it;
    } else {
      profLine = new ProfLine(m.id);
      lineMap[m.id] = profLine;
    }
    profLine->addMeasurement(m);
  }
  for(auto line : lineMap) {
    table.push_back(line);
  }
}

void Profile::buildProfTable(unsigned core, unsigned sensor, std::vector<ProfLine*> &table) {
  QSqlDatabase db = QSqlDatabase::database(dbConnection);
  QSqlQuery query(db);

  QVector<Measurement> *measurements = new QVector<Measurement>;

  QString queryString = QString() +
    "SELECT time,timeSinceLast,function" + QString::number(core+1) +
    ",module" + QString::number(core+1) +
    ",current" + QString::number(sensor+1) +
    ",voltage" + QString::number(sensor+1) +
    " FROM measurements";

  query.exec(queryString);

  while(query.next()) {
    int64_t time = query.value("time").toLongLong();
    int64_t timeSinceLast = query.value("timeSinceLast").toLongLong();

    double current = query.value("current" + QString::number(sensor+1)).toDouble();
    double voltage = query.value("voltage" + QString::number(sensor+1)).toDouble();

    double power = current * voltage;

    QString module = query.value("module" + QString::number(core+1)).toString();
    QString function = query.value("function" + QString::number(core+1)).toString();

    measurements->push_back(Measurement(time, timeSinceLast, power, module, function));
  }

  buildProfTable(measurements, table);
}

LynsynSample Profile::getSample() {
  LynsynSample sample;

  unsigned coreMask = 0;

  switch(cores()) {
    case 0: coreMask = 0x0; break;
    case 1: coreMask = 0x1; break;
    case 2: coreMask = 0x3; break;
    case 3: coreMask = 0x7; break;
    default:
    case 4: coreMask = 0xf; break;
  }

  lynsyn_getSample(&sample, true, coreMask);
  return sample;
}
