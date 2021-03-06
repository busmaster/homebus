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

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#include "sio.h"
#include "sysdef.h"
#include "board.h"
#include "bus.h"
#include "led.h"
#include "flash.h"

/*-----------------------------------------------------------------------------
*  AT90USB647 fuse bit settings (8 MHz crystal)
*
*  HWBE      = 1
*  BODLEVEL2 = 0
*  BODLEVEL1 = 0
*  BODLEVEL0 = 1  BOD 3.5 V (voltage dip at usb power on)
*
*  OCDEN     = 1
*  JTAGEN    = 1
*  SPIEN     = 0
*  WDTON     = 1
*  EESAVE    = 0
*  BOOTSZ1   = 0
*  BOOTSZ0   = 0
*  BOOTRST   = 0
*
*  CKDIV8    = 1
*  CKOUT     = 1
*  SUT1      = 0
*  SUT0      = 1
*  CKSEL3    = 1
*  CKSEL2    = 1
*  CKSEL1    = 1
*  CKSEL0    = 1
*
*  Extended Fuse Byte: F9
*  Fuse High Byte:     D0
*  Fuse Low Byte:      DF
*/

/*-----------------------------------------------------------------------------
*  Macros
*/
/* own address */
#define MY_ADDR                     sMyAddr

/* the module address is stored in the first byte of eeprom */
#define MODUL_ADDRESS               0

/* statemachine for flash update */
#define WAIT_FOR_UPD_ENTER          0
#define WAIT_FOR_UPD_ENTER_TIMEOUT  1
#define WAIT_FOR_UPD_DATA           2

/* max size of application */
#define MAX_FIRMWARE_SIZE           (56UL * 1024UL)
#define FLASH_CHECKSUM              0x1234  /* result of checksum calculation */
#define CHECKSUM_BLOCK_SIZE         256     /* num words */

#define IDLE_SIO0                   0x01

/*-----------------------------------------------------------------------------
*  Variables
*/
volatile uint8_t     gTimeMs = 0;
volatile uint16_t    gTimeMs16 = 0;
volatile uint16_t    gTimeS = 0;

static TBusTelegram  *spBusMsg;
static TBusTelegram  sTxBusMsg;
static uint8_t       sFwuState = WAIT_FOR_UPD_ENTER_TIMEOUT;
static uint8_t       sMyAddr;
static uint8_t       sIdle = 0;

/* word addresses 0x7f80 .. 0x7ffe reseverd in flash for the version string */
char version[] __attribute__ ((section (".version")));
char version[] = "smartmeterif_bootloader_0.01";

/*-----------------------------------------------------------------------------
*  Functions
*/
extern void ApplicationEntry(void);

static void PortInit(void);
static void TimerInit(void);
static void TimerStart(void);
static void TimerExit(void);
static void ProcessBus(uint8_t ret);
static uint8_t CheckTimeout(void);
static void Idle(void);
static void IdleSio0(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {

    uint8_t   ret;
    uint16_t  flashWordAddr;
    uint16_t  sum;
    int       sioHdl;

    cli();
    MCUSR = 0;
    wdt_disable();

    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);

    PortInit();
    TimerInit();
    LedInit();
    FlashInit();

    SioInit();

    /* sio for bus interface */
    sioHdl = SioOpen("USART0", eSioBaud9600, eSioDataBits8, eSioParityNo,
                     eSioStopBits1, eSioModeHalfDuplex);

    SioSetIdleFunc(sioHdl, IdleSio0);
    SioSetTransceiverPowerDownFunc(sioHdl, BusTransceiverPowerDown);

    BusTransceiverPowerDown(true);

    BusInit(sioHdl);
    spBusMsg = BusMsgBufGet();

    /* switch vector table to bootloader */
    MCUCR = (1 << IVCE);
    MCUCR = (1 << IVSEL);

    LedSet(eLedGreenOff);

    /* calculate application checksum */
    sum = 0;
    for (flashWordAddr = 0; flashWordAddr < (MAX_FIRMWARE_SIZE / 2); flashWordAddr += CHECKSUM_BLOCK_SIZE) {
        sum += FlashSum(flashWordAddr, (uint8_t)CHECKSUM_BLOCK_SIZE);
    }

    if (sum == FLASH_CHECKSUM) {
        /* OK */
        LedSet(eLedGreenFlashFast);
    } else {
        /* Fehler */
        sFwuState = WAIT_FOR_UPD_ENTER;
        LedSet(eLedGreenFlashSlow);
    }

    ENABLE_INT;

    TimerStart();

    /* send startup message */
    sTxBusMsg.type = eBusDevStartup;
    sTxBusMsg.senderAddr = MY_ADDR;
    BusSend(&sTxBusMsg);

    while (1) {
        Idle();
        ret = BusCheck();
        ProcessBus(ret);
        LedCheck();
        /* wait for update request */
        if (sFwuState == WAIT_FOR_UPD_ENTER_TIMEOUT) {
            ret = CheckTimeout();
            if (ret != 0) {
               /* start application */
               break;
            }
        }
     }

    /* switch back the vector table */
    MCUCR = (1 << IVCE);
    MCUCR = (0 << IVSEL);

    cli();

    TimerExit();
    SioExit();
    LedSet(eLedGreenOff);

    /* jum to application */
    ApplicationEntry();
    /* never returns */
    return 0;
}

