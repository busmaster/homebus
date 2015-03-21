/*
             LUFA Library
     Copyright (C) Dean Camera, 2014.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2014  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the USBtoSerial project. This file contains the main tasks of
 *  the project and is responsible for the initial application hardware configuration.
 */

#include <stdint.h>
#include "sio.h"
#include "usbif.h"


/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = INTERFACE_ID_CDC_CCI,
				.DataINEndpoint                 =
					{
						.Address                = CDC_TX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.DataOUTEndpoint                =
					{
						.Address                = CDC_RX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.NotificationEndpoint           =
					{
						.Address                = CDC_NOTIFICATION_EPADDR,
						.Size                   = CDC_NOTIFICATION_EPSIZE,
						.Banks                  = 1,
					},
			},
	};


uint32_t Boot_Key ATTR_NO_INIT;
#define MAGIC_BOOT_KEY 0xDC42ACCA
#define BOOTLOADER_SEC_SIZE_BYTES 0x1000
#define FLASH_SIZE_BYTES          0x8000
#define BOOTLOADER_START_ADDRESS ((FLASH_SIZE_BYTES - BOOTLOADER_SEC_SIZE_BYTES) >> 1)

void Bootloader_Jump_Check(void) ATTR_INIT_SECTION(3);

#ifdef DEBUG_TRACE
#define TRACE_BUF_SIZE  1024
uint8_t trace_buf[TRACE_BUF_SIZE];
uint16_t trace_wr_idx = 0;
uint16_t trace_rd_idx = 0;

void Trace(uint8_t val1, uint8_t val2) {

    if (trace_wr_idx < (TRACE_BUF_SIZE - 1)) {
        trace_buf[trace_wr_idx] = val1;
        trace_wr_idx++;
        trace_buf[trace_wr_idx] = val2;
        trace_wr_idx++;
    }
}

bool TraceRead(uint8_t *pCh) {

    if (trace_rd_idx <= trace_wr_idx) {
        *pCh = trace_buf[trace_rd_idx];
        trace_rd_idx++;
        return true;
    } else {
        return false;
    }
}
#else
void Trace(uint8_t val1, uint8_t val2) {

}
bool TraceRead(uint8_t *pCh) {
    return false;
}

#endif

