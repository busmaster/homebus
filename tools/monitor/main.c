/*-----------------------------------------------------------------------------
*  Main.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include <conio.h>
#include <windows.h>
#endif
#include <time.h>
#include <unistd.h>

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

#define SIZE_COMPORT   100
#define MAX_NAME_LEN   255

#define STX 0x02
#define ESC 0x1B             
/* Startwert f�r checksumberechung */
#define CHECKSUM_START   0x55        

#define SPACE "                             "

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
static void BusMonDecoded(int sioHandle);
static void BusMonRaw(int sioHandle);

#ifndef WIN32
#include <termios.h>


int kbhit(void) {
   struct termios term, oterm;
   int fd = 0;
   int c = 0;
   tcgetattr(fd, &oterm);
   memcpy(&term, &oterm, sizeof(term));
   term.c_lflag = term.c_lflag & (!ICANON);
   term.c_cc[VMIN] = 0;
   term.c_cc[VTIME] = 1;
   tcsetattr(fd, TCSANOW, &term);
   c = getchar();
   tcsetattr(fd, TCSANOW, &oterm);
   if (c != -1)
   ungetc(c, stdin);
   return ((c != -1) ? 1 : 0);
}

int getch()
{
   static int ch = -1, fd = 0;
   struct termios neu, alt;
   fd = fileno(stdin);
   tcgetattr(fd, &alt);
   neu = alt;
   neu.c_lflag &= ~(ICANON|ECHO);
   tcsetattr(fd, TCSANOW, &neu);
   ch = getchar();
   tcsetattr(fd, TCSANOW, &alt);
   return ch;
}
#endif

