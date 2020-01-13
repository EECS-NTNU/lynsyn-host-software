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

#include "profmodel.h"

ProfModel::ProfModel(Profile *profile, QObject *parent) : QAbstractTableModel(parent) {
  this->profile = profile;
}

void ProfModel::recalc(unsigned core, unsigned sensor) {
  for(auto i : table) {
    delete i;
  }
  table.clear();
  profile->buildProfTable(core, sensor, table);
}

int ProfModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent);
  return table.size();
}

int ProfModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent);
  return 4;
}

QVariant ProfModel::data(const QModelIndex &index, int role) const {
  if (role == Qt::DisplayRole) {
    ProfLine *line = table[index.row()];
    if(index.column() == 0) {
      return line->id;
    }
    switch(index.column()-1) {
      case 0: return line->getRuntime();
      case 1: return line->getPower();
      case 2: return line->getEnergy();
    }
  }
  return QVariant();
}

QVariant ProfModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch(section) {
      case 0: return QVariant("ID"); break;
      case 1: return QVariant("Runtime [s]"); break;
      case 2: return QVariant("Power [W]"); break;
      case 3: return QVariant("Energy [J]"); break;
    }
  }
  return QVariant();
}

void ProfModel::sort(int column, Qt::SortOrder order) {
  ProfSort profSort(column, order);
  std::sort(table.begin(), table.end(), profSort);
  emit dataChanged(createIndex(0, 0), createIndex(table.size()-1, 6));
}
