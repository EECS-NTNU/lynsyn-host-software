/* This work, "xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd)
 * by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/).
 * "xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/)
 * by Avnet and is used by Xilinx for XAPP1251.
 *
 *  Description : XAPP1251 Xilinx Virtual Cable Server for Linux
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <pthread.h>

#include "lynsyn.h"

typedef struct {
  uint32_t length_offset;
  uint32_t tms_offset;
  uint32_t tdi_offset;
  uint32_t tdo_offset;
  uint32_t ctrl_offset;
} jtag_t;

static int verbose = 0;

#define XVC_PORT 2542

static int sread(int fd, void *target, int len) {
  unsigned char *t = target;
  while (len) {
    int r = read(fd, t, len);
    if (r <= 0)
      return r;
    t += r;
    len -= r;
  }
  return 1;
}

int handle_data(int fd) {
  char xvcInfo[32];
  unsigned int bufferSize = SHIFT_BUFFER_SIZE * 2;

  sprintf(xvcInfo, "xvcServer_v1.0:%u\n", bufferSize);

  do {
    char cmd[16];
    unsigned char buffer[bufferSize], result[bufferSize / 2];
    memset(cmd, 0, 16);

    if (sread(fd, cmd, 2) != 1)
      return 1;

    if (memcmp(cmd, "ge", 2) == 0) {
      if (sread(fd, cmd, 6) != 1)
        return 1;
      memcpy(result, xvcInfo, strlen(xvcInfo));
      if (write(fd, result, strlen(xvcInfo)) != strlen(xvcInfo)) {
        perror("write");
        return 1;
      }
      if (verbose) {
        printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
        printf("\t Replied with %s\n", xvcInfo);
      }
      break;
    } else if (memcmp(cmd, "se", 2) == 0) {
      if (sread(fd, cmd, 9) != 1)
        return 1;
      memcpy(result, cmd + 5, 4);

      uint32_t period = (result[3] << 24) | (result[2] << 16) | (result[1] << 8) | result[0];
      uint32_t newPeriod = lynsyn_setTck(period);

      result[3] = (newPeriod >> 24) & 0xff;
      result[2] = (newPeriod >> 16) & 0xff;
      result[1] = (newPeriod >>  8) & 0xff;
      result[0] = newPeriod         & 0xff;

      if (write(fd, result, 4) != 4) {
        perror("write");
        return 1;
      }

      if (verbose) {
        printf("%u : Received command: 'settck'\n", (int)time(NULL));
        printf("\t Replied with %d\n\n", newPeriod);
      }
      break;
    } else if (memcmp(cmd, "sh", 2) == 0) {
      if (sread(fd, cmd, 4) != 1)
        return 1;
      if (verbose) {
        printf("%u : Received command: 'shift'\n", (int)time(NULL));
      }
    } else {
      fprintf(stderr, "invalid cmd '%s'\n", cmd);
      return 1;
    }

    int len;
    if (sread(fd, &len, 4) != 1) {
      fprintf(stderr, "reading length failed\n");
      return 1;
    }

    int nr_bytes = (len + 7) / 8;
    if (nr_bytes * 2 > sizeof(buffer)) {
      fprintf(stderr, "buffer size exceeded\n");
      return 1;
    }

    if (sread(fd, buffer, nr_bytes * 2) != 1) {
      fprintf(stderr, "reading data failed\n");
      return 1;
    }
    memset(result, 0, nr_bytes);

    if (verbose) {
      printf("\tNumber of Bits  : %d\n", len);
      printf("\tNumber of Bytes : %d \n", nr_bytes);
      printf("\n");
    }

    lynsyn_shift(len, &buffer[0], &buffer[nr_bytes], result);

    if (write(fd, result, nr_bytes) != nr_bytes) {
      perror("write");
      return 1;
    }
  } while (1);

  /* Note: Need to fix JTAG state updates, until then no exit is allowed */
  return 0;
}

int main(int argc, char **argv) {
  int i;
  int s;
  int c;
  struct sockaddr_in address;
  char hostname[256];

  opterr = 0;

  while ((c = getopt(argc, argv, "v")) != -1) {
    switch (c) {
      case 'v':
        verbose = 1;
        break;
      case '?':
        fprintf(stderr, "usage: %s [-v]\n", *argv);
        return 1;
    }
  }

  s = socket(AF_INET, SOCK_STREAM, 0);

  if (s < 0) {
    perror("socket");
    return 1;
  }

  i = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(XVC_PORT);
  address.sin_family = AF_INET;

  if (bind(s, (struct sockaddr*) &address, sizeof(address)) < 0) {
    perror("bind");
    return 1;
  }

  if (listen(s, 0) < 0) {
    perror("listen");
    return 1;
  }

  if (gethostname(hostname, sizeof(hostname)) != 0) {
    perror("hostname lookup");
    close(s);
    return 1;
  }

  printf("INFO: To connect to this lynsyn_xvc instance, use url: TCP:%s:%u\n\n", hostname, XVC_PORT);

  fd_set conn;
  int maxfd = 0;

  FD_ZERO(&conn);
  FD_SET(s, &conn);

  maxfd = s;

  while (1) {
    fd_set read = conn, except = conn;
    int fd;

    if (select(maxfd + 1, &read, 0, &except, 0) < 0) {
      perror("select");
      break;
    }

    for (fd = 0; fd <= maxfd; ++fd) {
      if (FD_ISSET(fd, &read)) {
        if (fd == s) {
          int newfd;
          socklen_t nsize = sizeof(address);

          newfd = accept(s, (struct sockaddr*) &address, &nsize);

          printf("connection accepted - fd %d\n", newfd);
          if (newfd < 0) {
            perror("accept");
          } else {
            if(!lynsyn_init()) {
              fprintf(stderr, "Failed to init lynsyn\n");
              return -1;
            }

            printf("setting TCP_NODELAY to 1\n");
            int flag = 1;
            int optResult = setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
            if (optResult < 0)
              perror("TCP_NODELAY error");
            if (newfd > maxfd) {
              maxfd = newfd;
            }
            FD_SET(newfd, &conn);
          }
        } else if (handle_data(fd)) {
          printf("connection closed - fd %d\n", fd);
          close(fd);
          FD_CLR(fd, &conn);
          lynsyn_release();
        }
      } else if (FD_ISSET(fd, &except)) {
        printf("connection aborted - fd %d\n", fd);
        close(fd);
        FD_CLR(fd, &conn);
        lynsyn_release();
        if (fd == s)
          break;
      }
    }
  }

  return 0;
}
