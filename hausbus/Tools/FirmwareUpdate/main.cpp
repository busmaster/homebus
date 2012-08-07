/*-----------------------------------------------------------------------------
*  Main.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <windows.h>

#include "Sio.h"
#include "Bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
/* eigene Adresse am Bus */
#define MY_ADDR    0

/* timeout für Wiederholung */

#define TIMEOUT_MS_STARTUP_RESP      2000
#define TIMEOUT_MS_UPD_ENTER_RESP    10000  /* braucht länger, weil Eeprom und Flash gelöscht werden */
#define TIMEOUT_MS_UPD_DATA_RESP     1000
#define TIMEOUT_MS_UPD_TERM_RESP     1000


/* Wiederholungen bis zum Abbruch */
#define MAX_RETRY  10

#define COM_PORT       argv[1]
#define FIRMWARE_FILE  argv[2]
#define TARGET_ADDR    sTargetAddr 


#define WAIT_FOR_STARTUP_MSG    0
#define WAIT_FOR_UPD_ENTER_RESP 1
#define WAIT_FOR_UPD_DATA_RESP  2
#define WAIT_FOR_UPD_TERM_RESP  3

#define ESC  0x1b

/*-----------------------------------------------------------------------------
*  Typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/

static TBusTelegramm  *spBusMsg;
static UINT8          sFwuState;
static FILE           *spFile;
static UINT16         sLastWordAddr;
static BOOL           sFileTransferComplete = false;
static BOOL           sFileEmpty = false;
static unsigned int   sTimeStamp;
static UINT8          sTargetAddr;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void ProcessBus(UINT8 ret);
static bool SendDataPacket(bool next) ;
static void PrintUsage(void);

/*-----------------------------------------------------------------------------
*  Programstart
*/
int main(int argc, char *argv[]) {

   int            handle;
   char           cKb;
   UINT8          ret;
   TBusTelegramm  txBusMsg;

   if (argc != 4) {
      PrintUsage();
      return 0;
   }

   sTargetAddr = atoi(argv[3]);

   /* File mit Firmware öffnen */
   spFile = fopen(FIRMWARE_FILE, "rb");
   if (spFile == 0) {
      printf("cannot open %s\r\n", FIRMWARE_FILE);
      return 0;
   }

   SioInit();
   handle = SioOpen(COM_PORT, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1);
   if (handle == -1) {
      if (spFile != 0) {
         fclose(spFile);
      }
      return 0;
   }


   BusInit(handle);
   spBusMsg = BusMsgBufGet();

   /* Reboot-Command aussenden */
   txBusMsg.type = eBusDevReqReboot;
   txBusMsg.senderAddr = MY_ADDR;
   txBusMsg.msg.devBus.receiverAddr = TARGET_ADDR;
   BusSend(&txBusMsg);

   sFwuState = WAIT_FOR_STARTUP_MSG;
   sTimeStamp = GetTickCount();

   do {
      ret = BusCheck();
      ProcessBus(ret);
      if (sFileTransferComplete) {
         /* Update beendet */
         /* Reboot-Command aussenden */
         txBusMsg.type = eBusDevReqReboot;
         txBusMsg.senderAddr = MY_ADDR;
         txBusMsg.msg.devBus.receiverAddr = TARGET_ADDR;
         BusSend(&txBusMsg);
         break;
      }

//      if (ret != BUS_MSG_RXING) {
//         Sleep(1);
//      }

      if (kbhit()) {
         cKb = getch();
      }
   } while (cKb != ESC);


   if (handle != -1) {
      SioClose(handle);
   }
   
   if (spFile == 0) {
      fclose(spFile);
   }

   return 0;
}


