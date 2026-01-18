/*
 * sio_c.h
 *
 * Copyright 2016 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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

#ifndef _SIO_C_H
#define _SIO_C_H

#ifdef __cplusplus
extern "C" {
#endif

/* buffer sizes */
#define SIO_RX_BUF_SIZE 64   /* must be power of 2 */
#define SIO_TX_BUF_SIZE 128  /* must be power of 2 */

/* usart channel */
#define SIO               1
#define SIO_UDRE_INT_VEC  USART1_UDRE_vect
#define SIO_TXC_INT_VEC   USART1_TX_vect
#define SIO_RXC_INT_VEC   USART1_RX_vect

/* ext interrupt */
#define SIO_EI_VEC        INT1_vect
#define SIO_EI_EIMSK      INT1
#define SIO_EI_EIFR       INTF1
#define SIO_EI_EICRA_SC0  ISC10
#define SIO_EI_EICRA_SC1  ISC11

/* timer OC1A */
#define SIO_TIMER_VEC        TIMER1_COMPA_vect
#define SIO_TIMER_TCCRA      TCCR1A
#define SIO_TIMER_TCCRB      TCCR1B
#define SIO_TIMER_TCCRB_CS   CS10
#define SIO_TIMER_TCCRC      TCCR1C
#define SIO_TIMER_TCNT       TCNT1
#define SIO_TIMER_TIMSK      TIMSK
#define SIO_TIMER_TIMSK_OCIE OCIE1A
#define SIO_TIMER_OCR        OCR1A
#define SIO_TIMER_TIFR       TIFR
#define SIO_TIMER_TIFR_OCF   OCF1A

#define SIO_TIMER_INIT_TCCR

/* these undefs are required because platform iom128.h defines these as generic
 * bit offsets. But these would be expanded by the N/N2 macros
 */
#undef UCSZ
#undef USBS
#undef TXC
#undef RXEN
#undef RXCIE
#undef TXEN
#undef UDRIE
#undef TXCIE
#undef U2X

#ifdef __cplusplus
}
#endif

#endif
