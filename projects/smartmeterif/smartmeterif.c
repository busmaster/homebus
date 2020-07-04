/*
 * smartmeterif.c
 *
 * Copyright 2017 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <stdio.h>

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

#include "sio.h"
#include "bus.h"
#include "led.h"
#include "board.h"

#include "aes.h"

#include "ConfigDescriptor.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
/* offset addresses in EEPROM */
#define MODUL_ADDRESS        0 /* size 1  */
#define KEY_ADDR             1 /* size 16 */

/* 8 bytes non volatile bus variables memory */
#define BUSVAR_NV_START      0x20
#define BUSVAR_NV_END        0x27

#if (BUSVAR_MEMSIZE > (BUSVAR_NV_END - BUSVAR_NV_START + 1))
#error "nvmem size too small"
#endif

/* our bus address */
#define MY_ADDR                    sMyAddr


/* FT232 line status */
#define FT_OE      (1<<1)
#define FT_PE      (1<<2)
#define FT_FE      (1<<3)
#define FT_BI      (1<<4)

#define FT_SIO_SET_BAUDRATE_REQUEST_TYPE    0x40
#define FT_SIO_SET_BAUDRATE_REQUEST         3

#define FT_SIO_SET_DATA_REQUEST             4
#define FT_SIO_SET_DATA_PARITY_EVEN         (0x2 << 8)
#define FT_SIO_SET_DATA_STOP_BITS_1         (0x0 << 11)

#define RECEIVE_TIMEOUT_MS                  2000  /* smartmeter sends every 1000 ms */

#define METER_TELEGRAM_SIZE                 101
#define AES_KEY_LEN                         16
#define AES_IV_LEN                          16

#define SEARCH_ACK   0xe5

/* byte offsets of MBUS */
#define MBUS_ACCESS_NUMBER_OFFS             15
#define MBUS_PAYLOAD_OFFS                   19
#define MBUS_PAYLOAD_SIZE                   80
#define MBUS_CHECKSUM_OFFS                  99

/* checksum range */
#define MBUS_CHECKSUM_START_OFFS            4
#define MBUS_CHECKSUM_END_OFFS              98

/*-----------------------------------------------------------------------------
*  Typedefs
*/
typedef struct {
    uint8_t  genErr;
    uint8_t  commErr;
    uint8_t  outErr;
    uint8_t  enumErr;
    uint8_t  writeStreamErr;
    uint8_t  readStreamErr;
    uint8_t  waitReadyErr;
    uint8_t  rxbufOvrErr;
    uint8_t  attachedCnt;
    uint8_t  unattachedCnt;
    uint8_t  csErr;
    uint8_t  cycCnt;
    uint8_t  rxBuf[128];
    uint8_t  rxIdx;
    uint16_t receiveTimeStamp;
    enum {
        eIfInit,
        eIfWaitForData,
        eIfIdle
    } commState;
    enum {
        eUsbInit            = 0,
        eUsbRun             = 1,
        eUsbInvDev          = 2,
        eUsbUnsupportedDev  = 3,
        eUsbConfigDescErr   = 4,
        eUsbSetDevConfigErr = 5,
        eUsbControlReqErr   = 6,
        eUsbHostErr         = 7,
        eUsbDeviceEnumErr   = 8
    } usbState;
} TIfState;

typedef struct {
    uint32_t countA_plus;
    uint32_t countA_minus;
    uint32_t countR_plus;
    uint32_t countR_minus;
    uint32_t activePower_plus;
    uint32_t activePower_minus;
    uint32_t reactivePower_plus;
    uint32_t reactivePower_minus;
} TMeterData;

static uint8_t sSearchSeq[] = { 0x10, 0x40, 0xf0, 0x30, 0x16 }; /* SND_NKE for 240 */

