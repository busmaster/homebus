/*
 * main.c
 *
 * Copyright 2021 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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
#include <avr/wdt.h>

#include "sio.h"
#include "sysdef.h"
#include "bus.h"
#include "flash.h"

/*-----------------------------------------------------------------------------
*  ATMega168 fuse bit settings (internal RC oscillator @ 1 MHz)
*
*  CKSEL0    = 0
*  CKSEL1    = 1
*  CKSEL2    = 0
*  CKSEL3    = 0
*  SUT0      = 0
*  SUT1      = 0
*  CKOUT     = 1
*  CKDIV8    = 0
*
*  BODLEVEL0 = 0
*  BODLEVEL1 = 0
*  BODLEVEL2 = 1
*  EESAVE    = 0
*  WDTON     = 1
*  SPIEN     = 0
*  DWEN      = 1
*  RSTDISBL  = 1
*
*  BOOTRST   = 0
*  BOOTSZ0   = 0
*  BOOTSZ1   = 0
*/

/*-----------------------------------------------------------------------------
*  Macros
*/
/* our bus address */
#define MY_ADDR                     sMyAddr

/* eeprom offsets */
#define MODUL_ADDRESS               0
#define OSCCAL_CORR                 1

/* state machine for firmware update */
#define WAIT_FOR_UPD_ENTER          0
#define WAIT_FOR_UPD_ENTER_TIMEOUT  1
#define WAIT_FOR_UPD_DATA           2

/* max size of firmware */
#define MAX_FIRMWARE_SIZE           (14UL * 1024UL)
#define FLASH_CHECKSUM              0x1234  /* expected checksum */
#define CHECKSUM_BLOCK_SIZE         256 /* words */

/*-----------------------------------------------------------------------------
*  Variables
*/
volatile uint8_t gTimeS8 = 0;

static TBusTelegram  *spRxBusMsg;
static TBusTelegram  sTxBusMsg;
static uint8_t sFwuState = WAIT_FOR_UPD_ENTER_TIMEOUT;
static uint8_t sMyAddr;

/*-----------------------------------------------------------------------------
*  Functions
*/
extern void ApplicationEntry(void);

static void ProcessBus(uint8_t ret);
static void SetMsg(TBusMsgType type, uint8_t receiver);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {

    uint8_t   ret;
    uint16_t  flashWordAddr;
    uint16_t  sum;

    cli();
    MCUSR = 0;
    wdt_disable();

    /* get oscillator correction value from EEPROM */
    EEAR = OSCCAL_CORR;
    /* Start eeprom read by writing EERE */
    EECR |= (1 << EERE);
    /* read data */
    OSCCAL += EEDR;

    /* get modul adress from EEPROM */
    EEAR = MODUL_ADDRESS;
    /* Start eeprom read by writing EERE */
    EECR |= (1 << EERE);
    /* read data */
    sMyAddr = EEDR;

    /* configure pins */
    PORTB = 0b00000000;
    DDRB =  0b11111111;

    PORTC = 0b00000000;
    DDRC =  0b10110000;

    PORTD = 0b10001111;
    DDRD =  0b11110010;

    /* configure Timer 0 */
    /* prescaler clk/64 -> Interrupt period 256/8000000 * 256 = 8.192 ms */
    TCCR0B = 4 << CS00;
    TIMSK0 = 1 << TOIE0;

    SioInit();
    spRxBusMsg = BusMsgBufGet();

    /* Enable change of Interrupt Vectors */
    MCUCR = (1 << IVCE);
    /* Move interrupts to Boot Flash section */
    MCUCR = (1 << IVSEL);

    /* checksum of application */
    sum = 0;
    for (flashWordAddr = 0; flashWordAddr < (MAX_FIRMWARE_SIZE / 2); flashWordAddr += CHECKSUM_BLOCK_SIZE) {
        sum += FlashSum(flashWordAddr, (uint8_t)CHECKSUM_BLOCK_SIZE);
    }

    if (sum != FLASH_CHECKSUM) {
        /* Fehler */
        sFwuState = WAIT_FOR_UPD_ENTER;
    }
    sei();

    /* send startup message */
    sTxBusMsg.type = eBusDevStartup;
    sTxBusMsg.senderAddr = MY_ADDR;
    BusSend(&sTxBusMsg);

    /* main loop */
    while (1) {
        ret = BusCheck();
        ProcessBus(ret);
        /* wait for request to enter firmware update  */
        if (sFwuState == WAIT_FOR_UPD_ENTER_TIMEOUT) {
            if (gTimeS8 >= 1) {
                /* start application */
                break;
            }
        }
    }

    cli();

    /* Enable change of Interrupt Vectors */
    MCUCR = (1 << IVCE);
    /* Move interrupts to application section */
    MCUCR = (0 << IVSEL);

    /* jump to application */
    ApplicationEntry();

    /* never reach this */
    return 0;
}

