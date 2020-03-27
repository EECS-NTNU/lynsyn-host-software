#ifndef LIVEDIALOG_H
#define LIVEDIALOG_H

#include <QDialog>
#include <QGridLayout>

#include "profile.h"

namespace Ui {
  class LiveDialog;
}

class LiveDialog : public QDialog {
  Q_OBJECT

 public:
  LiveDialog(Profile *profile, bool useJtag, QWidget *parent = 0);
  ~LiveDialog();

 private:
  Profile *profile;
  bool useJtag;
  QVector<QLabel*> powerLabels;
  QVector<QLabel*> currentLabels;
  QVector<QLabel*> voltageLabels;
  QVector<QLabel*> coreLabels;
  Ui::LiveDialog *ui;
  QTimer *timer;

 public slots:
  void updateValues();
};

#endif // LIVEDIALOG_H