static unsigned char sKey[AES_KEY_LEN];
/* lower half of iv is the secondary address - it's the same for all EAG meters */
static unsigned char sIv[AES_IV_LEN]  = { 0x2d, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*-----------------------------------------------------------------------------
*  Variables
*/
volatile uint8_t     gTimeMs = 0;
volatile uint16_t    gTimeMs16 = 0;
volatile uint16_t    gTimeS = 0;

static uint8_t       sMyAddr;
static TBusTelegram *spBusMsg;
static TIfState sIfState;

static uint8_t sPayLoad[MBUS_PAYLOAD_SIZE];
static TMeterData sMd;

static char version[] = "smif 0.03";

static TBusTelegram    sTxMsg;
static bool            sTxRetry = false;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void ProcessBus(uint8_t ret);
static void ApplicationCheck(void);

void SetupHardware(void);
static void Host_Task(void);

void EVENT_USB_Host_HostError(const uint8_t ErrorCode);
void EVENT_USB_Host_DeviceAttached(void);
void EVENT_USB_Host_DeviceUnattached(void);
void EVENT_USB_Host_DeviceEnumerationFailed(const uint8_t ErrorCode,
                                            const uint8_t SubErrorCode);
void EVENT_USB_Host_DeviceEnumerationComplete(void);


/*
 * entry
 */
int main(void) {

    static TBusTelegram    sTxMsg;

    SetupHardware();

    LedSet(eLedGreenOff);

    GlobalInterruptEnable();

    sTxMsg.type = eBusDevStartup;
    sTxMsg.senderAddr = MY_ADDR;
    BusSend(&sTxMsg);

    sIfState.commState = eIfInit;
    sIfState.usbState = eUsbInit;

    while (1) {
        Host_Task();

        USB_USBTask();

        ProcessBus(BusCheck());
        LedCheck();
        BusVarProcess();
        ApplicationCheck();
    }
}

/*-----------------------------------------------------------------------------
*  process busvar state change
*/
static void ApplicationCheck(void) {

    static uint8_t sAveEnabledOld = 0;
    static uint16_t sEnabledStartTimeS = 0;
    static TMeterData sMdOld;
    uint8_t aveEnabled;
    uint16_t actualTimeS = 0;
    TBusVarResult result;
    TBusDevReqActualValueEvent *pAve;

    if (BusVarRead(0, &aveEnabled, sizeof(aveEnabled), &result) != sizeof(aveEnabled)) {
        return;
    }
    if (aveEnabled == 0) {
        return;
    }

    if (sAveEnabledOld == 0) {
        GET_TIME_S(sEnabledStartTimeS);
        sAveEnabledOld = 1;
    }

    GET_TIME_S(actualTimeS);
    if ((uint16_t)(actualTimeS - sEnabledStartTimeS) >= 60) {
        aveEnabled = 0;
        sAveEnabledOld = 0;
        BusVarWrite(0, &aveEnabled, sizeof(aveEnabled), &result);
    }

    if (memcmp(&sMdOld, &sMd, sizeof(sMdOld)) != 0) {
        memcpy(&sMdOld, &sMd, sizeof(sMdOld));
        pAve = &sTxMsg.msg.devBus.x.devReq.actualValueEvent;
        sTxMsg.type = eBusDevReqActualValueEvent;
        sTxMsg.senderAddr = MY_ADDR;
        BusVarRead(2, &sTxMsg.msg.devBus.receiverAddr, sizeof(uint8_t), &result);
        pAve->devType = eBusDevTypeSmIf;

        pAve->actualValue.smif.countA_plus         = sMd.countA_plus;
        pAve->actualValue.smif.countA_minus        = sMd.countA_minus;
        pAve->actualValue.smif.countR_plus         = sMd.countR_plus;
        pAve->actualValue.smif.countR_minus        = sMd.countR_minus;
        pAve->actualValue.smif.activePower_plus    = sMd.activePower_plus;
        pAve->actualValue.smif.activePower_minus   = sMd.activePower_minus;
        pAve->actualValue.smif.reactivePower_plus  = sMd.reactivePower_plus;
        pAve->actualValue.smif.reactivePower_minus = sMd.reactivePower_minus;

        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
    }
}

/*-----------------------------------------------------------------------------
*  process received bus telegrams
*/
static void ProcessBus(uint8_t ret) {
    TBusMsgType            msgType;
    bool                   msgForMe = false;
    TBusDevRespInfo        *pInfo;
    TBusDevRespActualValue *pActVal;
    uint8_t                *pData;
    uint8_t                val8;

    if (sTxRetry) {
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        return;
    }

    if (ret == BUS_MSG_OK) {
        msgType = spBusMsg->type;
        switch (msgType) {
        case eBusDevReqReboot:
        case eBusDevReqInfo:
        case eBusDevReqActualValue:
        case eBusDevReqSetAddr:
        case eBusDevReqEepromRead:
        case eBusDevReqEepromWrite:
        case eBusDevReqDiag:
        case eBusDevReqGetVar:
        case eBusDevReqSetVar:
        case eBusDevRespGetVar:
        case eBusDevRespSetVar:
            if (spBusMsg->msg.devBus.receiverAddr == MY_ADDR) {
                msgForMe = true;
            }
            break;
        default:
            break;
        }
    }

    if (msgForMe == false) {
       return;
    }

    switch (msgType) {
    case eBusDevReqReboot:
        /* use watchdog to reboot */
        /* set the watchdog timeout as short as possible (14 ms) */
        cli();
        wdt_enable(WDTO_15MS);
        /* wait for reset */
        while (1);
        break;
    case eBusDevReqInfo:
        /* response packet */
        pInfo = &sTxMsg.msg.devBus.x.devResp.info;
        sTxMsg.type = eBusDevRespInfo;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        pInfo->devType = eBusDevTypeSmIf;
        strncpy((char *)(pInfo->version), version, sizeof(pInfo->version));
        pInfo->version[sizeof(pInfo->version) - 1] = '\0';
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqActualValue:
        /* response packet */
        pActVal = &sTxMsg.msg.devBus.x.devResp.actualValue;
        sTxMsg.type = eBusDevRespActualValue;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        pActVal->devType = eBusDevTypeSmIf;

        pActVal->actualValue.smif.countA_plus         = sMd.countA_plus;
        pActVal->actualValue.smif.countA_minus        = sMd.countA_minus;
        pActVal->actualValue.smif.countR_plus         = sMd.countR_plus;
        pActVal->actualValue.smif.countR_minus        = sMd.countR_minus;
        pActVal->actualValue.smif.activePower_plus    = sMd.activePower_plus;
        pActVal->actualValue.smif.activePower_minus   = sMd.activePower_minus;
        pActVal->actualValue.smif.reactivePower_plus  = sMd.reactivePower_plus;
        pActVal->actualValue.smif.reactivePower_minus = sMd.reactivePower_minus;

        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqSetAddr:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespSetAddr;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        eeprom_write_byte((uint8_t *)MODUL_ADDRESS, spBusMsg->msg.devBus.x.devReq.setAddr.addr);
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqEepromRead:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespEepromRead;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.readEeprom.data =
            eeprom_read_byte((const uint8_t *)spBusMsg->msg.devBus.x.devReq.readEeprom.addr);
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqEepromWrite:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespEepromWrite;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        eeprom_write_byte((uint8_t *)spBusMsg->msg.devBus.x.devReq.readEeprom.addr,
                          spBusMsg->msg.devBus.x.devReq.writeEeprom.data);
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqDiag:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespDiag;
        sTxMsg.msg.devBus.x.devResp.diag.devType = eBusDevTypeSmIf;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        pData = sTxMsg.msg.devBus.x.devResp.diag.data;
        if (sizeof(sTxMsg.msg.devBus.x.devResp.diag.data) > 13) {
            *(pData + 0)  = sIfState.usbState;
            *(pData + 1)  = sIfState.cycCnt;
            *(pData + 2)  = sIfState.genErr;
            *(pData + 3)  = sIfState.commErr;
            *(pData + 4)  = sIfState.outErr;
            *(pData + 5)  = sIfState.enumErr;
            *(pData + 6)  = sIfState.writeStreamErr;
            *(pData + 7)  = sIfState.readStreamErr;
            *(pData + 8)  = sIfState.waitReadyErr;
            *(pData + 9)  = sIfState.rxbufOvrErr;
            *(pData + 10) = sIfState.attachedCnt;
            *(pData + 11) = sIfState.unattachedCnt;
            *(pData + 12) = sIfState.csErr;
        }
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqGetVar:
        val8 = spBusMsg->msg.devBus.x.devReq.getVar.index;
        sTxMsg.msg.devBus.x.devResp.getVar.length =
            BusVarRead(val8, sTxMsg.msg.devBus.x.devResp.getVar.data,
                       sizeof(sTxMsg.msg.devBus.x.devResp.getVar.data),
                       &sTxMsg.msg.devBus.x.devResp.getVar.result);
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespGetVar;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.getVar.index = val8;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqSetVar:
        val8 = spBusMsg->msg.devBus.x.devReq.setVar.index;
        BusVarWrite(val8, spBusMsg->msg.devBus.x.devReq.setVar.data,
                    spBusMsg->msg.devBus.x.devReq.setVar.length,
                    &sTxMsg.msg.devBus.x.devResp.setVar.result);
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespSetVar;
        sTxMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.setVar.index = val8;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevRespSetVar:
        BusVarRespSet(spBusMsg->senderAddr, &spBusMsg->msg.devBus.x.devResp.setVar);
        break;
    case eBusDevRespGetVar:
        BusVarRespGet(spBusMsg->senderAddr, &spBusMsg->msg.devBus.x.devResp.getVar);
        break;
    default:
        break;
    }
}

/*-----------------------------------------------------------------------------
*  port settings
*/
static void PortInit(void) {

    /* PA.x: unused: output low */
    PORTA = 0b00000000;
    DDRA  = 0b11111111;

    /* PB.7: unused: output low */
    /* PB.6: unused: output low */
    /* PB.5: unused: output low */
    /* PB.4: unused: output low */
    /* PB.3, MISO: output low */
    /* PB.2: MOSI: output low */
    /* PB.1: clk: output low */
    /* PB.0: LED: output low */
    PORTB = 0b00000000;
    DDRB  = 0b11111111;

    /* PC.7: unused: output low */
    /* PC.6: unused: output low  */
    /* PC.5: unused: output low */
    /* PC.4: unused: output low */
    /* PC.3: unused: output low */
    /* PC.2: unused: output low */
    /* PC.1: unused: output low */
    /* PC.0: unused: output low */
    /* PC.0: transceiver power, output high */
    PORTC = 0b00000001;
    DDRC  = 0b11111111;

    /* PD.7: unused: output low */
    /* PD.6: unused: output low */
    /* PD.5: unused: output low */
    /* PD.4: unused: output low */
    /* PD.3: TX1: output high */
    /* PD.2: RX1: input pull up */
    /* PD.1: unused: output low */
    /* PD.0: input: high z */
    PORTD = 0b00001100;
    DDRD  = 0b11111010;

    /* PE.7: USB Power: output low */
    /* PE.6: unused: output low */
    /* PE.5: unused: output low */
    /* PE.4: unused: output low */
    /* PE.3: IUID: high z */
    /* PE.2: unused: output low */
    /* PE.1: unused: output low */
    /* PE.0: unused: output low */
    PORTE = 0b00001000;
    DDRE  = 0b11110111;

    /* PF.x: unused: output low */
    PORTF = 0b00000000;
    DDRF  = 0b11111111;
}

/*-----------------------------------------------------------------------------
*  init timer
*/
static void TimerInit(void) {

    /* use timer 3/output compare A */
    /* timer3 compare B is used for sio timing - do not change the timer mode WGM
     * and change sio timer settings when changing the prescaler!
     */

    /* prescaler @ 8.0000 MHz:  1024  */
    /* compare match pin not used: COM3A[1:0] = 00 */
    /* compare register OCR3A:  */
    /* 8.0000 MHz: 39 -> 5 ms */
    /* timer mode 0: normal: WGM3[3:0]= 0000 */

   TCCR3A = (0 << COM3A1) | (0 << COM3A0) | (0 << COM3B1) | (0 << COM3B0) | (0 << WGM31) | (0 << WGM30);
   TCCR3B = (0 << ICNC3) | (0 << ICES3) |
            (0 << WGM33) | (0 << WGM32) |
            (1 << CS32)  | (0 << CS31)  | (1 << CS30);

#if (F_CPU == 8000000UL)
    #define TIMER_TCNT_INC    39
    #define TIMER_INC_MS      5
#else
#error adjust timer settings for your CPU clock frequency
#endif
}

static void TimerStart(void) {

    OCR3A = TCNT3 + TIMER_TCNT_INC;
    TIFR3 = 1 << OCF3A;
    TIMSK3 |= 1 << OCIE3A;
}

/*-----------------------------------------------------------------------------
*  time interrupt for time counter
*/
ISR(TIMER3_COMPA_vect)  {

    static uint16_t sCounter = 0;

    OCR3A = OCR3A + TIMER_TCNT_INC;

    /* ms */
    gTimeMs += TIMER_INC_MS;
    gTimeMs16 += TIMER_INC_MS;
    sCounter++;
    if (sCounter >= (1000 / TIMER_INC_MS)) {
        sCounter = 0;
        /* s */
        gTimeS++;
    }
}

/*-----------------------------------------------------------------------------
*  NV memory for persistent bus variables
*/
static bool BusVarNv(uint16_t address, void *buf, uint8_t bufSize, TBusVarDir dir) {

    void *eeprom;

    // range check
    if ((address + bufSize) > (BUSVAR_NV_END - BUSVAR_NV_START + 1)) {
        return false;
    }

    eeprom = (void *)(BUSVAR_NV_START + address);
    if (dir == eBusVarRead) {
        eeprom_read_block(buf, eeprom, bufSize);
    } else {
        eeprom_update_block(buf, eeprom, bufSize);
    }
    return true;
}

/*
 * configuration
 */
void SetupHardware(void) {
    uint8_t sioHdl;
    uint8_t i;

    /* Disable watchdog if enabled by bootloader/fuses */
    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    /* Disable clock division */
    clock_prescale_set(clock_div_1);

    /* get module address from EEPROM */
    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);

    /* our AES key s stored in eeprom */
    for (i = 0; i < sizeof(sKey); i++) {
        sKey[i] = eeprom_read_byte((const uint8_t *)(KEY_ADDR + i));
    }

    LedInit();

    BusVarInit(sMyAddr, BusVarNv);

    /* bus variables */
    /* enable ActualValueEvent message */
    BusVarAdd(0, sizeof(uint8_t), false);
    /* activation duration of ActaulValueEvent in seconds */
    BusVarAdd(1, sizeof(uint8_t), true);
    /* ActualValueEvent Receiver receiverAddress */
    BusVarAdd(2, sizeof(uint8_t), true);

    SioInit();
    SioRandSeed(sMyAddr);

    /* sio for bus interface */
    sioHdl = SioOpen("USART1", eSioBaud9600, eSioDataBits8, eSioParityNo,
                    eSioStopBits1, eSioModeHalfDuplex);

//    SioSetIdleFunc(sioHdl, IdleSio1);
//    SioSetTransceiverPowerDownFunc(sioHdl, BusTransceiverPowerDown);
//    BusTransceiverPowerDown(true);

    PortInit();

    BUS_TRANSCEIVER_POWER_UP;

    TimerInit();
    TimerStart();

    BusInit(sioHdl);
    spBusMsg = BusMsgBufGet();

    USB_Init();
}
/*
 *  Task to manage an enumerated FT232 smartmeter probe once connected
 */
