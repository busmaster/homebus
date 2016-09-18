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

#include "board.h"
#include "sio.h"
#include "sysdef.h"
#include "bus.h"
#include "flash.h"

/*-----------------------------------------------------------------------------
*  ATMega32u4 fuse bit settings (14.7456 MHz crystal)
*
*  HWBE      = 1
*  BODLEVEL0 = 0
*  BODLEVEL1 = 0
*  BODLEVEL2 = 0
*  OCDEN     = 1
*  JTAGEN    = 1
*  SPIEN     = 0
*  WDTON     = 1
*  EESAVE    = 0
*  BOOTSZ0   = 0
*  BOOTSZ1   = 0
*  BOOTRST   = 0
*  CKDIV8    = 0
*  CKOUT     = 1
*  SUT1      = 0
*  SUT0      = 1
*  CKSEL3    = 1
*  CKSEL2    = 1 
*  CKSEL1    = 1
*  CKSEL0    = 1
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
#define MAX_FIRMWARE_SIZE           (28UL * 1024UL)
#define FLASH_CHECKSUM              0x1234  /* result of checksum calculation */
#define CHECKSUM_BLOCK_SIZE         256     /* num words */

/*-----------------------------------------------------------------------------
*  Variables
*/
volatile uint8_t     gTimeS8 = 0;

static TBusTelegram  *spBusMsg;
static TBusTelegram  sTxBusMsg;
static uint8_t       sFwuState = WAIT_FOR_UPD_ENTER_TIMEOUT;
static uint8_t       sMyAddr;

/*-----------------------------------------------------------------------------
*  Functions
*/
extern void ApplicationEntry(void);

static void PortInit(void);
static void TimerInit(void);
static void TimerStart(void);
static void TimerExit(void);
static void ProcessBus(uint8_t ret);

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
   
    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);

    PortInit();
    TimerInit();

    SioInit();

    spBusMsg = BusMsgBufGet();

    /* switch vector table to bootloader */
    MCUCR = (1 << IVCE);
    MCUCR = (1 << IVSEL);

    /* wait for full supply voltage */
    while (!POWER_GOOD);

    /* calculate application checksum */
    sum = 0;
    for (flashWordAddr = 0; flashWordAddr < (MAX_FIRMWARE_SIZE / 2); flashWordAddr += CHECKSUM_BLOCK_SIZE) {
        sum += FlashSum(flashWordAddr, (uint8_t)CHECKSUM_BLOCK_SIZE);
    }

    if (sum != FLASH_CHECKSUM) {
        /* Fehler */
        sFwuState = WAIT_FOR_UPD_ENTER;
    }

    ENABLE_INT;

    TimerStart();

    /* send startup message */
    sTxBusMsg.type = eBusDevStartup;
    sTxBusMsg.senderAddr = MY_ADDR;
    BusSend(&sTxBusMsg);

    while (1) {
        ret = BusCheck();
        ProcessBus(ret);
        /* wait for update request */
        if (sFwuState == WAIT_FOR_UPD_ENTER_TIMEOUT) {
            if (gTimeS8 >= 4) {
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

    /* jum to application */
    ApplicationEntry();
    /* never returns */
    return 0;
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
    }
}

/*-----------------------------------------------------------------------------
*  timer
*/
static void TimerInit(void) {

    /* configure Timer 0 */
    /* prescaler clk/1024 -> Interrupt period 1024/14745600 * 256 = 17.778 ms */
    TCCR0B = (1 << CS02) | (1 << CS00); 
}

static void TimerStart(void) {
 
    TIMSK0 = 1 << TOIE0;
}

static void TimerExit(void) {

    TIMSK0 = 0;
    TCCR0A = 0;
    TCCR0B = 0;
    TCNT0 = 0;
    TIFR0 = TIFR0;
}

/*-----------------------------------------------------------------------------
*  Timer 0 overflow ISR
*  period:  17.778 ms
*/
ISR(TIMER0_OVF_vect) {

  static uint8_t intCnt = 0;

  intCnt++;
  if (intCnt >= 56) { /* 17.778 ms * 56 = 1 s*/
     intCnt = 0;
     gTimeS8++;
  }
}

/*-----------------------------------------------------------------------------
*  port settings
*/
static void PortInit(void) {

    /* PB.7: digout: output low */
    /* PB.6, digout: output low */
    /* PB.5: digout: output low */
    /* PB.4: unused: output low */
    /* PB.3: MISO: output low */
    /* PB.2: MOSI: output low */
    /* PB.1: SCK: output low */
    /* PB.0: unused: output low */
    PORTB = 0b00000000;
    DDRB  = 0b11111111;

    /* PC.7: digout: output low  */
    /* PC.6: unused: output low */
    PORTC = 0b00000000;
    DDRC  = 0b11000000;

    /* PD.7: unused: output low */
    /* PD.6: unused: output low */
    /* PD.5: transceiver power, output high */
    /* PD.4: unused: output low */
    /* PD.3: TX1: output high */
    /* PD.2: RX1: input pull up */
    /* PD.1: input high z */
    /* PD.0: input high z */
    PORTD = 0b00101100;
    DDRD  = 0b11111000;
    
    /* PE.6: unused: output low  */
    /* PE.2: unused: output low */
    PORTE = 0b00000000;
    DDRE  = 0b01000100;

    /* PF.7: unused: output low  */
    /* PF.6: unused: output low  */
    /* PF.5: unused: output low  */
    /* PF.4: unused: output low  */
    /* PF.1: unused: output low  */
    /* PF.0: unused: output low */
    PORTF = 0b00000000;
    DDRF  = 0b11110011;
}