#define STX                0x02
#define T0_PRESCALER       1024
#define TCNT0_MS(x)        (F_CPU / T0_PRESCALER * (x) / 1000)
#define TCNT0_TX_TIMEOUT   TCNT0_MS(5)


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
    int     sioFd;
    uint8_t sioRxCh;
    uint8_t sioRxNumChar;

    int16_t usbReceiveCh;
    uint8_t sioTxCh;
    bool    sioTxFrameStarted = false;
    bool    sioDoTx = false;
    bool    sioTxBlocked = false;
    uint8_t currTime;
    uint8_t usbRxTime = 0;

    SetupHardware();
    sioFd = SioOpen("USART1", eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);

    GlobalInterruptEnable();

    for (;;) {
        /* sio tx */
        if (!sioTxBlocked) {
            usbReceiveCh = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
            if (usbReceiveCh >= 0) {
                Trace(TCNT0 - usbRxTime, (uint8_t)usbReceiveCh);
                usbRxTime = TCNT0;
                sioDoTx = true; 
                sioTxCh = (uint8_t)usbReceiveCh;
            }
        }
        currTime = TCNT0;
        if ((sioTxFrameStarted) &&
            ((((uint8_t)(currTime - usbRxTime)) > TCNT0_TX_TIMEOUT) ||
            ((sioDoTx) && (sioTxCh == STX)))) {
            SioSendBuffer(sioFd);
            sioTxFrameStarted = false;
        }
        if (sioDoTx) {
            if (SioWriteBuffered(sioFd, &sioTxCh, sizeof(sioTxCh)) == 0) {
                sioTxBlocked = true;
            } else {
                sioTxBlocked = false;
                sioTxFrameStarted = true;
                sioDoTx = false;
            }
        }

        /* sio rx */
        sioRxNumChar = SioGetNumRxChar(sioFd);
        if (sioRxNumChar > 0) {
            Endpoint_SelectEndpoint(VirtualSerial_CDC_Interface.Config.DataINEndpoint.Address);

            /* Check if a packet is already enqueued to the host - if so, we shouldn't try to send more data
             * until it completes as there is a chance nothing is listening and a lengthy timeout could occur */
            if (Endpoint_IsINReady()) {
                /* Never send more than one bank size less one byte to the host at a time, so that we don't block
                 * while a Zero Length Packet (ZLP) to terminate the transfer is sent if the host isn't listening */
                uint8_t BytesToSend = MIN(sioRxNumChar, (CDC_TXRX_EPSIZE - 1));

                /* Read bytes from the USART receive buffer into the USB IN endpoint */
                while (BytesToSend--) {
                    SioRead(sioFd, &sioRxCh, sizeof(sioRxCh));
                    /* Try to send the next byte of data to the host, abort if there is an error without dequeuing */
                    if (CDC_Device_SendByte(&VirtualSerial_CDC_Interface, sioRxCh) != ENDPOINT_READYWAIT_NoError) {
                        SioUnRead(sioFd, &sioRxCh, sizeof(sioRxCh));
                        break;
                    }
                }
            }
        }

        CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
        USB_USBTask();
    }
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#endif

    /* configure pins to input/pull up */
    PORTB = 0b11111111;
    DDRB =  0b00000000;

    /* set PD3 to output mode */
    PORTD = 0b11111111;
    DDRD =  0b00001000;

    SioInit();
    SioRandSeed(0 /* todo */);

    /* configure Timer 0 */
    /* prescaler clk/1024 */
    TCCR0B = 5 << CS00; 

    /* Hardware Initialization */
    USB_Init();
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

void Jump_To_Bootloader(void);

/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
    if (CDCInterfaceInfo->State.LineEncoding.BaudRateBPS == 57600) {
        uint8_t ch1;
        uint8_t ch2;
        char    buf[20];
        uint8_t i;

        while (TraceRead(&ch1) && TraceRead(&ch2)) {
            Endpoint_SelectEndpoint(VirtualSerial_CDC_Interface.Config.DataINEndpoint.Address);

            sprintf(buf, "%d %02x\r\n", ch1, ch2);

            for (i = 0; Endpoint_IsINReady() && (i < strlen(buf)); i++) {
                CDC_Device_SendByte(&VirtualSerial_CDC_Interface, buf[i]);
            }
        }
    }

    if ((CDCInterfaceInfo->State.LineEncoding.ParityType == CDC_PARITY_Odd) &&
        (CDCInterfaceInfo->State.LineEncoding.DataBits == 6) &&
        (CDCInterfaceInfo->State.LineEncoding.BaudRateBPS == 115200)) {
        /* enter bootloader on line setting 115200/6 data bits/odd parity */
        Jump_To_Bootloader();
    } else {
        /* we don't want to change settings */
        return;
    }
}


void Bootloader_Jump_Check(void)
{
    // If the reset source was the bootloader and the key is correct, clear it and jump to the bootloader
    if ((MCUSR & (1 << WDRF)) && (Boot_Key == MAGIC_BOOT_KEY))
    {
        Boot_Key = 0;
        ((void (*)(void))BOOTLOADER_START_ADDRESS)();
        }
    }
void Jump_To_Bootloader(void)
{
    // If USB is used, detach from the bus and reset it
    USB_Disable();
    // Disable all interrupts
    cli();
    // Wait two seconds for the USB detachment to register on the host
    Delay_MS(2000);
    // Set the bootloader key to the magic value and force a reset
    Boot_Key = MAGIC_BOOT_KEY;
    wdt_enable(WDTO_250MS);
    for (;;);
}
