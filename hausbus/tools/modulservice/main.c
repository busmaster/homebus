/*-----------------------------------------------------------------------------
*  main.c
*/

/*-----------------------------------------------------------------------------
*  Includes
*/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef WIN32
#include <conio.h>
#include <windows.h>
#endif 

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/

#define SIZE_COMPORT   100

/* eigene Adresse am Bus */
#define MY_ADDR    0

/* default timeout for receiption of response telegram */
#define RESPONSE_TIMEOUT   1000 /* ms */  

#define OP_INVALID                          0
#define OP_SET_NEW_ADDRESS                  1
#define OP_SET_CLIENT_ADDRESS_LIST          2
#define OP_GET_CLIENT_ADDRESS_LIST          3
#define OP_RESET                            4
#define OP_EEPROM_READ                      5
#define OP_EEPROM_WRITE                     6

#define SIZE_CLIENT_LIST                    BUS_MAX_CLIENT_NUM

#define SIZE_EEPROM_BUF                     100

/*-----------------------------------------------------------------------------
*  Typedefs
*/

/*-----------------------------------------------------------------------------
*  Variables
*/

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PrintUsage(void);
static bool BusModulReset(uint8_t address);  
static bool BusModulNewAddress(uint8_t address, uint8_t newAddress);  
static bool BusModulReadClientAddress(uint8_t address, uint8_t *pList, uint8_t listLen);  
static bool BusModulSetClientAddress(uint8_t address, uint8_t *pList, uint8_t listLen);  
static bool BusModulReadEeprom(uint8_t address, uint8_t *pBuf, unsigned int bufLen, unsigned int eepromAddress);
static bool BusModulWriteEeprom(uint8_t address, uint8_t *pBuf, unsigned int bufLen, unsigned int eepromAddress);

#ifndef WIN32
static unsigned long GetTickCount(void);
#endif
/*-----------------------------------------------------------------------------
*  program start
*/
int main(int argc, char *argv[]) {

   int            handle;
   int   i;   
   int   j;
   int   k;
   char           comPort[SIZE_COMPORT] = "";
   uint8_t          operation = OP_INVALID;
   uint8_t          clientList[SIZE_CLIENT_LIST];
   uint8_t          eepromData[SIZE_EEPROM_BUF];
   uint8_t          moduleAddr;
   uint8_t          newModuleAddr;
   unsigned int   eepromAddress;
   unsigned int   eepromLength;
   uint8_t          *pBuf;
   bool           ret = false;

   /* get com interface */
   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-c") == 0) {
         if (argc > i) { 
            strncpy(comPort, argv[i + 1], sizeof(comPort) - 1);
            comPort[sizeof(comPort) - 1] = '\0';
         }
         break;
      } 
   }
   if (strlen(comPort) == 0) {
      PrintUsage();
      return 0;
   }

   /* get module address */
   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-a") == 0) {
         if (argc > i) {
            moduleAddr = atoi(argv[i + 1]);
         }
         break;
      } 
   }
   
   /* get requestet operation */
   
   /* set new module address */
   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-na") == 0) {
         if (argc > i) {
            operation = OP_SET_NEW_ADDRESS;
            newModuleAddr = atoi(argv[i + 1]);
         }
         break;
      } 
   }
   
   /* set client address list */
   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-setcl") == 0) { 
         memset(clientList, 0xff, sizeof(clientList));
         operation = OP_SET_CLIENT_ADDRESS_LIST;
         for (j = i + 1, k = 0; (j < argc) && (k < (int)sizeof(clientList)); j++, k++) {
            clientList[k] = atoi(argv[j]);
         }
         break;
      } 
   }   
       
   /* get client address list  */
   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-getcl") == 0) {
         operation = OP_GET_CLIENT_ADDRESS_LIST;
         break;
      } 
   }

   /* reset module */
   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-reset") == 0) {
         operation = OP_RESET;
         break;
      } 
   }
   
   /* read EEPROM */
   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-eerd") == 0) { 
         if (argc > i) {
            eepromAddress = atoi(argv[i + 1]);
            if (argc > (i + 1)) {
               eepromLength = atoi(argv[i + 2]);
               operation = OP_EEPROM_READ;
            }
         }
         break;
      } 
   }   
   
   /* write EEPROM */
   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-eewr") == 0) { 
         if (argc > i) {
            eepromAddress = atoi(argv[i + 1]);
            operation = OP_EEPROM_WRITE;
            eepromLength = 0;
            for (j = i + 2, k = 0; (j < argc) && (k < (int)sizeof(eepromData)); j++, k++) {
               eepromData[k] = atoi(argv[j]);
               eepromLength++;
            }
            break;
         }
      } 
   }   
     
   SioInit();

   handle = SioOpen(comPort, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);

   if (handle == -1) {
      printf("cannot open %s\r\n", comPort);
      return 0;
   }
   uint8_t len;
   while ((len = SioGetNumRxChar(handle)) != 0) {
      uint8_t rxBuf[255];
      SioRead(handle, rxBuf, len);      
   }
   BusInit(handle);

   switch (operation) {
      case OP_SET_NEW_ADDRESS:
         ret = BusModulNewAddress(moduleAddr, newModuleAddr);
         if (ret) {
            printf("OK\r\n");   
         }
         break;
      case OP_SET_CLIENT_ADDRESS_LIST:
         ret = BusModulSetClientAddress(moduleAddr, clientList, sizeof(clientList));  
         if (ret) {
            printf("OK\r\n");   
         }
         break;  
      case OP_GET_CLIENT_ADDRESS_LIST:
         ret = BusModulReadClientAddress(moduleAddr, clientList, sizeof(clientList));  
         if (ret) {
            printf("client list: ");
            for (i = 0; i < (int)sizeof(clientList); i++) {
               printf("%02x ", clientList[i]);
            } 
            printf("\r\n");
         } 
         break;
      case OP_RESET:
         ret = BusModulReset(moduleAddr);
         if (ret) {
            printf("OK\r\n");   
         }
         break;
      case OP_EEPROM_READ:
         pBuf = (uint8_t *)malloc(eepromLength);
         if (pBuf != 0) {
            ret = BusModulReadEeprom(moduleAddr, pBuf, eepromLength, eepromAddress);
            if (ret) {
               eepromLength += eepromAddress % 16;
               printf("eeprom dump:");
               for (i = 0; i < (int)eepromLength; i++) {
                  if ((i % 16) == 0) {
                     printf("\r\n%04x: ", eepromAddress + i - eepromAddress % 16);
                  }
                  if (i < ((int)eepromAddress % 16)) {
                     printf("   ");
                  } else {
                     printf("%02x ", *(pBuf + i - eepromAddress % 16)); 
                  }
               }
               printf("\r\n");
            }
            free(pBuf);
         } else {
            printf("cannot allocate memory for command execution\r\n");       
         }
         break;
      case OP_EEPROM_WRITE:
         ret = BusModulWriteEeprom(moduleAddr, eepromData, eepromLength, eepromAddress);
         if (ret) {
            printf("OK\r\n");
         }
         break;
      default:
         break;
   }
   if (handle != -1) {
      SioClose(handle);
   }
   if (ret) {
      return 0;       
   } else {
      printf("ERROR\r\n");
      return -1;       
   }
}