/*-----------------------------------------------------------------------------
*  process received bus message
*/
static void ProcessBus(uint8_t ret) {

    TBusMsgType msgType;
    uint16_t    *pData;
    uint16_t    wordAddr;
    bool        rc;
    bool        msgForMe = false;

    if (ret != BUS_MSG_OK) {
		return;
    }

    msgType = spRxBusMsg->type;
    switch (msgType) {
    case eBusDevReqReboot:
    case eBusDevReqUpdEnter:
    case eBusDevReqUpdData:
    case eBusDevReqUpdTerm:
        if (spRxBusMsg->msg.devBus.receiverAddr == MY_ADDR) {
            msgForMe = true;
        }
    default:
        break;
    }
    if (msgForMe == false) {
        return;
    }

    if (msgType == eBusDevReqReboot) {
        /* use watchdog to reset */
        /* set watchdog timeout to shortest time (14 ms) */
        cli();
        wdt_enable(WDTO_15MS);
        /* warten auf Reset */
        while (1);
    } else {
        switch (sFwuState) {
        case WAIT_FOR_UPD_ENTER_TIMEOUT:
        case WAIT_FOR_UPD_ENTER:
            if (msgType == eBusDevReqUpdEnter) {
                /* erase application */
                FlashErase();
                /* send response */
                SetMsg(eBusDevRespUpdEnter, spRxBusMsg->senderAddr);
                BusSend(&sTxBusMsg);
                sFwuState = WAIT_FOR_UPD_DATA;
            }
            break;
        case WAIT_FOR_UPD_DATA:
            if (msgType == eBusDevReqUpdData) {
                wordAddr = spRxBusMsg->msg.devBus.x.devReq.updData.wordAddr;
                pData = spRxBusMsg->msg.devBus.x.devReq.updData.data;
                /* program flash */
                rc = FlashProgram(wordAddr, pData, sizeof(spRxBusMsg->msg.devBus.x.devReq.updData.data) / 2);
                /* send response */
                SetMsg(eBusDevRespUpdData, spRxBusMsg->senderAddr);
                if (rc == true) {
                    /* programming OK: return wordAddr */
                    sTxBusMsg.msg.devBus.x.devResp.updData.wordAddr = wordAddr;
                } else {
                    /* programming error: return -1 as wordAddr */
                    sTxBusMsg.msg.devBus.x.devResp.updData.wordAddr = -1;
                }
                BusSend(&sTxBusMsg);
            } else if (msgType == eBusDevReqUpdTerm) {
                /* terminate programming */
                rc = FlashProgramTerminate();
                /* send response */
                SetMsg(eBusDevRespUpdTerm, spRxBusMsg->senderAddr);
                if (rc == true) {
                    /* programming OK */
                    sTxBusMsg.msg.devBus.x.devResp.updTerm.success = 1;
                } else {
                    /* programming error */
                    sTxBusMsg.msg.devBus.x.devResp.updTerm.success = 0;
                }
                BusSend(&sTxBusMsg);
            }
            break;
        default:
            break;
        }
    }
}

/*-----------------------------------------------------------------------------
*  fill in send data
*/
static void SetMsg(TBusMsgType type, uint8_t receiver) {
    sTxBusMsg.type = type;
    sTxBusMsg.senderAddr = MY_ADDR;
    sTxBusMsg.msg.devBus.receiverAddr = receiver;
}

/*-----------------------------------------------------------------------------
*  Timer 0 overflow ISR
*  period:  8.192 ms
*/
ISR(TIMER0_OVF_vect) {

    static uint8_t intCnt = 0;

    intCnt++;
    if (intCnt >= 122) { /* 8.192 ms * 122 = 1 s*/
        intCnt = 0;
        gTimeS8++;
    }
}



