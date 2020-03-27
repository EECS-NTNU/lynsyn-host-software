#include "livedialog.h"
#include "ui_livedialog.h"

#define TIMER_INTERVAL 500

LiveDialog::LiveDialog(Profile *profile, bool useJtag, QWidget *parent) : QDialog(parent), ui(new Ui::LiveDialog) {
  ui->setupUi(this);

  this->profile = profile;
  this->useJtag = useJtag;

  QLabel *label = new QLabel("Power");
  label->setStyleSheet("font-weight: bold");
  ui->sensorLayout->addWidget(label, 0, 1);

  label = new QLabel("Current");
  label->setStyleSheet("font-weight: bold");
  ui->sensorLayout->addWidget(label, 0, 2);

  label = new QLabel("Voltage");
  label->setStyleSheet("font-weight: bold");
  ui->sensorLayout->addWidget(label, 0, 3);

  for(unsigned i = 0; i < profile->sensors(); i++) {
    QLabel *label = new QLabel("Sensor " + QString::number(i));
    label->setStyleSheet("font-weight: bold");
    ui->sensorLayout->addWidget(label, i + 1, 0);
 
    powerLabels.push_back(new QLabel("-"));
    ui->sensorLayout->addWidget(powerLabels.back(), i + 1, 1);

    currentLabels.push_back(new QLabel("-"));
    ui->sensorLayout->addWidget(currentLabels.back(), i + 1, 2);

    voltageLabels.push_back(new QLabel("-"));
    ui->sensorLayout->addWidget(voltageLabels.back(), i + 1, 3);
  }

  if(useJtag) {
    label = new QLabel("PC");
    label->setStyleSheet("font-weight: bold");
    ui->coreLayout->addWidget(label, 0, 1);

    for(unsigned i = 0; i < profile->cores(); i++) {
      QLabel *label = new QLabel("Core " + QString::number(i));
      label->setStyleSheet("font-weight: bold");
      ui->coreLayout->addWidget(label, i + 1, 0);
 
      coreLabels.push_back(new QLabel("-"));
      ui->coreLayout->addWidget(coreLabels.back(), i + 1, 1);
    }
  } else {
    ui->coreGroup->setVisible(false);
  }

  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(updateValues()));
  timer->start(TIMER_INTERVAL);
}

LiveDialog::~LiveDialog() {
  delete timer;
  delete ui;
}

void LiveDialog::updateValues() {
  LynsynSample sample = profile->getSample();
  for(unsigned i = 0; i < profile->sensors(); i++) {
    powerLabels[i]->setText(QString::number(sample.current[i] * sample.voltage[i]) + "W");
    currentLabels[i]->setText(QString::number(sample.current[i]) + "A");
    voltageLabels[i]->setText(QString::number(sample.voltage[i]) + "V");
  }
  if(useJtag) {
    for(unsigned i = 0; i < profile->cores(); i++) {
      coreLabels[i]->setText(QString::number(sample.pc[i], 16));
    }
  }
}