/*-----------------------------------------------------------------------------
*  bus module reset
*/
static bool BusModulReset(uint8_t address) {

   TBusTelegramm   txBusMsg;
   uint8_t           ret;  
   unsigned long   startTimeMs;   
   unsigned long   actualTimeMs;
   TBusTelegramm   *pBusMsg;
   bool            responseOk = false;
   bool            timeOut = false;
  
   txBusMsg.type = eBusDevReqReboot;
   txBusMsg.senderAddr = MY_ADDR;
   txBusMsg.msg.devBus.receiverAddr = address;
   BusSend(&txBusMsg);
   startTimeMs = GetTickCount();
   do {
      actualTimeMs = GetTickCount();
      ret = BusCheck();
      if (ret == BUS_MSG_OK) {
         pBusMsg = BusMsgBufGet();
         if ((pBusMsg->type == eBusDevStartup) &&
             (pBusMsg->senderAddr == address)) {
            responseOk = true;
         }
      } else {
         if ((actualTimeMs - startTimeMs) > RESPONSE_TIMEOUT) {
            timeOut = true;
         }
      }
   } while (!responseOk && !timeOut);
   
   if (responseOk) {
      return true;
   } else {
      return false;    
   }
}

/*-----------------------------------------------------------------------------
*  set new bus module address
*/
static bool BusModulNewAddress(uint8_t address, uint8_t newAddress) {

   TBusTelegramm   txBusMsg;
   uint8_t           ret;  
   unsigned long   startTimeMs;   
   unsigned long   actualTimeMs;
   TBusTelegramm   *pBusMsg;
   bool            responseOk = false;
   bool            timeOut = false;

   
   txBusMsg.type = eBusDevReqSetAddr;
   txBusMsg.senderAddr = MY_ADDR;
   txBusMsg.msg.devBus.receiverAddr = address;
   txBusMsg.msg.devBus.x.devReq.setAddr.addr = newAddress;
   BusSend(&txBusMsg);
   startTimeMs = GetTickCount();
   do {
      actualTimeMs = GetTickCount();
      ret = BusCheck();
      if (ret == BUS_MSG_OK) {
         pBusMsg = BusMsgBufGet();
         if ((pBusMsg->type == eBusDevRespSetAddr)         &&
             (pBusMsg->msg.devBus.receiverAddr == MY_ADDR) &&
             (pBusMsg->senderAddr == address)) {
            responseOk = true;
         }
      } else {
         if ((actualTimeMs - startTimeMs) > RESPONSE_TIMEOUT) {
            timeOut = true;
         }
      }
   } while (!responseOk && !timeOut);
 
   if (responseOk) {
      return true;
   } else {
      return false;    
   }
}

