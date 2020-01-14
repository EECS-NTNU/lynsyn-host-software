#ifndef XVC_SERVER
#define XVC_SERVER

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <QTcpServer>

#include "lynsyn.h"

//#define VERBOSE

#define XVC_PORT 2542

class XvcServer : public QObject {
  Q_OBJECT

private:
  QTcpServer *tcpServer;

  int sread(QTcpSocket *tcpSocket, void *target, int len);

public:
  XvcServer(QObject *parent) : QObject(parent) {}

public slots:
  void run();
  void newConnection();
  void disconnected();
  int handleData();

signals:
  void finished();
};

#endif
