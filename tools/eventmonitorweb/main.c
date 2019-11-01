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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
#define SIZE_COMPORT   100
#define MAX_NAME_LEN   255
#define SIZE_CMD_BUF   20


/* eigene Adresse am Bus */
#define MY_ADDR1    140

/*-----------------------------------------------------------------------------
*  Typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/
static FILE  *spOutput;

typedef struct {
          uint8_t adress;
          uint8_t digOutByte[4];
          uint8_t shader[15];
         }do31_data;		

/*-----------------------------------------------------------------------------
*  Functions
*/

/*-----------------------------------------------------------------------------
*  program start
*/
int main(int argc, char *argv[]) {

    int           handle;
    int           i;
    char          comPort[SIZE_COMPORT] = "/dev/hausbus_event";
    char          logFile[MAX_NAME_LEN] = "/usr/share/nginx/www/app/log.log";
    uint8_t       myAddr = MY_ADDR1;
    bool          myAddrValid = false;
    uint8_t       busRet;
    TBusTelegram  *pBusMsg;
    uint8_t       ch;
    uint8_t       val8;
    FILE           *pLogFile = 0;
    do31_data     save_data,tmp_data;
    /* get com interface */

   if (strlen(logFile) != 0) {
      pLogFile = fopen(logFile, "r+b");
      if (pLogFile == 0) {
         pLogFile = fopen(logFile,"w+b");
         if (pLogFile == 0) {
            printf("cannot open %s\r\n", logFile);
            return 0;
         } else {
            printf("logging to new file %s\r\n", logFile);
         }
      } else {
         printf("logging to %s\r\n", logFile);
      }
      spOutput = pLogFile;
   } else {
      printf("logging to console\r\n");
      spOutput = stdout;
   }
    /* our bus address */

 myAddrValid = true;
    if ((strlen(comPort) == 0) ||
        !myAddrValid) {
        return 0;
    }
    
    SioInit();
    handle = SioOpen(comPort, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);
    if (handle == -1) {
        printf("cannot open %s\r\n", comPort);
        if (pLogFile != 0) {
           fclose(pLogFile);
        }
        return 0;
    }
    while (SioGetNumRxChar(handle) > 0) {
        SioRead(handle, &ch, sizeof(ch));
    }
    BusInit(handle);

    for (;;) {
        busRet = BusCheck();
        if (busRet == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevReqActualValueEvent) && ((pBusMsg->msg.devBus.receiverAddr == myAddr))) {
                printf("%03d;", pBusMsg->senderAddr);
		        save_data.adress = pBusMsg->senderAddr;
                switch (pBusMsg->msg.devBus.x.devReq.actualValueEvent.devType) {
                case eBusDevTypeDo31:
                    for (i = 0; i < BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE; i++) {
                       printf("%d;", 
                           pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.digOut[i]);
                       save_data.digOutByte[i] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.digOut[i];
                    }
                    for (i = 0; i < BUS_DO31_SHADER_SIZE_ACTUAL_VALUE; i++) {
                       printf("%d;", 
                           pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.shader[i]);
                       save_data.shader[i] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.shader[i];
                    }
                    printf("\n");
                    break;
                case eBusDevTypeSw8:
                        val8 = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.sw8.state;
                        printf("%d;",val8);
                        save_data.digOutByte[0]=val8;

                        for (i = 1; i < BUS_DO31_DIGOUT_SIZE_ACTUAL_VALUE; i++) {
                            printf("%d;",0);
                            save_data.digOutByte[i] = 0;
                        }
                    for (i = 0; i < BUS_DO31_SHADER_SIZE_ACTUAL_VALUE; i++) {
                       printf("%d;",0);
                       save_data.shader[i] = 0;
                    }
					printf("\n");
                    break;
                 case eBusDevTypePwm16:
                            printf("%d;",pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.state&0xFF);
                            save_data.digOutByte[0] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.state&0xFF;
                            printf("%d;",(pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.state>>8)&0xFF);
                            save_data.digOutByte[1] = (pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.state>>8)&0xFF;							
                        for (i = 0; i < BUS_PWM16_PWM_SIZE_ACTUAL_VALUE; i++) {
                            printf("%d;",pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[i]);                            
                        }		
						save_data.digOutByte[2] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[0];
						save_data.digOutByte[3] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[1];
						save_data.shader[0] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[2];
						save_data.shader[1] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[3];
						save_data.shader[2] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[4];
						save_data.shader[3] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[5];
						save_data.shader[4] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[6];
						save_data.shader[5] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[7];
						save_data.shader[6] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[8];
						save_data.shader[7] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[9];
						save_data.shader[8] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[10];
						save_data.shader[9] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[11];
						save_data.shader[10] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[12];
						save_data.shader[11] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[13];
						save_data.shader[12] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[14];		
						save_data.shader[13] = pBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.pwm16.pwm[15];	
                        save_data.shader[14] = 0;						
                        printf("\n");						
                        break;					
                default:
                    break;
                }

		        fseek(spOutput, 0, SEEK_SET);
                    while(fread(&tmp_data, sizeof(tmp_data), 1, spOutput) == 1){
                      if(tmp_data.adress == save_data.adress){
                         fseek(spOutput, - sizeof(save_data) , SEEK_CUR);
                         break;
                      }
                   }
	               fwrite(&save_data, sizeof(save_data), 1, spOutput);
                   fseek(spOutput, 0, SEEK_END);
                   fflush(spOutput);
            }
        }
    }

    SioClose(handle);
    if (pLogFile != 0) {
       fclose(pLogFile);
    }
    return 0;
}


