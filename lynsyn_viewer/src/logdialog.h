#ifndef LOGDIALOG_H
#define LOGDIALOG_H

#include <QDialog>

namespace Ui {
  class LogDialog;
}

class LogDialog : public QDialog {
  Q_OBJECT

public:
  Ui::LogDialog *ui;
  explicit LogDialog(QWidget *parent = nullptr);
  ~LogDialog();
};

#endif // LOGDIALOG_H
