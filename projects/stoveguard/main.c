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

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#include "sio.h"
#include "sysdef.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
/* offset addresses in EEPROM */
#define MODUL_ADDRESS      0
#define OSCCAL_CORR        1
#define SWITCH_ADDR        2
#define SWITCH_INPUT       3

/* our bus address */
#define MY_ADDR            sMyAddr

#define IDLE_SIO           0x01

/* lock/unlock button (pull to GND) on portc.0 and portc.1 */
#define LED_RED_ON         PORTB |= (1 << 0)
#define LED_RED_OFF        PORTB &= ~(1 << 0)
#define LED_GREEN_ON       PORTB |= (1 << 1)
#define LED_GREEN_OFF      PORTB &= ~(1 << 1)

#define BUTTONS_RELEASED   ((PIND & 0b1000) == 0)
#define BUTTONS_PRESSED    ((PIND & 0b1000) != 0)

/* clock calibraation */
#define TIMER1_PRESCALER   (0b001 << CS10) // prescaler clk/1
#define MAX_CAL_TEL        31

/* Port D bit 7 controls bus transceiver power down */
#define BUS_TRANSCEIVER_POWER_DOWN  PORTD |= (1 << 7)
#define BUS_TRANSCEIVER_POWER_UP    PORTD &= ~(1 << 7)

/* ADC */
/* RESF1:0 = 01     select AVCC as AREF */
/* ADLAR = 1        left adjust         */
#define ADMUX_CONF         0b01100000
/* use ADC1, ADC2 ADC3 */
#define MIN_ADC            1
#define MAX_ADC            3

/*-----------------------------------------------------------------------------
*  Typedefs
*/
typedef struct {
    uint8_t oldAdc;
    uint8_t max;
    uint8_t min;
    bool    inc;
    uint8_t diff; // diff of last max and min
} TAdc;

/*-----------------------------------------------------------------------------
*  Variables
*/
char version[] = "stoveguard 0.00";

static TBusTelegram *spRxBusMsg;
static TBusTelegram sTxBusMsg;

volatile uint8_t    gTimeMs;
volatile uint16_t   gTimeMs16;
volatile uint16_t   gTimeS;

static uint8_t      sMyAddr;

static uint8_t      sIdle = 0;

static int          sSioHandle;

#define NUM_ADC     3
static TAdc         sAdc[NUM_ADC];

static uint8_t      sSwitchAddr;
static uint8_t      sSwitchInput;
static bool         sSwitchState;
static bool         sSwitchStateReceived;

/*-----------------------------------------------------------------------------
*  Functions
*/
static void PortInit(void);
static void AdcInit(void);
static void TimerInit(void);
static void Idle(void);
static void ProcessBus(uint8_t ret);
static void SendStartupMsg(void);
static void IdleSio(bool setIdle);
static void BusTransceiverPowerDown(bool powerDown);
static void InitTimer1(void);
static void InitComm(void);
static void ExitComm(void);
static void CurrentMonitor(void);

/*-----------------------------------------------------------------------------
*  program start
*/
int main(void) {

    uint8_t ret;

    MCUSR = 0;
    wdt_disable();

    /* get module address */
    sMyAddr = eeprom_read_byte((const uint8_t *)MODUL_ADDRESS);
    sSwitchAddr = eeprom_read_byte((const uint8_t *)SWITCH_ADDR);
    sSwitchInput = eeprom_read_byte((const uint8_t *)SWITCH_INPUT);

    PortInit();
    AdcInit();
    TimerInit();

    InitComm();

    /* enable global interrupts */
    ENABLE_INT;

    SendStartupMsg();

    while (1) {
        Idle();
        ret = BusCheck();
        ProcessBus(ret);
        CurrentMonitor();
    }
    return 0;  /* never reached */
}

/*-----------------------------------------------------------------------------
*  switch to Idle mode
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
static void IdleSio(bool setIdle) {

    if (setIdle == true) {
        sIdle &= ~IDLE_SIO;
    } else {
        sIdle |= IDLE_SIO;
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

/*-----------------------------------------------------------------------------
 * wait for first low pulse that is long than MIN_PULSE_CNT and return the
 * start timestamp of this low pulse
 */

#define MIN_PULSE_CNT 300

