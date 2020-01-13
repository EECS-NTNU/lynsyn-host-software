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

#ifndef ELFSUPPORT_H
#define ELFSUPPORT_H

#include <QString>
#include <QStringList>
#include <QMap>

#include <map>
#include <sstream>

class Addr2Line {
public:
  QString filename;
  QString elfName;
  QString function;
  uint64_t lineNumber;
  bool valid;

  Addr2Line() {
    filename = "Unknown";
    elfName = "Unknown";
    function = "Unknown";
    lineNumber = 0;
    valid = false;
  }

  Addr2Line(QString filename, QString elfName, QString function, uint64_t lineNumber) {
    this->filename = filename;
    this->elfName = elfName;
    this->function = function;
    this->lineNumber = lineNumber;
    this->valid = true;
  }
};

class ElfSupport {

private:
  std::map<uint64_t, Addr2Line> addr2lineCache;

  QStringList elfFiles;
  uint64_t prevPc;
  QStringList kallsyms;

  QMap<QString,QProcess*> elfProcesses;

  Addr2Line addr2line;

  void setPc(uint64_t pc);
  Addr2Line addr2Line(QString filename, uint64_t addr);
  char *readLine(char *s, int size, FILE *stream);

public:
  ElfSupport() {
    prevPc = -1;
  }
  ~ElfSupport();
    
  void addElf(QString elfFile) {
    if(elfFile.trimmed() != "") {
      elfFiles.push_back(elfFile);
    }
  }
  void addKallsyms(QString symsFile);

  // get debug info
  QString getFilename(uint64_t pc);
  QString getElfName(uint64_t pc);
  QString getFunction(uint64_t pc);
  uint64_t getLineNumber(uint64_t pc);

  // get symbol value
  uint64_t lookupSymbol(QString symbol);
};

#endif