/*-----------------------------------------------------------------------------
*  Programstart
*/
int main(int argc, char *argv[]) {

   int            handle;
   int            i;
   FILE           *pLogFile = 0;
   char           *pStr;
   char           comPort[SIZE_COMPORT] = "";
   char           logFile[MAX_NAME_LEN] = "";
   bool           raw = false;

   /* COM-Port ermitteln */
   for (i = 1; i < argc; i++) {
      pStr = strstr(argv[i], "-c"); 
      if (pStr != 0) {
         pStr += 2;
         strncpy(comPort, pStr, sizeof(comPort) - 1);
         break;
      } 
   }    
   if (strlen(comPort) == 0) {
      PrintUsage();
      return 0;
   }      
          
   /* Name des Logfile ermittlen */
   for (i = 1; i < argc; i++) {
      pStr = strstr(argv[i], "-l"); 
      if (pStr != 0) {
         pStr += 2;
         strncpy(logFile, pStr, sizeof(logFile));
         break;
      }
   }    

   /* raw-Modus? */
   for (i = 1; i < argc; i++) {
      pStr = strstr(argv[i], "-raw"); 
      if (pStr != 0) {
         raw = true;
         break;
      }
   }    
          

   if (strlen(logFile) != 0) {
      pLogFile = fopen(logFile, "wb");
      if (pLogFile == 0) {
         printf("cannot open %s\r\n", logFile);
         return 0;
      } else {
         printf("logging to %s\r\n", logFile);
      }
      spOutput = pLogFile;
   } else {
      printf("logging to console\r\n");
      spOutput = stdout;
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

   if (raw) {
      BusMonRaw(handle);
   } else {
      BusMonDecoded(handle);
   }

   if (handle != -1) {
      SioClose(handle);
   }

   if (pLogFile != 0) {
      fclose(pLogFile);
   }
   
   return 0;
}

/*-----------------------------------------------------------------------------
*  Hilfe anzeigen
*/
static void PrintUsage(void) {

   printf("\r\nUsage:");
   printf("busmonitor -cport [-lfile] [-raw]\r\n");
   printf("port: com1 com2 ..\r\n");
   printf("file, if no logfile: log to console\r\n");
   printf("-raw: log hex data");
}

/*-----------------------------------------------------------------------------
*  Anzeige des Busverkehrs im hex-Mode
*/
static void BusMonRaw(int sioHandle) {

   uint8_t        ch;
   uint8_t        lastCh = 0;
   uint8_t        checkSum = 0;
   bool         charIsInverted = false;
#ifdef WIN32
   SYSTEMTIME   sysTime;
#else
   struct timespec ts;
   struct tm       *ptm;
#endif
   char         cKb = 0;

   do {
      if (SioRead(sioHandle, &ch, 1) == 1) {
         /* Zeichen empfangen */
         if (ch == STX) {
#ifdef WIN32
            GetLocalTime(&sysTime);
#else
            clock_gettime(CLOCK_REALTIME, &ts);
#endif
            /* vorherige Pr�fsumme checken */ 
            checkSum -= lastCh;           
            if (checkSum != lastCh) {
               fprintf(spOutput, "checksum error");
            }            
            /* n�chste Zeile */
            fprintf(spOutput, "\r\n");
            checkSum = CHECKSUM_START; 
#ifdef WIN32
            fprintf(spOutput, "%d-%02d-%02d %2d:%02d:%02d.%03d  ", sysTime.wYear, sysTime.wMonth, sysTime.wDay, sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
#else
            ptm = localtime(&ts.tv_sec);
            fprintf(spOutput, "%d-%02d-%02d %2d:%02d:%02d.%03d  ", ptm->tm_year, ptm->tm_mon, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)ts.tv_nsec / 1000000);
#endif
         }
         lastCh = ch;
         
         if (charIsInverted) {
            lastCh = ~lastCh;
            charIsInverted = false;
            /* Korrektur f�r folgende checksum-Berechnung */
            checkSum = checkSum - ch + ~ch;
         }
         
         if (ch == ESC) {
            charIsInverted = true;
            /* ESC-Zeichen wird nicht addiert, bei unten folgender Berechnung */
            /* aber nicht unterschieden -> ESC subtrahieren */
            checkSum -= ESC;
         }
         
         checkSum += ch;
         fprintf(spOutput, "%02x ", ch);
         fflush(spOutput);
      }             

      if (kbhit()) {
         cKb = getch();
      }
   } while (cKb != ESC);
}

/*-----------------------------------------------------------------------------
*  Decodierte Anzeige des Busverkehrs
*/
static void BusMonDecoded(int sioHandle) {

   int            i;
   uint8_t        ret;   
   TBusTelegramm  *pBusMsg;
   char           cKb = 0;

   BusInit(sioHandle);
   pBusMsg = BusMsgBufGet();
   
   do {
      ret = BusCheck();
      if (ret == BUS_MSG_OK) {
#ifdef WIN32
    	  SYSTEMTIME   sysTime;
    	  GetLocalTime(&sysTime);
          fprintf(spOutput, "%d-%02d-%02d %2d:%02d:%02d.%03d ", sysTime.wYear, sysTime.wMonth, sysTime.wDay, sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
#else
    	  struct timespec ts;
          struct tm       *ptm;
    	  clock_gettime(CLOCK_REALTIME, &ts);
          ptm = localtime(&ts.tv_sec);
          fprintf(spOutput, "%d-%02d-%02d %2d:%02d:%02d.%03d  ", ptm->tm_year, ptm->tm_mon, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)ts.tv_nsec / 1000000);
#endif

         fprintf(spOutput, "%4d ", pBusMsg->senderAddr);

         switch (pBusMsg->type) {
            case eBusButtonPressed1:
               fprintf(spOutput, "button 1 pressed ");
               break;
            case eBusButtonPressed2:
               fprintf(spOutput, "button 2 pressed ");
               break;
            case eBusButtonPressed1_2:
               fprintf(spOutput, "buttons 1 and 2 pressed ");
               break;
            case eBusDevReqReboot:
               fprintf(spOutput, "request reboot ");
               fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
               break;
            case eBusDevReqUpdEnter:
               fprintf(spOutput, "request update enter ");
               fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
               break;
            case eBusDevRespUpdEnter:
               fprintf(spOutput, "response update enter ");
               fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
               break;
            case eBusDevReqUpdData:
               fprintf(spOutput, "request update data ");
               fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
               fprintf(spOutput, "wordaddr %x ", pBusMsg->msg.devBus.x.devReq.updData.wordAddr);
               fprintf(spOutput, "data: ");
               for (i = 0; i < BUS_FWU_PACKET_SIZE / 2; i++) {
                  fprintf(spOutput, "%04x ", pBusMsg->msg.devBus.x.devReq.updData.data[i]);                   
               }
               break;
            case eBusDevRespUpdData:
               fprintf(spOutput, "response update data ");
               fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
               fprintf(spOutput, "wordaddr %x ", pBusMsg->msg.devBus.x.devResp.updData.wordAddr);
               break;
            case eBusDevReqUpdTerm:
               fprintf(spOutput, "request update terminate ");
               fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
               break;
            case eBusDevRespUpdTerm:
               fprintf(spOutput, "response update terminate ");
               fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
               fprintf(spOutput, "success %d ", pBusMsg->msg.devBus.x.devResp.updTerm.success);
               break;
            case eBusDevReqInfo:
               fprintf(spOutput, "request info ");
               fprintf(spOutput, "receiver %d ", pBusMsg->msg.devBus.receiverAddr);
               break;
            case eBusDevRespInfo:
               fprintf(spOutput, "response info\r\n");
               switch (pBusMsg->msg.devBus.x.devResp.info.devType) {
                  case eBusDevTypeDo31:
                     fprintf(spOutput, SPACE "device DO31\r\n");
                     fprintf(spOutput, SPACE "shader configuration:\r\n");
                     for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
                        uint8_t onSw = pBusMsg->msg.devBus.x.devResp.info.devInfo.do31.onSwitch[i];
                        uint8_t dirSw = pBusMsg->msg.devBus.x.devResp.info.devInfo.do31.dirSwitch[i];
                        if ((dirSw == 0xff) && 
                            (onSw == 0xff)) {
                            continue;
                        }
                        fprintf(spOutput, SPACE "   shader %d:\r\n", i);
                        if (dirSw != 0xff) {
                           fprintf(spOutput, SPACE "      onSwitch: %d\r\n", onSw);
                        }
                        if (onSw != 0xff) {                             
                           fprintf(spOutput, SPACE "      dirSwitch: %d\r\n", dirSw);
                        }
                     }
                     break;
                  case eBusDevTypeSw8:
                     fprintf(spOutput, SPACE "device SW8\r\n");
                     break;
                  default:
                     fprintf(spOutput, SPACE "device unknown\r\n");
                     break;
               }
               fprintf(spOutput, SPACE "version: %s", pBusMsg->msg.devBus.x.devResp.info.version);
               break;
            case eBusDevReqSetState:
               fprintf(spOutput, "request set state ");
               fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
               switch (pBusMsg->msg.devBus.x.devReq.setState.devType) {
                  case eBusDevTypeDo31:
                     fprintf(spOutput, SPACE "device DO31\r\n");
                     fprintf(spOutput, SPACE "output state:\r\n");
                     for (i = 0; i < 31 /* 31 DO's */; i++) {
                        uint8_t state = (pBusMsg->msg.devBus.x.devReq.setState.state.do31.digOut[i / 4] >> ((i % 4) * 2)) & 0x3;
                        if (state != 0) {
                           fprintf(spOutput, SPACE "   DO%d: ", i);
                           switch (state) {
                              case 0x02:
                                 fprintf(spOutput, "OFF");
                                 break;
                              case 0x03:
                                 fprintf(spOutput, "ON");
                                 break;
                              default:
                                 fprintf(spOutput, "invalid state");
                                 break;
                           }
                           fprintf(spOutput, "\r\n");
                        } else {
                           continue;    
                        }
                     }
                     fprintf(spOutput, SPACE "shader state:\r\n");
                     for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
                        uint8_t state = (pBusMsg->msg.devBus.x.devReq.setState.state.do31.shader[i / 4] >> ((i % 4) * 2)) & 0x3;
                        if (state != 0) {
                           fprintf(spOutput, SPACE "   SHADER%d: ", i);
                           switch (state) {
                              case 0x01:
                                 fprintf(spOutput, "OPEN");
                                 break;
                              case 0x02:
                                 fprintf(spOutput, "CLOSE");
                                 break;
                              case 0x03:
                                 fprintf(spOutput, "STOP");
                                 break;
                              default:
                                 fprintf(spOutput, "invalid state");
                                 break;
                           }
                           fprintf(spOutput, "\r\n");          
                        } else {
                           continue;    
                        }
                     }
                     break;
                  default:
                     fprintf(spOutput, SPACE "device unknown");
                     break;
               }
               break;
            case eBusDevRespSetState:
               fprintf(spOutput, "response set state ");
               fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
               break;
            case eBusDevReqGetState:
               fprintf(spOutput, "request get state ");
               fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
               break;
            case eBusDevRespGetState:
               fprintf(spOutput, "response get state ");
               fprintf(spOutput, "receiver %d\r\n", pBusMsg->msg.devBus.receiverAddr);
               switch (pBusMsg->msg.devBus.x.devResp.getState.devType) {
                  case eBusDevTypeDo31:
                     fprintf(spOutput, SPACE "device DO31\r\n");
                     fprintf(spOutput, SPACE "output state: ");
                     for (i = 0; i < 31 /* 31 DO's */; i++) {
                        uint8_t state = (pBusMsg->msg.devBus.x.devResp.getState.state.do31.digOut[i / 8] >> (i % 8)) & 0x1;
                        if (state == 0) {
                            fprintf(spOutput, "0");
                        } else {    
                            fprintf(spOutput, "1");
                        }    
                     }                               
                     fprintf(spOutput, "\r\n");                     
                     fprintf(spOutput, SPACE "shader state:\r\n");
                     for (i = 0; i < BUS_DO31_NUM_SHADER; i++) {
                        uint8_t state = (pBusMsg->msg.devBus.x.devResp.getState.state.do31.shader[i / 4] >> ((i % 4) * 2)) & 0x3;
                        if (state != 0) {
                           fprintf(spOutput, SPACE "   SHADER%d: ", i);
                           switch (state) {
                              case 0x01:
                                 fprintf(spOutput, "OPENING");
                                 break;
                              case 0x02:
                                 fprintf(spOutput, "CLOSING");
                                 break;
                              case 0x03:
                                 fprintf(spOutput, "STOPPED");
                                 break;
                              default:
                                 fprintf(spOutput, "invalid state");
                                 break;
                           }
                           fprintf(spOutput, "\r\n");          
                        } else {
                           continue;    
                        }
                     }
                     break;
                  case eBusDevTypeSw8:
                     fprintf(spOutput, SPACE "device SW8\r\n");
                     fprintf(spOutput, SPACE "switch state: ");
                     for (i = 0; i < 8 /* 8 Switches */; i++) {
                        uint8_t state = (pBusMsg->msg.devBus.x.devResp.getState.state.sw8.switchState >> i) & 0x1;
                        if (state == 0) {
                            fprintf(spOutput, "0");
                        } else {    
                            fprintf(spOutput, "1");
                        }    
                     }                               
                     fprintf(spOutput, "\r\n");                     
                     break;
                  default:   
                     fprintf(spOutput, SPACE "device unknown");
                     break;
               }
               break;
            case eBusDevReqSwitchState:
               fprintf(spOutput, "request switch state ");
               fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
               fprintf(spOutput, SPACE "switch state: ");
               for (i = 0; i < 8 /* 8 Switches */; i++) {
                  uint8_t state = (pBusMsg->msg.devBus.x.devReq.switchState.switchState >> i) & 0x1;
                  if (state == 0) {
                     fprintf(spOutput, "0");
                  } else {    
                     fprintf(spOutput, "1");
                  }    
               }
               break;
            case eBusDevRespSwitchState:
               fprintf(spOutput, "response switch state ");
               fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
               fprintf(spOutput, SPACE "switch state: ");
               for (i = 0; i < 8 /* 8 Switches */; i++) {
                  uint8_t state = (pBusMsg->msg.devBus.x.devResp.switchState.switchState >> i) & 0x1;
                  if (state == 0) {
                     fprintf(spOutput, "0");
                  } else {    
                     fprintf(spOutput, "1");
                  }    
               }
               break;
            case eBusDevReqSetClientAddr:
               fprintf(spOutput, "request set client address ");
               fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
               fprintf(spOutput, SPACE "client addresses: ");
               for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
                  uint8_t address = pBusMsg->msg.devBus.x.devReq.setClientAddr.clientAddr[i];
                  fprintf(spOutput, "%02x ", address);
               }
               break;
            case eBusDevRespSetClientAddr:
               fprintf(spOutput, "response set client address ");
               fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
               break;
            case eBusDevReqGetClientAddr:
               fprintf(spOutput, "request get client address ");
               fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
               break;
            case eBusDevRespGetClientAddr:
               fprintf(spOutput, "response get client address ");
               fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
               fprintf(spOutput, SPACE "client addresses: ");
               for (i = 0; i < BUS_MAX_CLIENT_NUM; i++) {
                  uint8_t address = pBusMsg->msg.devBus.x.devResp.getClientAddr.clientAddr[i];
                  fprintf(spOutput, "%02x ", address);
               }
               break;
            case eBusDevReqSetAddr:
               fprintf(spOutput, "request set address ");
               fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
               fprintf(spOutput, SPACE "address: ");
               {
                  uint8_t address = pBusMsg->msg.devBus.x.devReq.setAddr.addr;
                  fprintf(spOutput, "%02x", address);
               }
               break;
            case eBusDevRespSetAddr:
               fprintf(spOutput, "response set address ");
               fprintf(spOutput, "receiver %d", pBusMsg->msg.devBus.receiverAddr);
               break;
            case eBusDevStartup:
               fprintf(spOutput, "device startup ");
               break;
            default:
               fprintf(spOutput, "unknown frame type %x ", pBusMsg->type);
               break;
         }
         fprintf(spOutput, "\r\n");
      } else if (ret == BUS_MSG_ERROR) {
         fprintf(spOutput, "frame error\r\n");             
      }
      fflush(spOutput);
      if (kbhit()) {
         cKb = getch();
      }
      if (ret != BUS_MSG_OK) {
#ifdef WIN32
    	  Sleep(10);
#else
    	  usleep(10000);
#endif
      }
   } while (cKb != ESC);
}