#define NOP_10 asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop")

#define NOP_4  asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop"); \
               asm volatile("nop")


uint16_t Synchronize(void) {

    uint16_t cnt;
    uint16_t startCnt;

    do {
        while ((PIND & 0b00000001) != 0);
        PORTB = 1; /* tigger timer capture */
        NOP_4;
        PORTB = 0;
        startCnt = ICR1;
        while ((PIND & 0b00000001) == 0);
        PORTB = 1; /* tigger timer capture */
        NOP_4;
        PORTB = 0;
        cnt = ICR1;
    } while ((cnt - startCnt) < MIN_PULSE_CNT);

    return startCnt;
}

uint16_t ClkMeasure(void) {

    uint16_t stopCnt;
    uint8_t  i;

    for (i = 0; i < 6; i++) {
        while ((PIND & 0b00000001) != 0);
        NOP_4;
        while ((PIND & 0b00000001) == 0);
        NOP_4;
    }
    while ((PIND & 0b00000001) != 0);
    NOP_4;
    while ((PIND & 0b00000001) == 0);
    PORTB = 1;  /* tigger timer capture */
    NOP_4;
    PORTB = 0;
    stopCnt = ICR1;

    return stopCnt;
}

/*-----------------------------------------------------------------------------
*  process received bus telegrams
*/
static void ProcessBus(uint8_t ret) {

    TBusMsgType                msgType;
    uint8_t                    i;
    uint8_t                    val8;
    uint8_t                    *p;
    bool                       msgForMe = false;
    uint8_t                    flags;
    static bool                sTxRetry = false;
    static TBusTelegram        sTxMsg;
    TBusDevRespActualValue     *pAv;

    if (sTxRetry) {
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        return;
    }

    if (ret == BUS_MSG_OK) {
        msgType = spRxBusMsg->type;
        switch (msgType) {
        case eBusDevReqReboot:
        case eBusDevReqInfo:
        case eBusDevReqEepromRead:
        case eBusDevReqEepromWrite:
        case eBusDevReqDoClockCalib:
        case eBusDevReqDiag:
        case eBusDevRespActualValue:
            if (spRxBusMsg->msg.devBus.receiverAddr == MY_ADDR) {
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
        /* reset controller with watchdog */
        /* set watchdog timeout to shortest value (14 ms) */
        cli();
        wdt_enable(WDTO_15MS);
        /* wait for reset */
        while (1);
        break;
    case eBusDevReqInfo:
        sTxMsg.type = eBusDevRespInfo;
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.info.devType = eBusDevTypeSg;
        strncpy((char *)(sTxMsg.msg.devBus.x.devResp.info.version),
                version, BUS_DEV_INFO_VERSION_LEN);
        sTxMsg.msg.devBus.x.devResp.info.version[BUS_DEV_INFO_VERSION_LEN - 1] = '\0';
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqEepromRead:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespEepromRead;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxMsg.msg.devBus.x.devResp.readEeprom.data =
            eeprom_read_byte((const uint8_t *)spRxBusMsg->msg.devBus.x.devReq.readEeprom.addr);
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqEepromWrite:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespEepromWrite;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        p = &(spRxBusMsg->msg.devBus.x.devReq.writeEeprom.data);
        eeprom_write_byte((uint8_t *)spRxBusMsg->msg.devBus.x.devReq.readEeprom.addr, *p);
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevReqDoClockCalib: {
        uint8_t         old_osccal;
        static uint8_t  sOsccal = 0;
        uint16_t        startCnt;
        uint16_t        stopCnt;
        uint16_t        clocks;
        uint16_t        diff;
        uint16_t        seqLen;
        static int      sCount = 0;
        static uint16_t sMinDiff = 0xffff;
        static uint8_t  sMinOsccal = 0;
        uint8_t         osccal_corr;

        if (spRxBusMsg->msg.devBus.x.devReq.doClockCalib.command == eBusDoClockCalibInit) {
            sCount = 0;
            sOsccal = 0;
            sMinDiff = 0xffff;
        } else if (sCount > MAX_CAL_TEL) {
            sTxMsg.msg.devBus.x.devResp.doClockCalib.state = eBusDoClockCalibStateError;
            sTxMsg.senderAddr = MY_ADDR;
            sTxMsg.type = eBusDevRespDoClockCalib;
            sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
            sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
            break;
        }

        flags = DISABLE_INT;

        BUS_TRANSCEIVER_POWER_UP;
        PORTB = 0;

        /* 8 bytes 0x00: 8 * (1 start bit + 8 data bits) + 7 stop bits  */
        seqLen = 8 * 1000000 * 9 / 9600 + 7 * 1000000 * 1 / 9600; /* = 8229 us */
        old_osccal = OSCCAL;

        ExitComm();
        InitTimer1();

        TCCR1B = (1 << ICES1) | TIMER1_PRESCALER;

        OSCCAL = sOsccal;
        NOP_10;

        for (i = 0; i < 8; i++) {
            startCnt = Synchronize();
            stopCnt = ClkMeasure();

            clocks = stopCnt - startCnt;
            if (clocks > seqLen) {
                diff = clocks - seqLen;
            } else {
                diff = seqLen - clocks;
            }
            if (diff < sMinDiff) {
                sMinDiff = diff;
                sMinOsccal = OSCCAL;
            }
            OSCCAL++;
            NOP_4;
        }

        BUS_TRANSCEIVER_POWER_DOWN;

        InitTimer1();
        InitComm();
        sOsccal = OSCCAL;
        OSCCAL = old_osccal;

        RESTORE_INT(flags);

        if (sCount < MAX_CAL_TEL) {
            sTxMsg.msg.devBus.x.devResp.doClockCalib.state = eBusDoClockCalibStateContiune;
            sCount++;
        } else {
            sTxMsg.msg.devBus.x.devResp.doClockCalib.state = eBusDoClockCalibStateSuccess;
            /* save the osccal correction value to eeprom */
            osccal_corr = eeprom_read_byte((const uint8_t *)OSCCAL_CORR);
            osccal_corr += sMinOsccal - old_osccal;
            eeprom_write_byte((uint8_t *)OSCCAL_CORR, osccal_corr);
            OSCCAL = sMinOsccal;
            NOP_10;
        }
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespDoClockCalib;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    }
    case eBusDevReqDiag:
        sTxMsg.senderAddr = MY_ADDR;
        sTxMsg.type = eBusDevRespDiag;
        sTxMsg.msg.devBus.x.devResp.diag.devType = eBusDevTypeSg;
        sTxMsg.msg.devBus.receiverAddr = spRxBusMsg->senderAddr;
        p = sTxMsg.msg.devBus.x.devResp.diag.data;
        *(p + 0) = sAdc[0].diff;
        *(p + 1) = sAdc[1].diff;
        *(p + 2) = sAdc[2].diff;
        sTxRetry = BusSend(&sTxMsg) != BUS_SEND_OK;
        break;
    case eBusDevRespActualValue:
        pAv = &spRxBusMsg->msg.devBus.x.devResp.actualValue;
        if (pAv->devType == eBusDevTypeSw8) {
            val8 = pAv->actualValue.sw8.state;
            if ((val8 & (1 << sSwitchInput)) == 0) {
				sSwitchState = false;
			} else {
				sSwitchState = true;
			}
			sSwitchStateReceived = true;
        }
        break;        
    default:
        break;
    }
}

/*-----------------------------------------------------------------------------
*  port init
*/
static void PortInit(void) {

    /* configure pins */
    PORTB = 0b00000000;
    DDRB =  0b11111111;

    PORTC = 0b00000000;
    DDRC =  0b10110000;

    PORTD = 0b10001111;
    DDRD =  0b11110010;

}

/*-----------------------------------------------------------------------------
*  adc init
*/
static void AdcInit(void) {

    TAdc *pAdc;
    uint8_t i;
 
    for (pAdc = sAdc, i = 0; i < (MAX_ADC - MIN_ADC); pAdc++, i++) {
        pAdc->min = 0xff;
        pAdc->max = 0;
        pAdc->inc = false;
        pAdc->diff = 0;
        pAdc->oldAdc = 0;
    }

    /* ADC */
    ADMUX = ADMUX_CONF | MIN_ADC; /* start with ADC1 */

    /* digital input disable for ADC0..3 */
    DIDR0 = 0b00001111;

    /* ADEN = 1         ADC enable */
    /* ADPS2:0 = 100    prescaler, division factor is 16 -> 1MHz/16 = 62500 Hz*/
    ADCSRA = 0b10000100;
    /* trigger first conversion */
    ADCSRA |= (1 << ADSC);
}

/*-----------------------------------------------------------------------------
*  timer init
*/
static void TimerInit(void) {

    /* prescaler clk/8
     * compare match 125
     * -> Interrupt period 125/1000000 * 8 = 1 ms */
    TCCR0A = 2 << WGM20; // CTC
    TCCR0B = 2 << CS00;
    OCR0A = 125; /* counting 0-124 */
    TIMSK0 = 1 << OCIE0A;
}

static void InitTimer1(void) {
    /* set input clock to  system clock */
    /* use software input capture       */
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1C = 0;
    TIMSK1 = 0;
    TCNT1 = 0;
    OCR1A = 0;
    OCR1B = 0;
    ICR1 = 0;
    TIFR1 = TIFR1;
}

/*-----------------------------------------------------------------------------
*  init bus communication
*/
static void InitComm(void) {

   SioInit();
   SioRandSeed(sMyAddr);
   sSioHandle = SioOpen("USART0", eSioBaud9600, eSioDataBits8, eSioParityNo,
                       eSioStopBits1, eSioModeHalfDuplex);
   SioSetIdleFunc(sSioHandle, IdleSio);
   SioSetTransceiverPowerDownFunc(sSioHandle, BusTransceiverPowerDown);

   BusTransceiverPowerDown(true);

   BusInit(sSioHandle);
   spRxBusMsg = BusMsgBufGet();
}

/*-----------------------------------------------------------------------------
*  bus communication exit
*/
static void ExitComm(void) {

    BusExit(sSioHandle);
    SioClose(sSioHandle);
    SioExit();
}

/*-----------------------------------------------------------------------------
*  send the startup msg. Delay depends on module address
*/
static void SendStartupMsg(void) {

    uint16_t startCnt;
    uint16_t cntMs;
    uint16_t diff;

    /* send startup message */
    /* delay depends on module address (address * 100 ms) */
    GET_TIME_MS16(startCnt);
    do {
        GET_TIME_MS16(cntMs);
        diff =  cntMs - startCnt;
    } while (diff < ((uint16_t)MY_ADDR * 100));

    sTxBusMsg.type = eBusDevStartup;
    sTxBusMsg.senderAddr = MY_ADDR;
    BusSend(&sTxBusMsg);
}

static void CurrentMonitor(void) {
	
	// if (current > xx) && (duration >> yy)
	// request switchstate
	// wait sSwitchStateReceived
	// if (switchstate == 0) or (timeout Switchstatereceived) -> output off
	// else reset duration
}

/*-----------------------------------------------------------------------------
*  Timer 0 COMPA ISR
*  period:  1 ms
*/
ISR(TIMER0_COMPA_vect) {

    static uint16_t intCnt = 0;
    static uint8_t mux = MIN_ADC;
    uint8_t adc;
    uint8_t oldAdc;
    TAdc *pAdc;

    /* ms counter */
    gTimeMs16 += 1;
    gTimeMs += 1;
    intCnt++;
    if (intCnt >= 1000) { /* 1 ms * 1000 = 1 s*/
        intCnt = 0;
        gTimeS++;
    }
    
    /* ADC */
    pAdc = &sAdc[mux - MIN_ADC];
    adc = ADCH;
    oldAdc = pAdc->oldAdc;
    if (pAdc->inc) {
        if (adc <= oldAdc) {
            /* pos inflection */
            pAdc->max = oldAdc;
            pAdc->diff = pAdc->max - pAdc->min;
            pAdc->inc = false;
            pAdc->min = 0xff;
        }
    } else {
        if (adc >= oldAdc) {
            /* neg inflection */
            pAdc->min = oldAdc;
            pAdc->diff = pAdc->max - pAdc->min;
            pAdc->inc = true;
            pAdc->max = 0;
        }
    }
    pAdc->oldAdc = adc;
    
    mux++;
    if (mux > MAX_ADC) {
        mux = MIN_ADC;
    }
    ADMUX = ADMUX_CONF | mux;
    // trigger next conversion
    ADCSRA |= (1 << ADSC);
}