/*-----------------------------------------------------------------------------
*  read client address list from bus module
*/
static bool BusModulReadClientAddress(uint8_t address, uint8_t *pList, uint8_t listLen) {

   TBusTelegramm   txBusMsg;
   uint8_t         ret;   
   unsigned long   startTimeMs;   
   unsigned long   actualTimeMs;
   TBusTelegramm   *pBusMsg;
   bool            responseOk = false;
   bool            timeOut = false;
   int             i;

   txBusMsg.type = eBusDevReqGetClientAddr;
   txBusMsg.senderAddr = MY_ADDR;
   txBusMsg.msg.devBus.receiverAddr = address;
   BusSend(&txBusMsg);
   startTimeMs = GetTickCount();
   do {
      actualTimeMs = GetTickCount();
      ret = BusCheck();
      if (ret == BUS_MSG_OK) {
         pBusMsg = BusMsgBufGet();
         if ((pBusMsg->type == eBusDevRespGetClientAddr)   &&
             (pBusMsg->msg.devBus.receiverAddr == MY_ADDR) &&
             (pBusMsg->senderAddr == address)) {
            responseOk = true;
            for (i = 0; (i < BUS_MAX_CLIENT_NUM) && (i < listLen); i++) {
               *pList = pBusMsg->msg.devBus.x.devResp.getClientAddr.clientAddr[i];
               pList++;
            }
         }
      } else {
         if ((actualTimeMs - startTimeMs) > RESPONSE_TIMEOUT) {
            timeOut = true;
         }
      }
   } while (!responseOk && !timeOut);
   
   if (responseOk) {
      return true;
   } else {
      return false;    
   }
}

/*-----------------------------------------------------------------------------
*  set new client address list
*/
static bool BusModulSetClientAddress(uint8_t address, uint8_t *pList, uint8_t listLen) {
       
   TBusTelegramm   txBusMsg;
   uint8_t         ret;   
   unsigned long   startTimeMs;   
   unsigned long   actualTimeMs;
   TBusTelegramm   *pBusMsg;
   bool            responseOk = false;
   bool            timeOut = false;
   int             i;
          
   txBusMsg.type = eBusDevReqSetClientAddr;
   txBusMsg.senderAddr = MY_ADDR;
   txBusMsg.msg.devBus.receiverAddr = address;
   memset(txBusMsg.msg.devBus.x.devReq.setClientAddr.clientAddr, 0xff, BUS_MAX_CLIENT_NUM);
   for (i = 0; i < listLen; i++) {
      txBusMsg.msg.devBus.x.devReq.setClientAddr.clientAddr[i] = pList[i];
   }
   BusSend(&txBusMsg);
   startTimeMs = GetTickCount();
   do {
      actualTimeMs = GetTickCount();
      ret = BusCheck();
      if (ret == BUS_MSG_OK) {
         pBusMsg = BusMsgBufGet();
         if ((pBusMsg->type == eBusDevRespSetClientAddr)   &&
             (pBusMsg->msg.devBus.receiverAddr == MY_ADDR) &&
             (pBusMsg->senderAddr == address)) {
            responseOk = true;
         }
      } else {
         if ((actualTimeMs - startTimeMs) > RESPONSE_TIMEOUT) {
            timeOut = true;
         }
      }
   } while (!responseOk && !timeOut);
 
   if (responseOk) {
      return true;
   } else {
      return false;    
   }
}