/*-----------------------------------------------------------------------------
*  Verarbeitung der Bustelegramme
*/
static void ProcessBus(UINT8 ret) {

   TBusMsgType   msgType;
   TBusTelegramm txBusMsg;
   static int    sRetryCnt = 0;

   if (ret == BUS_MSG_OK) {
      sTimeStamp = GetTickCount();
      msgType = spBusMsg->type;
      switch (sFwuState) {
         case WAIT_FOR_STARTUP_MSG:
            if ((msgType == eBusDevStartup) &&
                (spBusMsg->senderAddr == TARGET_ADDR)) {
               txBusMsg.type = eBusDevReqUpdEnter;
               txBusMsg.senderAddr = MY_ADDR;
               txBusMsg.msg.devBus.receiverAddr = TARGET_ADDR;
               BusSend(&txBusMsg);
               sFwuState = WAIT_FOR_UPD_ENTER_RESP;
               sRetryCnt = 0;
            }
            break;
         case WAIT_FOR_UPD_ENTER_RESP:
            if ((msgType == eBusDevRespUpdEnter) &&
                (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {

               printf("send data (bytes):        ");
               /* erstes Datenpaket senden */
               if (SendDataPacket(true) == false) {
                  /* File hat Länge 0 */
                  sFileTransferComplete = true;
                  printf("\r\nERROR: file is empty\r\n");
               } else {
                  sFwuState = WAIT_FOR_UPD_DATA_RESP;
                  sRetryCnt = 0;
               }
            }
            break;
         case WAIT_FOR_UPD_DATA_RESP:
            if ((msgType == eBusDevRespUpdData) &&
                (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {

               sRetryCnt = 0;

               if (spBusMsg->msg.devBus.x.devResp.updData.wordAddr == sLastWordAddr) {
                  /* Antwort OK -> nächstes Paket */
                  if (SendDataPacket(true) == false) {
                     /* keine Daten mehr zu senden -> Update beenden */
                     txBusMsg.type = eBusDevReqUpdTerm;
                     txBusMsg.senderAddr = MY_ADDR;
                     txBusMsg.msg.devBus.receiverAddr = TARGET_ADDR;
                     BusSend(&txBusMsg);
                     sFwuState = WAIT_FOR_UPD_TERM_RESP;
                  }
               } else {
                  /* Wiederholung des letzten Paketes */
                  SendDataPacket(false);
               }
            }
            break;
         case WAIT_FOR_UPD_TERM_RESP:
            if ((msgType == eBusDevRespUpdTerm) &&
                (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {
               sFileTransferComplete = true;
               printf("\r\ntransfer OK\r\n");
            }
            break;
         default:
            break;
      }
   } else {
      /* keine Antwort erhalten */

      if (sRetryCnt > MAX_RETRY) {
         /* Abbruch */
         printf("\r\ntransfer ERROR\r\n");
         sFileTransferComplete = true;
         return;
      }

      switch (sFwuState) {
         case WAIT_FOR_STARTUP_MSG:
            if ((GetTickCount() - sTimeStamp) > TIMEOUT_MS_STARTUP_RESP) {
               sTimeStamp = GetTickCount();
               sRetryCnt++;
               /* Reboot Command wiederholen */
               txBusMsg.type = eBusDevReqReboot;
               txBusMsg.senderAddr = MY_ADDR;
               txBusMsg.msg.devBus.receiverAddr = TARGET_ADDR;
               BusSend(&txBusMsg);
            }  
            break;
         case WAIT_FOR_UPD_ENTER_RESP:
            if ((GetTickCount() - sTimeStamp) > TIMEOUT_MS_UPD_ENTER_RESP) {
               sTimeStamp = GetTickCount();
               sRetryCnt++;
               /* Upd enter Command wiederholen */
               txBusMsg.type = eBusDevReqUpdEnter;
               txBusMsg.senderAddr = MY_ADDR;
               txBusMsg.msg.devBus.receiverAddr = TARGET_ADDR;
               BusSend(&txBusMsg);
            }
            break;
         case WAIT_FOR_UPD_DATA_RESP:
            if ((GetTickCount() - sTimeStamp) > TIMEOUT_MS_UPD_DATA_RESP) {
               sTimeStamp = GetTickCount();
               sRetryCnt++;
               /* Wiederholung des letzten Datenpaketes */
               SendDataPacket(false);
            }
            break;
         case WAIT_FOR_UPD_TERM_RESP:
            if ((GetTickCount() - sTimeStamp) > TIMEOUT_MS_UPD_TERM_RESP) {
               sTimeStamp = GetTickCount();
               sRetryCnt++;
               /* Upd term wiederholen */
               txBusMsg.type = eBusDevReqUpdTerm;
               txBusMsg.senderAddr = MY_ADDR;
               txBusMsg.msg.devBus.receiverAddr = TARGET_ADDR;
               BusSend(&txBusMsg);
            }
            break;
         default:
            break;
      }
   }
}


/*-----------------------------------------------------------------------------
*  Datenpaket aus File laden und senden
*  der Parameter gibt an, ob das zuletzt gesendete Paket wiederholt werden soll
*/
static bool SendDataPacket(bool next) {

   TBusTelegramm txBusMsg;
   static UINT16 sNextWordAddr = 0;

   txBusMsg.type = eBusDevReqUpdData;
   txBusMsg.senderAddr = MY_ADDR;
   txBusMsg.msg.devBus.receiverAddr = TARGET_ADDR;

   if (next) {
      
/* printf("sendpacket next word addr: %04x\r\n", sNextWordAddr);
*/      
      txBusMsg.msg.devBus.x.devReq.updData.wordAddr = sNextWordAddr;
      sLastWordAddr = sNextWordAddr;
      sNextWordAddr += BUS_FWU_PACKET_SIZE / 2;
   } else {
      /* letztes Paket wiederholen */

/* printf("sendpacket repeat word addr: %04x\r\n", sLastWordAddr);
*/
      txBusMsg.msg.devBus.x.devReq.updData.wordAddr = sLastWordAddr;
      fseek(spFile, sLastWordAddr * 2, SEEK_SET);
   }



   int lenToRead = sizeof(txBusMsg.msg.devBus.x.devReq.updData.data);
   int len = fread(txBusMsg.msg.devBus.x.devReq.updData.data, 1, lenToRead, spFile);

   if (len == 0) {
      /* Fileende - alle Daten sind übertragen */
      return false;
   } else {
      printf("\b\b\b\b\b\b%6d", txBusMsg.msg.devBus.x.devReq.updData.wordAddr * 2 + len); 
      fflush(stdout);

      /* falls Fileende erreicht, ist letztes Paket kann nicht mehr so groß wie Puffer */  
      /* nicht def. Daten werden auf 0xff gesetzt */
      memset((void *)((int)txBusMsg.msg.devBus.x.devReq.updData.data + len), 0xff, lenToRead - len);
      BusSend(&txBusMsg);
      return true;
   }
}   


/*-----------------------------------------------------------------------------
*  Hilfe anzeigen
*/
static void PrintUsage(void) {

   printf("\r\nUsage:");
   printf("firmwarupdate comport filename target\r\n");
   printf("comport: com1 com2 ..\r\n");
   printf("filename: new firmware binary file\r\n");
   printf("target: target address\r\n");
}