static void Host_Task(void) {

    struct {
        uint8_t modemStat;
        uint8_t lineStat;
    } statbuf;
    uint16_t rxLen;
    bool     sendReq = false;
    uint8_t  txbuf[2];
    uint8_t  rc;
    uint16_t actualTime16;
    uint8_t  checksum;
    uint8_t  i;

    if (USB_HostState != HOST_STATE_Configured) {
        return;
    }
    GET_TIME_MS16(actualTime16);

    /* Select the data IN pipe */
    Pipe_SelectPipe(FT232_DATA_IN_PIPE);
    Pipe_Unfreeze();

    /* Check to see if a packet has been received */
    if (Pipe_IsINReceived()) {
        /* Check if data is in the pipe */
        if (Pipe_IsReadWriteAllowed()) {
            rxLen = Pipe_BytesInPipe();
            if (rxLen < 2) {
                if (sIfState.genErr < 0xff) {
                    sIfState.genErr++;
                }
                sIfState.rxIdx = 0;
                rxLen = 0;
            } else {
                Pipe_Read_Stream_LE(&statbuf, sizeof(statbuf), 0);
                rxLen -= sizeof(statbuf);
                if ((statbuf.lineStat & (FT_OE | FT_PE | FT_FE | FT_BI)) != 0) {
                    if (sIfState.commErr < 0xff) {
                        sIfState.commErr++;
                    }
                    sIfState.rxIdx = 0;
                    rxLen = 0;
                }
                if (rxLen > (sizeof(sIfState.rxBuf) - sIfState.rxIdx)) {
                    if (sIfState.rxbufOvrErr < 0xff) {
                        sIfState.rxbufOvrErr++;
                    }
                    sIfState.rxIdx = 0;
                    rxLen = 0;
                }
                if (rxLen > 0) {
                    sIfState.receiveTimeStamp = actualTime16;
                    /* Read in the pipe data to the buffer */
                    if (Pipe_Read_Stream_LE(sIfState.rxBuf + sIfState.rxIdx, rxLen, 0) == PIPE_RWSTREAM_NoError) {
                        sIfState.rxIdx += rxLen;
                    } else {
                        if (sIfState.readStreamErr < 0xff) {
                            sIfState.readStreamErr++;
                        }
                    }
                } else {
                    if (((uint16_t)(actualTime16 - sIfState.receiveTimeStamp)) >= RECEIVE_TIMEOUT_MS) {
                        sIfState.commState = eIfInit;
                        sIfState.rxIdx = 0;
                    }
                }
            }
            if (rxLen > 0) {
                switch (sIfState.commState) {
                case eIfInit:
                    if (sIfState.rxIdx >= sizeof(sSearchSeq)) {
                        if (memcmp(sSearchSeq, sIfState.rxBuf, sizeof(sSearchSeq)) == 0) {
                            sIfState.commState = eIfWaitForData;
                            sendReq = true;
                        }
                        sIfState.rxIdx = 0;
                    }
                    break;
                case eIfWaitForData:
                    if (sIfState.rxIdx >= METER_TELEGRAM_SIZE) {
                        LedSet(eLedGreenBlinkOnceShort);
                        checksum = 0;
                        for (i = MBUS_CHECKSUM_START_OFFS; i <= MBUS_CHECKSUM_END_OFFS; i++) {
                            checksum += sIfState.rxBuf[i];
                        }
                        if (checksum != sIfState.rxBuf[MBUS_CHECKSUM_OFFS]) {
                            if (sIfState.csErr < 0xff) {
                                sIfState.csErr++;
                            }
                        } else {
                            /* set upper half of iv */
                            for (i = 8; i < 16; i++) {
                                sIv[i] = sIfState.rxBuf[MBUS_ACCESS_NUMBER_OFFS];
                            }
                            AES128_CBC_decrypt_buffer(sPayLoad, &sIfState.rxBuf[MBUS_PAYLOAD_OFFS], sizeof(sPayLoad), sKey, sIv);

                            sMd.activePower_plus  = (uint32_t)sPayLoad[44]         |
                                                    ((uint32_t)sPayLoad[45] << 8)  |
                                                    ((uint32_t)sPayLoad[46] << 16) |
                                                    ((uint32_t)sPayLoad[47] << 24);
                            sMd.activePower_minus = (uint32_t)sPayLoad[51]         |
                                                    ((uint32_t)sPayLoad[52] << 8)  |
                                                    ((uint32_t)sPayLoad[53] << 16) |
                                                    ((uint32_t)sPayLoad[54] << 24);
                            sMd.reactivePower_plus  = (uint32_t)sPayLoad[58]         |
                                                      ((uint32_t)sPayLoad[59] << 8)  |
                                                      ((uint32_t)sPayLoad[60] << 16) |
                                                      ((uint32_t)sPayLoad[61] << 24);
                            sMd.reactivePower_minus = (uint32_t)sPayLoad[66]         |
                                                      ((uint32_t)sPayLoad[67] << 8)  |
                                                      ((uint32_t)sPayLoad[68] << 16) |
                                                      ((uint32_t)sPayLoad[69] << 24);
                            sMd.countA_plus  = (uint32_t)sPayLoad[12]         |
                                               ((uint32_t)sPayLoad[13] << 8)  |
                                               ((uint32_t)sPayLoad[14] << 16) |
                                               ((uint32_t)sPayLoad[15] << 24);
                            sMd.countA_minus = (uint32_t)sPayLoad[19]         |
                                               ((uint32_t)sPayLoad[20] << 8)  |
                                               ((uint32_t)sPayLoad[21] << 16) |
                                               ((uint32_t)sPayLoad[22] << 24);
                            sMd.countR_plus  = (uint32_t)sPayLoad[28]         |
                                               ((uint32_t)sPayLoad[29] << 8)  |
                                               ((uint32_t)sPayLoad[30] << 16) |
                                               ((uint32_t)sPayLoad[31] << 24);
                            sMd.countR_minus = (uint32_t)sPayLoad[38]         |
                                               ((uint32_t)sPayLoad[39] << 8)  |
                                               ((uint32_t)sPayLoad[40] << 16) |
                                               ((uint32_t)sPayLoad[41] << 24);
                        }
                        sIfState.rxIdx = 0;
                        sendReq = true;
                        sIfState.cycCnt++;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        /* Clear the pipe after it is read, ready for the next packet */
        Pipe_ClearIN();
    }
    /* Re-freeze IN pipe after use */
    Pipe_Freeze();

    if (sendReq) {
        /* Select the data IN pipe */
        Pipe_SelectPipe(FT232_DATA_OUT_PIPE);
        Pipe_Unfreeze();

        if (Pipe_IsOUTReady()) {
            txbuf[0] = 0x05;       /* linestat: len = 1 */
            txbuf[1] = SEARCH_ACK;

            rc = Pipe_Write_Stream_LE(txbuf, sizeof(txbuf), 0);
            if (rc != PIPE_RWSTREAM_NoError) {
                if (sIfState.writeStreamErr < 0xff) {
                    sIfState.writeStreamErr++;
                }
            }
            /* Send the data in the pipe to the device */
            Pipe_ClearOUT();
           	rc = Pipe_WaitUntilReady();
            if (rc != PIPE_READYWAIT_NoError) {
                if (sIfState.waitReadyErr < 0xff) {
                    sIfState.waitReadyErr++;
                }
        }
        } else {
            if (sIfState.outErr < 0xff) {
                sIfState.outErr++;
            }
        }
        Pipe_Freeze();
    }
}

/*
 * device attached
 */
void EVENT_USB_Host_DeviceAttached(void) {

    if (sIfState.attachedCnt < 0xff) {
        sIfState.attachedCnt++;
    }
}

/*
 * device unattached
 */
void EVENT_USB_Host_DeviceUnattached(void) {

    if (sIfState.unattachedCnt < 0xff) {
        sIfState.unattachedCnt++;
    }
}

/*
 *  Event handler for the USB_DeviceEnumerationComplete event.
 */
void EVENT_USB_Host_DeviceEnumerationComplete(void) {

    USB_Descriptor_Device_t devDesc;

    if (USB_Host_GetDeviceDescriptor(&devDesc) != HOST_SENDCONTROL_Successful) {
        sIfState.usbState = eUsbInvDev;
        return;
    }

    if ((devDesc.Header.Size != 0x12) ||
        (devDesc.VendorID != 0x0403)  ||
        (devDesc.ProductID != 0x6001)) {
        sIfState.usbState = eUsbUnsupportedDev;
        return;
    }
    /* Get and process the configuration descriptor data */
    if (ProcessConfigDescriptor() != SuccessfulConfigRead) {
        sIfState.usbState = eUsbConfigDescErr;
        return;
    }

    /* Set the device configuration to the first configuration (rarely do devices use multiple configurations) */
    if (USB_Host_SetDeviceConfiguration(1) != HOST_SENDCONTROL_Successful) {
        sIfState.usbState = eUsbSetDevConfigErr;
        return;
    }
    USB_ControlRequest = (USB_Request_Header_t) {
        .bmRequestType = FT_SIO_SET_BAUDRATE_REQUEST_TYPE,
        .bRequest      = FT_SIO_SET_BAUDRATE_REQUEST,
        .wValue        = /* divisor for 9600 */0x4138, /* /drivers/usb/serial/ftdi_sio.c change_speed ->           */
                                                       /* get_ftdi_divisor -> ftdi_232bm_baud_to_divisor (FT232RL) */
        .wIndex        = 0,
        .wLength       = 0,
    };

    /* Set the Line Encoding of the interface within the device, so that it is ready to accept data */
    Pipe_SelectPipe(PIPE_CONTROLPIPE);
    if (USB_Host_SendControlRequest(0) != HOST_SENDCONTROL_Successful) {
        sIfState.usbState = eUsbControlReqErr;
        return;
    }

    USB_ControlRequest = (USB_Request_Header_t) {
        .bmRequestType = FT_SIO_SET_BAUDRATE_REQUEST_TYPE,
        .bRequest      = FT_SIO_SET_DATA_REQUEST,
        .wValue        = FT_SIO_SET_DATA_PARITY_EVEN | FT_SIO_SET_DATA_STOP_BITS_1 | 8,
        .wIndex        = 0,
        .wLength       = 0,
    };

    /* Set the Line Encoding of the interface within the device, so that it is ready to accept data */
    Pipe_SelectPipe(PIPE_CONTROLPIPE);
    if (USB_Host_SendControlRequest(0) != HOST_SENDCONTROL_Successful) {
        sIfState.usbState = eUsbControlReqErr;
        return;
    }

    sIfState.commState = eIfInit;
    sIfState.usbState = eUsbInit;

    sIfState.rxIdx = 0;
}

/*
 *  hardware error
 */
void EVENT_USB_Host_HostError(const uint8_t ErrorCode)
{
    USB_Disable();

    sIfState.usbState = eUsbHostErr;
}

/*
 *  Enumeration failed
 */
void EVENT_USB_Host_DeviceEnumerationFailed(const uint8_t ErrorCode,
                                            const uint8_t SubErrorCode) {
    sIfState.usbState = eUsbDeviceEnumErr;
}

/*
 * dev desc
 * 12 01 00 02 00 00 00 08 03 04 01 60 00 06 01 02
 * 03 01
*/
