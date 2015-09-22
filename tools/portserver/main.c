/*
 * main.c
 *
 * Copyright 2013 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE

#undef  DEBUG_LOG

#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <signal.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include "sio.h"

#define NUM_MAX_SLAVES     20
#define NUM_PTS            (NUM_MAX_SLAVES + 1)
#define TMP_DIR            "/tmp/busportserver"

#define CMD_ADD            "add device"
#define CMD_REMOVE         "remove device"

#define SIO_DEV_NAME       argv[1]

struct ptyDesc {
   int ptmFd;
   int ptsFd;
   bool closed;
   int wd;
};

static char sTmpFileName[200];

#ifdef DEBUG_LOG

static FILE *logFile = 0;

void LogOpen(char *pName) {
    logFile = fopen(pName, "w");
}
void LogClose(void) {
    if (logFile != 0) {
       fclose(logFile);
    }
}

void LogPrint(char *format, ...) {
   va_list args;
   if (logFile != 0) {
      va_start(args, format);
      vfprintf(logFile, format, args);
      va_end(args);
      fflush(logFile);
   }
}

#else

void LogOpen(char *pName) { }
void LogClose(void) { }
void LogPrint(char *format, ...) { }

#endif

int make_nonblocking(int fd)
{
  int flags;

  flags = fcntl(fd, F_GETFL, 0);
  if(flags == -1) /* Failed? */
   return 0;
  /* Clear the blocking flag. */
  flags |= O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags) != -1;
}

void Cmd(int cmdFd, struct ptyDesc *pty, int numPtm, int notifyFd) {

    char buf[100];
    char *ptsName;
    int len;
    int i;
    struct ptyDesc *p = pty;

    len = read(cmdFd, buf, sizeof(buf) - 1);
    buf[len] = '\0';

    if (strncmp(buf, CMD_ADD, strlen(CMD_ADD)) == 0) {
        // find unused index
        for (i = 0; i < numPtm; i++) {
            if (p->ptmFd == -1) {
                break;
            }
            p++;
        }
        if (i == numPtm) {
            // exceeded
            LogPrint("error: number of pty devices exceeded\n");
            len = snprintf(buf, sizeof(buf), "error: number of pty devices exceeded\n");
            write(cmdFd, buf, len);
        } else {
            p->ptmFd = open("/dev/ptmx", O_RDWR | O_NOCTTY);

            make_nonblocking(p->ptmFd);
            grantpt(p->ptmFd);
            unlockpt(p->ptmFd);
            ptsName = ptsname(p->ptmFd);

            p->wd = inotify_add_watch(notifyFd, ptsName, IN_CLOSE_WRITE | IN_CLOSE_NOWRITE | IN_OPEN);

            LogPrint("add pts: ptmFd=%d, ptsName=%s, wd=%d\n", p->ptmFd, ptsName, p->wd);

            write(cmdFd, ptsname(p->ptmFd), strlen(ptsName));
            write(cmdFd, "\n", 1);
        }
    } else if (strncmp(buf, CMD_REMOVE, strlen(CMD_REMOVE)) == 0) {
        ptsName = buf + strlen(CMD_REMOVE) + 1;
        for (i = 0; i < numPtm; i++) {
            if (strcmp(ptsName, ptsname(p->ptmFd)) == 0) {
                close(p->ptmFd);
                inotify_rm_watch(notifyFd, p->wd);
                LogPrint("rm pts: ptmFd=%d, ptsName=%s", p->ptmFd, ptsName);
                p->ptmFd = -1;
                break;
            }
            p++;
        }
        if (i == numPtm) {
            // did not find
            LogPrint("error: unable to find %s\n", ptsName);
            len = snprintf(buf, sizeof(buf), "error: unable to find %s\n", ptsName);
            write(cmdFd, buf, len);
        } else {
            write(cmdFd, ptsname(p->ptmFd), strlen(ptsName));
            write(cmdFd, "\n", 1);
        }
    }
}

void sighandler(int sig) {

    unlink(sTmpFileName);
    rmdir(TMP_DIR);

    exit(0);
}

