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

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef WIN32
#include <conio.h>
#include <windows.h>
#endif
#include <time.h>
#include <unistd.h>

#include "sio.h"
#include "aes.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

#define SIZE_COMPORT   100
#define MAX_NAME_LEN   255
#define ESC 0x1B

/*-----------------------------------------------------------------------------
*  Typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/
static FILE  *spOutput;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PrintUsage(void);

void gotoxy(int x,int y)
{
  printf("%c[%d;%df",0x1B,y,x);
}

#ifndef WIN32
#include <sys/select.h>
#include <termios.h>

struct termios orig_termios;

void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}
#endif

/*-----------------------------------------------------------------------------
*  Programstart
*/
int main(int argc, char *argv[]) {

   int            handle;
   int            i;
   uint8_t	  j;
   char           comPort[SIZE_COMPORT] = "";
   uint8_t ch;
   char    cKb = 0;
   uint8_t count = 0;

   /* COM-Port ermitteln */
   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-c") == 0) {
         if (argc > i) {
            strncpy(comPort, argv[i + 1], sizeof(comPort) - 1);
            comPort[sizeof(comPort) - 1] = 0;
         }
         break;
      }
   }
   if (strlen(comPort) == 0) {
      PrintUsage();
      return 0;
   }

#ifndef WIN32
   set_conio_terminal_mode();
#endif

   SioInit();
   handle = SioOpen(comPort, eSioBaud9600, eSioDataBits8, eSioParityEven, eSioStopBits1, eSioModeHalfDuplex);

   if (handle == -1) {
      printf("cannot open %s\r\n", comPort);
      return 0;
   }

  system("clear");

  spOutput = stdout;


  static unsigned char key[16] = { add your key here };
  static unsigned char iv[16]  = { 0x2d, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  static unsigned char io_buffer[256];
  static unsigned char buffer[80];
  static unsigned char temp_buffer[256];
uint8_t offset = 82;

//unsigned int chcnt = 0;

   do {
      if (SioRead(handle, &ch, 1) == 1) {

//if ((chcnt % 16) == 0) {
//    printf("\r\n");
//}
//printf("%02x ", ch);
//chcnt++;

         io_buffer[count++]=ch;
         fflush(spOutput);

        if(ch == 0x16){
         if((io_buffer[count-2]==0x30)&&(io_buffer[count-3]==0xF0)&&(io_buffer[count-4]==0x40)&&(io_buffer[count-5]==0x10)) {
            ch = 0xe5;
            SioWrite(handle, &ch , 1);
//printf("\r\n write 1 e5\r\n");
//chcnt = 0;
           count = 0;
           } // Start seqenz
         else if (count >= 101){
            ch = 0xe5;
            SioWrite(handle, &ch , 1);
//printf("\r\n write 2 e5\r\n");
//chcnt = 0;
            for(j=8;j<16;j++) {
               iv[j] = io_buffer[count-86];
               }
            for(j = 0; j < 80; ++j) {
               temp_buffer[j]=io_buffer[count-82+j];
               }
            AES128_CBC_decrypt_buffer(buffer+0, temp_buffer, 16, key, iv);
            AES128_CBC_decrypt_buffer(buffer+16, temp_buffer+16, 16, 0, 0);
            AES128_CBC_decrypt_buffer(buffer+32, temp_buffer+32, 16, 0, 0);
            AES128_CBC_decrypt_buffer(buffer+48, temp_buffer+48, 16, 0, 0);
            AES128_CBC_decrypt_buffer(buffer+64, temp_buffer+64, 16, 0, 0);
gotoxy(0,0);
            fprintf(spOutput,"\r\n   Datum+Uhrzeit:        20%02d-%02d-%02d %2d:%02d:%02d     \r\n",
                   (buffer[7]>>5)+((buffer[8]&0xF0)>>1),buffer[8]&0x0F,buffer[7]&0x1F,buffer[6]&0x1F,buffer[5]&0x3F,buffer[4]&0x3F);
            fprintf(spOutput,"   Zählerstand Energie A+:        %10d   Wh     \r\n", 
                    buffer[12]+256*buffer[13]+256*256*buffer[14]+256*256*256*buffer[15]);
            fprintf(spOutput,"   Zählerstand Energie A-:        %10d   Wh     \r\n", 
                    buffer[19]+256*buffer[20]+256*256*buffer[21]+256*256*256*buffer[22]);
            fprintf(spOutput,"   Zählerstand Energie R+:        %10d varh     \r\n",
                    buffer[28]+256*buffer[29]+256*256*buffer[30]+256*256*256*buffer[31]);
            fprintf(spOutput,"   Zählerstand Energie R-:        %10d varh     \r\n", 
                    buffer[38]+256*buffer[39]+256*256*buffer[40]+256*256*256*buffer[41]);
            fprintf(spOutput,"   momentane Wirkleistung P+:     %10d    W     \r\n", 
                    buffer[44]+256*buffer[45]+256*256*buffer[46]+256*256*256*buffer[47]);
            fprintf(spOutput,"   momentane Wirkleistung P-:     %10d    W     \r\n", 
                    buffer[51]+256*buffer[52]+256*256*buffer[53]+256*256*256*buffer[54]);
            fprintf(spOutput,"   momentane Blindleistung Q+:    %10d  Var     \r\n",
                    buffer[58]+256*buffer[59]+256*256*buffer[60]+256*256*256*buffer[61]);
            fprintf(spOutput,"   momentane Blindleistung Q-:    %10d  Var     \r\n",
                    buffer[66]+256*buffer[67]+256*256*buffer[68]+256*256*256*buffer[69]);

            fflush(spOutput);
            count = 0;
            }
         }
      }
      if (kbhit()) {
         cKb = getch();
      }
   } while (cKb != ESC);

   if (handle != -1) {
      SioClose(handle);
   }

   return 0;
}

/*-----------------------------------------------------------------------------
*  Hilfe anzeigen
*/
static void PrintUsage(void) {

   printf("\r\nUsage:");
   printf("smartmeter -c port\r\n");
   printf("port: com1 com2 ..\r\n");
}