/*-----------------------------------------------------------------------------
*  timeout for update entry
*/
static uint8_t CheckTimeout(void) {

    uint16_t actTimeS;

    GET_TIME_S(actTimeS);

    if (actTimeS < 4) {
        return 0;
    } else {
        return 1;
    }
}

/*-----------------------------------------------------------------------------
*  process bus telegrams
*/
static void ProcessBus(uint8_t ret) {

    TBusMsgType   msgType;
    uint16_t        *pData;
    uint16_t        wordAddr;
    bool          rc;

    if (ret == BUS_MSG_OK) {
        msgType = spBusMsg->type;
        if ((msgType == eBusDevReqReboot) &&
            (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {
            /* reboot using the watchdog */
            cli();
            wdt_enable(WDTO_15MS);
            /* wait for reset */
            while (1);
        } else {
            switch (sFwuState) {
            case WAIT_FOR_UPD_ENTER_TIMEOUT:
            case WAIT_FOR_UPD_ENTER:
                if ((msgType == eBusDevReqUpdEnter) &&
                    (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {
                    /* erase application */
                    FlashErase();
                    /* send response */
                    sTxBusMsg.type = eBusDevRespUpdEnter;
                    sTxBusMsg.senderAddr = MY_ADDR;
                    sTxBusMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
                    BusSend(&sTxBusMsg);
                    sFwuState = WAIT_FOR_UPD_DATA;
                    LedSet(eLedRedFlashFast);
                    LedSet(eLedGreenFlashFast);
                }
                break;
            case WAIT_FOR_UPD_DATA:
                if ((msgType == eBusDevReqUpdData) &&
                    (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {
                    wordAddr = spBusMsg->msg.devBus.x.devReq.updData.wordAddr;
                    pData = spBusMsg->msg.devBus.x.devReq.updData.data;
                    /* program to flash */
                    rc = FlashProgram(wordAddr, pData, sizeof(spBusMsg->msg.devBus.x.devReq.updData.data)/2);
                    /* send response */
                    sTxBusMsg.type = eBusDevRespUpdData;
                    sTxBusMsg.senderAddr = MY_ADDR;
                    sTxBusMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
                    if (rc == true) {
                        /* programming OK: return wordAddr */
                        sTxBusMsg.msg.devBus.x.devResp.updData.wordAddr = wordAddr;
                    } else {
                        /* programming error: return -1 */
                        sTxBusMsg.msg.devBus.x.devResp.updData.wordAddr = -1;
                    }
                    BusSend(&sTxBusMsg);
                 } else if ((msgType == eBusDevReqUpdTerm) &&
                            (spBusMsg->msg.devBus.receiverAddr == MY_ADDR)) {
                    /* terminate programming */
                    rc = FlashProgramTerminate();
                    /* send response */
                    sTxBusMsg.type = eBusDevRespUpdTerm;
                    sTxBusMsg.senderAddr = MY_ADDR;
                    sTxBusMsg.msg.devBus.receiverAddr = spBusMsg->senderAddr;
                    if (rc == true) {
                        /* programming OK: retrun 1 */
                        sTxBusMsg.msg.devBus.x.devResp.updTerm.success = 1;
                    } else {
                        /* programming error: return 0 */
                        sTxBusMsg.msg.devBus.x.devResp.updTerm.success = 0;
                    }
                    BusSend(&sTxBusMsg);
                }
                break;
            default:
                break;
            }
        }
    } else if (ret == BUS_MSG_ERROR) {
//      LedSet(eLedRedBlinkOnceLong);
   }
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

static void TimerExit(void) {

    TIMSK3 &= ~(1 << OCIE3A);
    TCCR3A = 0;
    TCCR3B = 0;
    TCNT3 = 0;
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
*   switch to idle mode
*/
static void Idle(void) {

    cli();
    if (sIdle == 0) {
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_enable();
        sei();
        sleep_cpu();
        sleep_disable();
    } else {
        sei();
    }
}

/*-----------------------------------------------------------------------------
*  sio idle enable
*/
static void IdleSio0(bool setIdle) {

    if (setIdle == true) {
        sIdle &= ~IDLE_SIO0;
    } else {
        sIdle |= IDLE_SIO0;
    }
}

/*-----------------------------------------------------------------------------
*  switch bus transceiver power down/up
*  called from interrupt context or disabled interrupt, so disable interrupt
*  is not required here
*/
static void BusTransceiverPowerDown(bool powerDown) {

    if (powerDown) {
        BUS_TRANSCEIVER_POWER_DOWN;
    } else {
        BUS_TRANSCEIVER_POWER_UP;
    }
}