/*-----------------------------------------------------------------------------
*  read eeprom data from bus module
*/
static bool BusModulReadEeprom(
   uint8_t address, 
   uint8_t *pBuf, 
   unsigned int bufLen,
   unsigned int eepromAddress) {

   TBusTelegramm   txBusMsg;
   uint8_t           ret;   
   unsigned long   startTimeMs;   
   unsigned long   actualTimeMs;
   TBusTelegramm   *pBusMsg;
   bool            responseOk = false;
   bool            timeOut = false;
   unsigned int    currentAddress = eepromAddress;
   unsigned int    numRead = 0;

   txBusMsg.type = eBusDevReqEepromRead;
   txBusMsg.senderAddr = MY_ADDR;
   txBusMsg.msg.devBus.receiverAddr = address;

   while (numRead < bufLen) {
      txBusMsg.msg.devBus.x.devReq.readEeprom.addr = (uint16_t)currentAddress;
      BusSend(&txBusMsg);
      startTimeMs = GetTickCount();
      do {
         actualTimeMs = GetTickCount();
         ret = BusCheck();
         if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespEepromRead) &&
                (pBusMsg->msg.devBus.receiverAddr == MY_ADDR) &&
                (pBusMsg->senderAddr == address)) {
               *pBuf = pBusMsg->msg.devBus.x.devResp.readEeprom.data; 
               numRead++;
               currentAddress++;
               pBuf++;                                 
               responseOk = true;
            }
         } else {
            if ((actualTimeMs - startTimeMs) > RESPONSE_TIMEOUT) {
               timeOut = true;
            }
         }
      } while (!responseOk && !timeOut);
      if (!responseOk) {
         break;
      } 
      responseOk = false;
   }
   
   if (numRead == bufLen) {
      return true;
   } else {
      return false;
   }
}

/*-----------------------------------------------------------------------------
*  write eeprom data from bus module
*/
static bool BusModulWriteEeprom(
   uint8_t address, 
   uint8_t *pBuf, 
   unsigned int bufLen,
   unsigned int eepromAddress) {

   TBusTelegramm   txBusMsg;
   uint8_t           ret;   
   unsigned long   startTimeMs;   
   unsigned long   actualTimeMs;
   TBusTelegramm   *pBusMsg;
   bool            responseOk = false;
   bool            timeOut = false;
   unsigned int    currentAddress = eepromAddress;
   unsigned int    numWritten = 0;

   txBusMsg.type = eBusDevReqEepromWrite;
   txBusMsg.senderAddr = MY_ADDR;
   txBusMsg.msg.devBus.receiverAddr = address;

   while (numWritten < bufLen) {
      txBusMsg.msg.devBus.x.devReq.writeEeprom.addr = (uint16_t)currentAddress;
      txBusMsg.msg.devBus.x.devReq.writeEeprom.data = *pBuf;
      BusSend(&txBusMsg);
      startTimeMs = GetTickCount();
      do {
         actualTimeMs = GetTickCount();
         ret = BusCheck();
         if (ret == BUS_MSG_OK) {
            pBusMsg = BusMsgBufGet();
            if ((pBusMsg->type == eBusDevRespEepromWrite) &&
                (pBusMsg->msg.devBus.receiverAddr == MY_ADDR) &&
                (pBusMsg->senderAddr == address)) {
               numWritten++;
               currentAddress++;
               pBuf++;                                 
               responseOk = true;
            }
         } else {
            if ((actualTimeMs - startTimeMs) > RESPONSE_TIMEOUT) {
               timeOut = true;
            }
         }
      } while (!responseOk && !timeOut);
      if (!responseOk) {
         break;
      } 
      responseOk = false;
   }
   
   if (numWritten == bufLen) {
      return true;
   } else {
      return false;
   }
}

#ifndef WIN32
static unsigned long GetTickCount(void) {

   struct timespec ts;
   unsigned long timeMs;

   clock_gettime(CLOCK_MONOTONIC, &ts);
   timeMs = (unsigned long)((unsigned long long)ts.tv_sec * 1000ULL + (unsigned long long)ts.tv_nsec / 1000000ULL);
   
   return timeMs;
}
#endif

/*-----------------------------------------------------------------------------
*  show help
*/
static void PrintUsage(void) {

   printf("\r\nUsage:\r\n");
   printf("busmodulservice -c port -a addr (-na addr                       |\r\n");
   printf("                                 -setcl addr1 .. addr16         |\r\n");
   printf("                                 -getcl                         |\r\n");
   printf("                                 -reset                         |\r\n");
   printf("                                 -eerd addr len                 |\r\n");
   printf("                                 -eewr addr data1 .. dataN)      \r\n");
   printf("-c port: com1 com2 ..\r\n");
   printf("-a addr: addr = address of module\r\n");
   printf("-na addr: set new address, addr = new address\r\n");
   printf("-setcl addr1 .. addr16 : set client address list, addr1 = 1st client's address\r\n");
   printf("-getcl: show client address list\r\n");
   printf("-reset: reset module\r\n");
   printf("-eerd addr len: read EEPROM data from offset addr with number len\r\n");
   printf("-eewr addr data1 .. dataN: write EEPROM data from offset\r\n");
}

