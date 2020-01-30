/* This work, "xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd)
 * by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/).
 * "xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/)
 * by Avnet and is used by Xilinx for XAPP1251.
 *
 *  Description : XAPP1251 Xilinx Virtual Cable Server for Linux
 */

#include "xvcserver.h"

#include <QCoreApplication>
#include <QTcpSocket>
#include <QHostInfo>
#include <QTimer>

int XvcServer::sread(QTcpSocket *tcpSocket, void *target, int len) {
  char *t = (char*)target;

  while(len) {
    int r = tcpSocket->read(t, len);
    if(r <= 0) return r;
    t += r;
    len -= r;
  }
  return 1;
}

int XvcServer::handleData() {
  QTcpSocket *tcpSocket = static_cast<QTcpSocket*>(sender());

  while(tcpSocket->bytesAvailable()) {
    char xvcInfo[32];
    unsigned int bufferSize = SHIFT_BUFFER_SIZE * 2;

    sprintf(xvcInfo, "xvcServer_v1.0:%u\n", bufferSize);

    {
      char cmd[16];
      unsigned char buffer[bufferSize], result[bufferSize / 2];
      memset(cmd, 0, 16);

      if (sread(tcpSocket, cmd, 2) != 1) {
        return 1;
      }

      if (memcmp(cmd, "ge", 2) == 0) {
        if (sread(tcpSocket, cmd, 6) != 1) {
          return 1;
        }
        memcpy(result, xvcInfo, strlen(xvcInfo));
        if (tcpSocket->write((const char*)result, strlen(xvcInfo)) != (int)strlen(xvcInfo)) {
          return 1;
        }
#ifdef VERBOSE
        printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
        printf("\t Replied with %s\n", xvcInfo);
        fflush(stdout);
#endif
        return 0;
      } else if (memcmp(cmd, "se", 2) == 0) {
        if (sread(tcpSocket, cmd, 9) != 1) {
          return 1;
        }
        memcpy(result, cmd + 5, 4);

        uint32_t period = (result[3] << 24) | (result[2] << 16) | (result[1] << 8) | result[0];
        uint32_t newPeriod = lynsyn_setTck(period);

        result[3] = (newPeriod >> 24) & 0xff;
        result[2] = (newPeriod >> 16) & 0xff;
        result[1] = (newPeriod >>  8) & 0xff;
        result[0] = newPeriod         & 0xff;

        if (tcpSocket->write((const char*)result, 4) != 4) {
          return 1;
        }

#ifdef VERBOSE
        printf("%u : Received command: 'settck'\n", (int)time(NULL));
        printf("\t Replied with %d\n\n", newPeriod);
        fflush(stdout);
#endif
        return 0;
      } else if (memcmp(cmd, "sh", 2) == 0) {
        if (sread(tcpSocket, cmd, 4) != 1) {
          return 1;
        }
#ifdef VERBOSE
        printf("%u : Received command: 'shift'\n", (int)time(NULL));
        fflush(stdout);
#endif
      } else {
        fprintf(stderr, "invalid cmd '%s'\n", cmd);
        fflush(stderr);
        return 1;
      }

      int len;
      if (sread(tcpSocket, &len, 4) != 1) {
        fprintf(stderr, "reading length failed\n");
        fflush(stderr);
        return 1;
      }

      int nr_bytes = (len + 7) / 8;
      if (nr_bytes * 2 > (int)sizeof(buffer)) {
        fprintf(stderr, "buffer size exceeded\n");
        fflush(stderr);
        return 1;
      }

      if (sread(tcpSocket, buffer, nr_bytes * 2) != 1) {
        fprintf(stderr, "reading data failed\n");
        fflush(stderr);
        return 1;
      }
      memset(result, 0, nr_bytes);

#ifdef VERBOSE
      printf("\tNumber of Bits  : %d\n", len);
      printf("\tNumber of Bytes : %d \n", nr_bytes);
      printf("\n");
      fflush(stdout);
#endif

      lynsyn_shift(len, &buffer[0], &buffer[nr_bytes], result);

      if (tcpSocket->write((const char*)result, nr_bytes) != nr_bytes) {
        return 1;
      }
    }
  }

  return 0;
}

void XvcServer::run() {
  tcpServer = new QTcpServer();

  if (!tcpServer->listen(QHostAddress::Any, XVC_PORT)) {
    fprintf(stderr, "Unable to start the server\n");
    fflush(stderr);
    exit(1);
  }

  printf("INFO: To connect to this lynsyn_xvc instance, use url: TCP:%s:%u\n\n", QHostInfo::localHostName().toUtf8().constData(), XVC_PORT);
  fflush(stdout);

  connect(tcpServer, &QTcpServer::newConnection, this, &XvcServer::newConnection);
}

void XvcServer::newConnection() {
  QTcpSocket *tcpSocket = tcpServer->nextPendingConnection();

  connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(disconnected()));
  connect(tcpSocket, SIGNAL(disconnected()), tcpSocket, SLOT(deleteLater()));

  printf("Connection accepted\n");
  fflush(stdout);

  if(!lynsyn_init()) {
    fprintf(stderr, "Failed to init lynsyn\n");
    fflush(stderr);
    exit(-1);
  }

  tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

  connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(handleData()));
}

void XvcServer::disconnected() {
  printf("Disconnected\n");
  fflush(stdout);
  lynsyn_release();
}

int main(int argc, char **argv) {
  QCoreApplication a(argc, argv);

  XvcServer *xvcServer = new XvcServer(&a);
  QObject::connect(xvcServer, SIGNAL(finished()), &a, SLOT(quit()));
  QTimer::singleShot(0, xvcServer, SLOT(run()));

  return a.exec();
}
