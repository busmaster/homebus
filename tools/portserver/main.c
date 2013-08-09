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

#define _XOPEN_SOURCE
#undef  DEBUG_LOG

#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <unistd.h>
#include <stropts.h>
#include <signal.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include <errno.h>
#include "sio.h"

#define NUM_MAX_SLAVES     20
#define NUM_PTS            (NUM_MAX_SLAVES + 1)
#define TMP_DIR            "/tmp/busportserver"

#define CMD_ADD            "add device"
#define CMD_REMOVE         "remove device"

struct ptyDesc {
   int ptmFd;
   int ptsFd;
   bool hup;
};

static char sTmpFileName[200];

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

void Cmd(int cmdFd, struct ptyDesc *pty, int numPtm) {
   
   char buf[100];
   char *ptsName;
   int len;
   int i;
   
   len = read(cmdFd, buf, sizeof(buf) - 1);
   buf[len] = '\0';
   
   if (strncmp(buf, CMD_ADD, strlen(CMD_ADD)) == 0) {
      // find unused index
      for (i = 0; i < numPtm; i++) {
         if ((pty + i)->ptmFd == -1) {
            break;
         }
      }
      if (i == numPtm) {
         // exceeded
         len = snprintf(buf, sizeof(buf), "error: number of pty devices exceeded\n");
         write(cmdFd, buf, len);
      } else {
         (pty + i)->ptmFd = open("/dev/ptmx", O_RDWR | O_NOCTTY);

         make_nonblocking((pty + i)->ptmFd);
         grantpt((pty + i)->ptmFd);
         unlockpt((pty + i)->ptmFd);
         ptsName = ptsname((pty + i)->ptmFd);
         // open + close slave to get initial HUP event
         (pty + i)->ptsFd = open(ptsName, O_RDWR);
         close((pty + i)->ptsFd);
         write(cmdFd, ptsname((pty + i)->ptmFd), strlen(ptsName));
         write(cmdFd, "\n", 1);
      }
   } else if (strncmp(buf, CMD_REMOVE, strlen(CMD_REMOVE)) == 0) {
      ptsName = buf + strlen(CMD_REMOVE) + 1;
      for (i = 0; i < numPtm; i++) {
         if (strcmp(ptsName, ptsname((pty + i)->ptmFd)) == 0) {
            close((pty + i)->ptmFd);
            (pty + i)->ptmFd = -1;
         }
      }
      if (i == numPtm) {
         // did not find
         len = snprintf(buf, sizeof(buf), "error: unable to find %s\n", ptsName);
         write(cmdFd, buf, len);
      } else {
         write(cmdFd, ptsname((pty + i)->ptmFd), strlen(ptsName));
         write(cmdFd, "\n", 1);
      }
   }
}

void sighandler(int sig) {

    unlink(sTmpFileName);
    rmdir(TMP_DIR);

    exit(0);
}

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
   }
}

#else

void LogOpen(char *pName) { }
void LogClose(void) { }
void LogPrint(char *format, ...) { }  

#endif

/*-----------------------------------------------------------------------------
*  program start
*/
int main(int argc, char *argv[]) {

    int sioHandle;
    struct ptyDesc pty[NUM_PTS];
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
    struct pollfd pfd[NUM_PTS];
    int    pfdidx2ptyidx[NUM_PTS];
    struct timeval tv;

    char *ptsName;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s serial_device\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    SioInit();

    sioHandle = SioOpen(argv[1], eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);

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
    pty[0].ptmFd = open("/dev/ptmx", O_RDWR | O_NOCTTY);
    grantpt(pty[0].ptmFd);
    unlockpt(pty[0].ptmFd);
    ptsName = ptsname(pty[0].ptmFd);
    pty[0].ptsFd = open(ptsName, O_RDWR);

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
   
    for (i = 1; i < NUM_PTS; i++) {
        pty[i].ptmFd = -1;
        pty[i].hup = true;
    }

    signal(SIGINT, sighandler);
    signal(SIGHUP, sighandler);
    signal(SIGTERM, sighandler);

    LogOpen("/tmp/portserver.log");

    while (1) {

        for (i = 1, j = 0; i < NUM_PTS; i++) {
            if (pty[i].ptmFd != -1) {
                pfd[j].fd = pty[i].ptmFd;
                pfd[j].events = POLLHUP;
                pfdidx2ptyidx[j] = i;
                j++;
            }
        }
        poll(pfd, j, 0);
        for (i = 0; i < j; i++) {
           if (pfd[i].revents & POLLHUP) {
               pty[pfdidx2ptyidx[i]].hup = true;
           } else {
               pty[pfdidx2ptyidx[i]].hup = false;
           }
        }

        maxFd = sioFd;
        FD_ZERO(&fds);
        FD_SET(sioFd, &fds);
        for (i = 0, j = 0; i < NUM_PTS; i++) {
            if ((pty[i].ptmFd != -1) && !pty[i].hup) {
                FD_SET(pty[i].ptmFd, &fds);
                maxFd = max(maxFd, pty[i].ptmFd);
            }
        }
        
        // select with timeout to get cyclic check of pts opened when no data
        // transfer is in progress
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        result = select(maxFd + 1, &fds, 0, 0, &tv);
        if (result > 0) {
            if (FD_ISSET(pty[0].ptmFd, &fds)) {
                Cmd(pty[0].ptmFd, &pty[1], NUM_MAX_SLAVES);
            }
            for (i = 1; i < NUM_PTS; i++) {  
                if ((pty[i].ptmFd != -1) &&
                    FD_ISSET(pty[i].ptmFd, &fds)) {
                    len = read(pty[i].ptmFd, buf, sizeof(buf));
                    if (len != -1) {
                        buf[len] = 0;
                        LogPrint("read ptm %d: %s\n", pty[i].ptmFd, buf);
                        lenWr = write(sioFd, buf, len);
                        if (lenWr != len) {
                            LogPrint("write sio: buf len %d, len written\n", len, lenWr);
                        }
                    } else {
                        LogPrint("read ptm %d: errno %d (%s)\n", pty[i].ptmFd, errno, strerror(errno));
                        break;
                    }
                }
            }

            if (FD_ISSET(sioFd, &fds)) {
                len = read(sioFd, buf, sizeof(buf));
                if (len != -1) {
                    buf[len] = 0;
                    LogPrint("read sio: %s\n", buf);
                    // write to all pty
                    for (i = 1; i < NUM_PTS; i++) {      
                        if ((pty[i].ptmFd != -1) && !pty[i].hup) {
                            lenWr = write(pty[i].ptmFd, buf, len);
                            if (lenWr != len) {
                               LogPrint("write ptm %d: buf len %d, len written\n", pty[i].ptmFd, len, lenWr);
                            }
                        }
                    }
                } else {
                    LogPrint("read sio: errno %d (%s)\n", errno, strerror(errno));
                }
            }  
        } else if (result < 0) {
            LogPrint("select error\n");
        }
// usleep(100000); 
    }


    return 0;
}
