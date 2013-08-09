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

#include <fcntl.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stropts.h>
#include <signal.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include <errno.h>


static void SetDefaultPtyParam(int fd) {
   struct termios  config;
   if(!isatty(fd)) {
      printf("no tty\n");
      return;
   }
   if(tcgetattr(fd, &config) < 0) {
      printf("tcgetattr error\n");
      return;
   }
   
   //
   // Input flags - Turn off input processing
   // convert break to null byte, no CR to NL translation,
   // no NL to CR translation, don't mark parity errors or breaks
   // no input parity check, don't strip high bit off,
   // no XON/XOFF software flow control
   //
   config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
                       INLCR | PARMRK | INPCK | ISTRIP | IXON);
   //
   // Output flags - Turn off output processing
   // no CR to NL translation, no NL to CR-NL translation,
   // no NL to CR translation, no column 0 CR suppression,
   // no Ctrl-D suppression, no fill characters, no case mapping,
   // no local output processing
   //
   // config.c_oflag &= ~(OCRNL | ONLCR | ONLRET |
   //                     ONOCR | ONOEOT| OFILL | OLCUC | OPOST);
   config.c_oflag = 0;
   //
   // No line processing:
   // echo off, echo newline off, canonical mode off, 
   // extended input processing off, signal chars off
   //
   config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
   //
   // Turn off character processing
   // clear current char size mask, no parity checking,
   // no output processing, force 8 bit input
   //
   config.c_cflag &= ~(CSIZE | PARENB);
   config.c_cflag |= CS8;
   //
   // One input byte is enough to return from read()
   // Inter-character timer off
   //
   config.c_cc[VMIN]  = 1;
   config.c_cc[VTIME] = 0;

   //
   // Finally, apply the configuration
   //
   if(tcsetattr(fd, TCSAFLUSH, &config) < 0) {
      printf("tcsetattr error\n");
      return;
   }
}

#define TEST_STRING argv[2]
/*-----------------------------------------------------------------------------
*  program start
*/
int main(int argc, char *argv[]) {

    fd_set fds;
    char rxbuf[50000];
    char txbuf[1000];
    int fd;
    int result;
    int txlen;
    int rxlen;
    bool tx = false;
    unsigned int cnt = 0;
    unsigned int missingRxCnt;
    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s tty_device teststring\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fd = open(argv[1], O_RDWR);

    if (fd == -1) {
        printf("cannot open %s\r\n", argv[1]);
        return 0;
    }

    SetDefaultPtyParam(fd);

    tcflush(fd, TCIOFLUSH);

    txlen = snprintf(txbuf, sizeof(txbuf), "%s%08x", TEST_STRING, cnt);
    write(fd, txbuf, txlen);
    missingRxCnt = 0;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        result = select(fd + 1, &fds, 0, 0, 0);
//      printf("result %d\n", result);
        if (result > 0) {
            if (FD_ISSET(fd, &fds)) {
                rxlen = read(fd, rxbuf, sizeof(rxbuf) - 1);
                rxbuf[rxlen] = 0;
                if (strstr(rxbuf, txbuf) != 0) {
                    tx = true;
                    missingRxCnt = 0;
                } else {
                    printf("tx: %s rx: %s\n", txbuf, rxbuf);
                    missingRxCnt++;
                }
                if (missingRxCnt == 10) {
                    break;
                }
            }
        }
        
        if (tx) {
            cnt++;
            txlen = snprintf(txbuf, sizeof(txbuf), "%s%08x", TEST_STRING, cnt);
            write(fd, txbuf, txlen);
            tx = false;
            printf("tx%d\n", cnt);
        }
    }

    return 0;
}