/*-----------------------------------------------------------------------------
*  program start
*/
int main(int argc, char *argv[]) {

    int sioHandle;
    struct ptyDesc pty[NUM_PTS];
    struct ptyDesc *p;
    int sioFd;
    fd_set fds;
    int maxFd;
    int result;
    int len;
    int lenWr;
    uint8_t buf[1000];
    int i;
    int j;
    int fd;
    char devName[100];
    char charBuf[100];
    char *ptsName;
    struct inotify_event notifyEvent;
    int                  notifyFd;

    daemon(0, 1);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s serial_device\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    SioInit();

    sioHandle = SioOpen(SIO_DEV_NAME, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);

    if (sioHandle == -1) {
        printf("cannot open %s\r\n", argv[1]);
        return 0;
    }
    sioFd = SioGetFd(sioHandle);

    snprintf(devName, sizeof(devName), "%s", argv[1]);
    // replace all / by _
    for (i = 0; i < strlen(devName); i++) {
        if (devName[i] == '/') {
            devName[i] = '_';
        }
    }
    // pty[0] is used for external cmd requests/responses
    mkdir(TMP_DIR, 0777);
    p = &pty[0];
    p->ptmFd = open("/dev/ptmx", O_RDWR | O_NOCTTY);
    grantpt(p->ptmFd);
    unlockpt(p->ptmFd);
    ptsName = ptsname(p->ptmFd);
    p->ptsFd = open(ptsName, O_RDWR);
    p->closed = false;

    // write the name of the cmd pts to file
    snprintf(sTmpFileName, sizeof(sTmpFileName), TMP_DIR "/%s_cmdPts", devName);
    fd = open(sTmpFileName,  O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd != -1) {
        snprintf(charBuf, sizeof(charBuf), "%s", ptsName);
        write(fd, charBuf, strlen(charBuf));
        close(fd);
    } else {
        fprintf(stderr, "create file %s error (errno=%d, %s)\n", sTmpFileName, errno, strerror(errno));
        SioClose(sioHandle);
        exit(EXIT_FAILURE);
    }

    p = &pty[1];
    for (i = 1; i < NUM_PTS; i++) {
        p->ptmFd = -1;
        p->closed = true;
        p++;
    }

    signal(SIGINT, sighandler);
    signal(SIGHUP, sighandler);
    signal(SIGTERM, sighandler);

    LogOpen(TMP_DIR "/portserver.log");

    notifyFd = inotify_init();

    while (1) {
        if (sioHandle == -1) {
            /* try to reopen the serial device */
            sleep(3);
            sioHandle = SioOpen(SIO_DEV_NAME, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);
            if (sioHandle == -1) {
                continue;
            } else {
                LogPrint("reopened %s\n", SIO_DEV_NAME);
                sioFd = SioGetFd(sioHandle);
            }
        }

        maxFd = sioFd;
        FD_ZERO(&fds);
        FD_SET(sioFd, &fds);
        p = pty;
        for (i = 0 ; i < NUM_PTS; i++) {
            if ((p->ptmFd != -1) && !p->closed) {
                FD_SET(p->ptmFd, &fds);
                maxFd = max(maxFd, p->ptmFd);
            }
            p++;
        }
        FD_SET(notifyFd, &fds);
        maxFd = max(maxFd, notifyFd);

        result = select(maxFd + 1, &fds, 0, 0, 0);
        if (result > 0) {
            // new command
            if (FD_ISSET(pty[0].ptmFd, &fds)) {
                Cmd(pty[0].ptmFd, &pty[1], NUM_MAX_SLAVES, notifyFd);
            }

            // pts open/close notify
            if (FD_ISSET(notifyFd, &fds)) {
                LogPrint("notify event ");
                read(notifyFd, &notifyEvent, sizeof(notifyEvent));
                p = &pty[1];
                for (i = 0; i < NUM_PTS; i++) {
                    if (p->wd == notifyEvent.wd) {
                        break;
                    }
                    p++;
                }
                if (i < NUM_PTS) {
                    LogPrint("pts of ptm %d ", p->ptmFd);
                    if (notifyEvent.mask & IN_OPEN) {
                        LogPrint("opened");
                        p->closed = false;
                    }
                    if(notifyEvent.mask & (IN_CLOSE_NOWRITE | IN_CLOSE_WRITE)) {
                        LogPrint("closed");
                        p->closed = true;
                    }
                } else {
                    LogPrint("error: did not find event.wd %d", notifyEvent.wd);
                }
                LogPrint("\n");
            }

            // rx from pts
            p = &pty[1];
            for (i = 1; i < NUM_PTS; i++) {
                if ((p->ptmFd != -1) &&
                    FD_ISSET(p->ptmFd, &fds) &&
                    !p->closed) {
                    len = read(p->ptmFd, buf, sizeof(buf));
                    if (len != -1) {
                        LogPrint("read ptm %d: ", p->ptmFd);
                        for (j = 0; j < len; j++) {
                            LogPrint("%02x ", buf[j]);
                        }
                        LogPrint("\n");
                        lenWr = write(sioFd, buf, len);
                        if (lenWr != len) {
                            LogPrint("write sio: buf len %d, len written\n", len, lenWr);
                        }
                    } else {
                        LogPrint("read ptm %d: errno %d (%s)\n", p->ptmFd, errno, strerror(errno));
                        break;
                    }
                }
                p++;
            }

            // rx from tty
            if (FD_ISSET(sioFd, &fds)) {
                len = read(sioFd, buf, sizeof(buf));
                if (len > 0) {
                    LogPrint("read sio: ", buf);
                    for (j = 0; j < len; j++) {
                        LogPrint("%02x ", buf[j]);
                    }
                    LogPrint("\n");
                    // write to all pty
                    p = &pty[1];
                    for (i = 1; i < NUM_PTS; i++) {
                        if ((p->ptmFd != -1) &&
                            !p->closed) {
                            lenWr = write(p->ptmFd, buf, len);
                            if (lenWr != len) {
                               LogPrint("write ptm %d: buf len %d, len written\n", p->ptmFd, len, lenWr);
                            }
                        }
                        p++;
                    }
                } else if (len == 0) {
                    LogPrint("read sio: %s not available\n", SIO_DEV_NAME);
                    SioClose(sioHandle);
                    sioHandle = -1;
                } else {
                    LogPrint("read sio: errno %d (%s)\n", errno, strerror(errno));
                }
            }
        } else if (result < 0) {
            LogPrint("select error\n");
        }
// usleep(1000000);
    }

    return 0;
}
