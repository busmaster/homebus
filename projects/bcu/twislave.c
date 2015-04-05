/*#################################################################################################
	Title	: TWI SLave
	Author	: Martin Junghans <jtronics@gmx.de>
	Hompage	: www.jtronics.de
	Software: AVR-GCC / Programmers Notpad 2
	License	: GNU General Public License 
	
	Aufgabe	:
	Betrieb eines AVRs mit Hardware-TWI-Schnittstelle als Slave. 
	Zu Beginn muss init_twi_slave mit der gewünschten Slave-Adresse als Parameter aufgerufen werden. 
	Der Datenaustausch mit dem Master erfolgt über die Buffer rxbuffer und txbuffer, auf die von Master und Slave zugegriffen werden kann. 
	rxbuffer und txbuffer sind globale Variablen (Array aus uint8_t).
	
	Ablauf:
	Die Ansteuerung des rxbuffers, in den der Master schreiben kann, erfolgt ähnlich wie bei einem normalen I2C-EEPROM.
	Man sendet zunächst die Bufferposition, an die man schreiben will, und dann die Daten. Die Bufferposition wird 
	automatisch hochgezählt, sodass man mehrere Datenbytes hintereinander schreiben kann, ohne jedesmal
	die Bufferadresse zu schreiben.
	Um den txbuffer vom Master aus zu lesen, überträgt man zunächst in einem Schreibzugriff die gewünschte Bufferposition und
	liest dann nach einem repeated start die Daten aus. Die Bufferposition wird automatisch hochgezählt, sodass man mehrere
	Datenbytes hintereinander lesen kann, ohne jedesmal die Bufferposition zu schreiben.

	Abgefangene Fehlbedienung durch den Master:
	- Lesen über die Grenze des txbuffers hinaus
	- Schreiben über die Grenzen des rxbuffers hinaus
	- Angabe einer ungültigen Schreib/Lese-Adresse
	- Lesezuggriff, ohne vorher Leseadresse geschrieben zu haben
	
	LICENSE:
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

//#################################################################################################*/

#include <util/twi.h> 								// Bezeichnungen für Statuscodes in TWSR
#include <avr/interrupt.h> 							// behandlung der Interrupts
#include <avr/pgmspace.h>

#include <inttypes.h>
// #include <compat/twi.h>

#include <stdint.h> 								// definiert Datentyp uint8_t
#include "twislave.h" 								

//#################################### Macros
//ACK nach empfangenen Daten senden/ ACK nach gesendeten Daten erwarten
#define TWCR_ACK 	TWCR = (1<<TWEN)|(1<<TWIE)|(1<<TWINT)|(1<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|(0<<TWWC);  

//NACK nach empfangenen Daten senden/ NACK nach gesendeten Daten erwarten     
#define TWCR_NACK 	TWCR = (1<<TWEN)|(1<<TWIE)|(1<<TWINT)|(0<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|(0<<TWWC);

//switched to the non adressed slave mode...
#define TWCR_RESET 	TWCR = (1<<TWEN)|(1<<TWIE)|(1<<TWINT)|(1<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|(0<<TWWC);  

//###################### Slave-Adresse
#define SLAVE_ADRESSE 0x3F 								// Slave-Adresse

//###################### Variablen
static const uint16_t ccitt_table[256] PROGMEM = {
		0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
		0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
		0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
		0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
		0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
		0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
		0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
		0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
		0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,
		0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
		0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
		0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
		0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,
		0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
		0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,
		0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
		0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
		0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
		0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
		0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
		0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
		0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
		0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
		0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
		0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
		0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
		0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
		0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
		0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
		0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
		0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
		0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
};

const uint8_t nr_table[8] = {4,4,5,5,6,6,7,7};
const uint8_t side_table[8] = {1,2,1,2,1,2,1,2};

uint8_t gc_call = 0, boot_up_gc = 1;

uint16_t crc_ccitt_table(uint8_t index) {
	return pgm_read_word(&ccitt_table[index]);
}

//########################################################################################## init_twi_slave 
void init_twi_slave(uint8_t adr)
{
	TWAR= adr; //Adresse setzen
	TWCR &= ~(1<<TWSTA)|(1<<TWSTO);
	TWCR|= (1<<TWEA) | (1<<TWEN)|(1<<TWIE); 	 
	sei();
}

//########################################################################################## ISR (TWI_vect) 
//ISR, die bei einem Ereignis auf dem Bus ausgelöst wird. Im Register TWSR befindet sich dann 
//ein Statuscode, anhand dessen die Situation festgestellt werden kann.
ISR (TWI_vect)  
{
	uint8_t data=0;
	switch (TW_STATUS) 								// TWI-Statusregister prüfen und nötige Aktion bestimmen 
		{
		case TW_SR_SLA_ACK: 						// 0x60 Slave Receiver, wurde adressiert	
			TWCR_ACK; 								// nächstes Datenbyte empfangen, ACK danach
			gc_call = 0;
			buffer_adr=0x00; 						// Bufferposition ist bei 0 starten
			buffer_crc = 0;
			rxcrc[buffer_crc++] = crc_ccitt_byte(0x1d0f, TWDR);
			break;

		case TW_SR_DATA_ACK: 						// 0x80 Slave Receiver,Daten empfangen
			data=TWDR; 								// Empfangene Daten auslesen
			rxbuffer[buffer_adr]=data; 		// Daten in Buffer schreiben
			buffer_adr++; 					// Buffer-Adresse weiterzählen für nächsten Schreibzugriff
			rxcrc[buffer_crc] = crc_ccitt_byte(rxcrc[buffer_crc-1], data);
			buffer_crc++;
			if(buffer_adr<(buffer_size-1)) // im Buffer ist noch Platz für mehr als ein Byte
			   {
			   TWCR_ACK;				// nächstes Datenbyte empfangen, ACK danach, um nächstes Byte anzufordern
			   }
			else   							// es kann nur noch ein Byte kommen, dann ist der Buffer voll
			   {
			   TWCR_NACK;				// letztes Byte lesen, dann NACK, um vollen Buffer zu signaliseren
			   }
			break;

		case TW_ST_SLA_ACK: 						//
		case TW_ST_DATA_ACK: 						// 0xB8 Slave Transmitter, weitere Daten wurden angefordert
			TWDR = 0x2F;
			TWCR_NACK;
			break;
		
		case TW_SR_GCALL_ACK:
			TWCR_ACK;
			gc_call = 1;
			buffer_adr=0x00; 
			buffer_crc = 0;
			rxcrc[buffer_crc++] = crc_ccitt_byte(0x1d0f, TWDR);
			break;

		case TW_SR_GCALL_DATA_ACK:           // general call data received, ACK returned 
			data=TWDR; 								// Empfangene Daten auslesen
			rxbuffer[buffer_adr]=data; 		// Daten in Buffer schreiben
			buffer_adr++; 					// Buffer-Adresse weiterzählen für nächsten Schreibzugriff
			rxcrc[buffer_crc] = crc_ccitt_byte(rxcrc[buffer_crc-1], data);
			buffer_crc++;
			if(buffer_adr<(buffer_size-1)) // im Buffer ist noch Platz für mehr als ein Byte
			   {
			   TWCR_ACK;				// nächstes Datenbyte empfangen, ACK danach, um nächstes Byte anzufordern
			   }
			else   							// es kann nur noch ein Byte kommen, dann ist der Buffer voll
			   {
			   TWCR_NACK;				// letztes Byte lesen, dann NACK, um vollen Buffer zu signaliseren
			   }
			break;		
		
		case TW_SR_STOP: //prüfe Daten 							// 0xA0 STOP empfangen
		    if(gc_call == 1) {
				if(boot_up_gc == 1) {
					// es ist ernst
					if(rxbuffer[4]==0x10 && rxbuffer[5]==0x61)
						send_startup = 1;
				}
				else {
					boot_up_gc = 0;
				}
			}
			if(rxbuffer[1] == 0xF5) {
				if((rxbuffer[buffer_adr-2]==((rxcrc[buffer_crc-3]>>8)&0xFF)) && (rxbuffer[buffer_adr-1]==(rxcrc[buffer_crc-3]&0xFF))) {// Empfangen CRC prüfen
				   switch (rxbuffer[5]) {
					   case 0x04: if(rxbuffer[6] == 3) {
									button_register = (button_register&(~0x01)) + rxbuffer[7];
									}
								  else if(rxbuffer[6] == 4) {
									button_register = (button_register&(~0x02)) + (rxbuffer[7]<<1);
									}								
									break;
					   case 0x05: if(rxbuffer[6] == 3) {
									button_register = (button_register&(~0x04)) + (rxbuffer[7]<<2);
									}
								  else if(rxbuffer[6] == 4) {
									button_register = (button_register&(~0x08)) + (rxbuffer[7]<<3);
									}								
									break;
					   case 0x06: if(rxbuffer[6] == 3) {
									button_register = (button_register&(~0x10)) + (rxbuffer[7]<<4);
									}
								  else if(rxbuffer[6] == 4) {
									button_register = (button_register&(~0x20)) + (rxbuffer[7]<<5);
									}								
									break;
					   case 0x07: if(rxbuffer[6] == 3) {
									button_register = (button_register&(~0x40)) + (rxbuffer[7]<<6);
									}
								  else if(rxbuffer[6] == 4) {
									button_register = (button_register&(~0x80)) + (rxbuffer[7]<<7);
									}								
									break;	
						default: break;
					   }
				   }
			   }

		case TW_ST_DATA_NACK: 						// 0xC0 Keine Daten mehr gefordert 
		case TW_SR_DATA_NACK: 						// 0x88 
		case TW_ST_LAST_DATA: 						// 0xC8  Last data byte in TWDR has been transmitted (TWEA = “0”); ACK has been received
		default: 	
			TWCR_RESET; 							// Übertragung beenden, warten bis zur nächsten Adressierung
			break;	
		} //end.switch (TW_STATUS)
} //end.ISR(TWI_vect)

// static inline uint16_t crc_ccitt_byte(uint16_t crc, const uint8_t c)
uint16_t crc_ccitt_byte(uint16_t crc, const uint8_t c)
{
        return (crc << 8) ^ crc_ccitt_table(((crc >> 8) ^ c) & 0xff);
}

/**
 *	crc_ccitt - recompute the CRC for the data buffer
 *	@crc: previous CRC value
 *	@buffer: data pointer
 *	@len: number of bytes in the buffer
 */
uint16_t crc_ccitt(uint16_t crc, uint8_t const *buffer, size_t len)
{
	while (len--)
		crc = crc_ccitt_byte(crc, *buffer++);
	return crc;
}



void set_LED(uint8_t nr,uint8_t color){

   uint8_t i;
   uint8_t  const data[6] = {0xBE,0x3F,0xF5,0x00,0x04,0x00};
   uint16_t crc = 0x1d0f;

   if( nr < 8 ) {	//Wertebereich prüfen
      if( color < 0x0f ) {
         crc = crc_ccitt(crc,data,sizeof(data));

         i2c_start_wait(data[0]);
         for(i=1;i<sizeof(data);i++) {
            i2c_write(data[i]);
            }
         i2c_write(nr_table[nr]);
         crc =  crc_ccitt_byte(crc, nr_table[nr]);
         i2c_write(side_table[nr]);
         crc =  crc_ccitt_byte(crc, side_table[nr]);
         i2c_write(color);
         crc =  crc_ccitt_byte(crc, color);
         i2c_write((crc>>8)&0xFF);
         i2c_write(crc&0xFF);
         i2c_rep_start(0xBF);
         i2c_readNak();
         i2c_stop();
         }
      }
}
void init_SWITCH(void) {
	
   uint8_t i;
   uint8_t const data[7] = {0x00,0x3F,0xF0,0x00,0x02,0x20,0x00};
   uint16_t crc = 0x1d0f;

   crc = crc_ccitt(crc,data,sizeof(data));

   i2c_start_wait(data[0]);
   for(i=1;i<sizeof(data);i++) {
      i2c_write(data[i]);
   } 
   i2c_write((crc>>8)&0xFF);
   i2c_write(crc&0xFF);
   i2c_stop();
}

void init_LED(void) {
	
   uint8_t i;
   uint8_t const data[9] = {0x00,0x3F,0xF0,0x00,0x04,0x50,0xBE,0x00,0x04};	
   uint16_t crc = 0x1d0f;

   crc = crc_ccitt(crc,data,sizeof(data));

   i2c_start_wait(data[0]);
   for(i=1;i<sizeof(data);i++) {
      i2c_write(data[i]);
   } 
   i2c_write((crc>>8)&0xFF);
   i2c_write(crc&0xFF);
   i2c_stop();
}
void set_SWITCH(uint8_t nr){
   uint8_t i;
   uint8_t  const data[10] = {0xBE,0x3F,0xF5,0x00,0x05,0x00,0x07,0x00,0x00,0x10};	
   uint16_t crc = 0x1d0f;

   if( nr > 7 || nr < 4 ) nr = 7;	//Wertebereich prüfen
   
   crc = crc_ccitt_byte(crc, data[0]);

   i2c_start_wait(data[0]);
   for(i=1;i<sizeof(data);i++) {
	  if (i != 6) {
         i2c_write(data[i]);
         crc = crc_ccitt_byte(crc, data[i]);
	  }		 
      else {
		 i2c_write(nr);  
         crc = crc_ccitt_byte(crc, nr);
	  }
   }   
   i2c_write((crc>>8)&0xFF);
   i2c_write(crc&0xFF);
   i2c_rep_start(0xBF);
   i2c_readNak();
   i2c_stop();
}
 
void init_BOOT_ONE(void) {
	
   uint8_t i;
   uint8_t const data[6] = {0x00,0x3F,0xF0,0x00,0x01,0x00};	
   uint16_t crc = 0x1d0f;

   crc = crc_ccitt(crc,data,sizeof(data));

   i2c_start_wait(data[0]);
   for(i=1;i<sizeof(data);i++) {
      i2c_write(data[i]);
   } 
   i2c_write((crc>>8)&0xFF);
   i2c_write(crc&0xFF);
   i2c_stop();
}
void init_BOOT_TWO(void) {
	
   uint8_t i;
   uint8_t const data[7] = {0x00,0x3F,0xF0,0x00,0x02,0x40,0x01};	
   uint16_t crc = 0x1d0f;

   crc = crc_ccitt(crc,data,sizeof(data));

   i2c_start_wait(data[0]);
   for(i=1;i<sizeof(data);i++) {
      i2c_write(data[i]);
   } 
   i2c_write((crc>>8)&0xFF);
   i2c_write(crc&0xFF);
   i2c_stop();	
}

void stop_KNX(void) {

   uint8_t i;
   uint8_t const data[7] = {0x00,0x3F,0xF0,0x00,0x02,0x40,0x00};	
   uint16_t crc = 0x1d0f;

   crc = crc_ccitt(crc,data,sizeof(data));

   i2c_start_wait(data[0]);
   for(i=1;i<sizeof(data);i++) {
      i2c_write(data[i]);
   } 
   i2c_write((crc>>8)&0xFF);
   i2c_write(crc&0xFF);
   i2c_stop();
} 
/*************************************************************************
 Initialization of the I2C bus interface. Need to be called only once
*************************************************************************/
void i2c_init(void)
{
  /* initialize TWI clock: 100 kHz clock, TWPS = 0 => prescaler = 1 */
  
  TWSR = 0;                         /* no prescaler */
  //TWBR = ((F_CPU/SCL_CLOCK)-16)/2;  /* must be > 10 for stable operation */
  TWBR = 32;  /* must be > 10 for stable operation */

}/* i2c_init */


/*************************************************************************	
  Issues a start condition and sends address and transfer direction.
  return 0 = device accessible, 1= failed to access device
*************************************************************************/
unsigned char i2c_start(unsigned char address)
{
    uint8_t   twst;

	// send START condition
	TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);

	// wait until transmission completed
	while(!(TWCR & (1<<TWINT)));

	// check value of TWI Status Register. Mask prescaler bits.
	twst = TW_STATUS & 0xF8;
	if ( (twst != TW_START) && (twst != TW_REP_START)) return 1;

	// send device address
	TWDR = address;
	TWCR = (1<<TWINT) | (1<<TWEN);

	// wail until transmission completed and ACK/NACK has been received
	while(!(TWCR & (1<<TWINT)));

	// check value of TWI Status Register. Mask prescaler bits.
	twst = TW_STATUS & 0xF8;
	if ( (twst != TW_MT_SLA_ACK) && (twst != TW_MR_SLA_ACK) ) return 1;

	return 0;

}/* i2c_start */


/*************************************************************************
 Issues a start condition and sends address and transfer direction.
 If device is busy, use ack polling to wait until device is ready
 
 Input:   address and transfer direction of I2C device
*************************************************************************/
void i2c_start_wait(unsigned char address)
{
    uint8_t   twst;

	TWI_Check_Bus();

    while ( 1 )
    {
	    // send START condition
	    TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
    
    	// wait until transmission completed
    	while(!(TWCR & (1<<TWINT)));
    
    	// check value of TWI Status Register. Mask prescaler bits.
    	twst = TW_STATUS & 0xF8;
    	if ( (twst != TW_START) && (twst != TW_REP_START)) continue;
    
    	// send device address
    	TWDR = address;
    	TWCR = (1<<TWINT) | (1<<TWEN);
    
    	// wail until transmission completed
    	while(!(TWCR & (1<<TWINT)));
    
    	// check value of TWI Status Register. Mask prescaler bits.
    	twst = TW_STATUS & 0xF8;
    	if ( (twst == TW_MT_SLA_NACK )||(twst ==TW_MR_DATA_NACK) ) 
    	{    	    
    	    /* device busy, send stop condition to terminate write operation */
	        TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
	        
	        // wait until stop condition is executed and bus released
	        while(TWCR & (1<<TWSTO));
	        
    	    continue;
    	}
    	//if( twst != TW_MT_SLA_ACK) return 1;
    	break;
     }

}/* i2c_start_wait */


/*************************************************************************
 Issues a repeated start condition and sends address and transfer direction 

 Input:   address and transfer direction of I2C device
 
 Return:  0 device accessible
          1 failed to access device
*************************************************************************/
unsigned char i2c_rep_start(unsigned char address)
{
    return i2c_start( address );

}/* i2c_rep_start */


/*************************************************************************
 Terminates the data transfer and releases the I2C bus
*************************************************************************/
void i2c_stop(void)
{
    /* send stop condition */
	TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
	
	// wait until stop condition is executed and bus released
	while(TWCR & (1<<TWSTO));

}/* i2c_stop */


/*************************************************************************
  Send one byte to I2C device
  
  Input:    byte to be transfered
  Return:   0 write successful 
            1 write failed
*************************************************************************/
unsigned char i2c_write( unsigned char data )
{	
    uint8_t   twst;
    
        TWI_Check_Bus();

	// send data to the previously addressed device
	TWDR = data;
	TWCR = (1<<TWINT) | (1<<TWEN);

	// wait until transmission completed
	while(!(TWCR & (1<<TWINT)));

	// check value of TWI Status Register. Mask prescaler bits
	twst = TW_STATUS & 0xF8;
	if( twst != TW_MT_DATA_ACK) return 1;
	return 0;

}/* i2c_write */


/*************************************************************************
 Read one byte from the I2C device, request more data from device 
 
 Return:  byte read from I2C device
*************************************************************************/
unsigned char i2c_readAck(void)
{
	TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
	while(!(TWCR & (1<<TWINT)));    

    return TWDR;

}/* i2c_readAck */


/*************************************************************************
 Read one byte from the I2C device, read is followed by a stop condition 
 
 Return:  byte read from I2C device
*************************************************************************/
unsigned char i2c_readNak(void)
{
	TWCR = (1<<TWINT) | (1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
	
    return TWDR;

}/* i2c_readNak */

void init_BJ(uint8_t slaadr) {

 i2c_init();
 i2c_slave(slaadr);
 
 init_BOOT_ONE();
 i2c_slave(slaadr);
 
_delay_us(800000);
 init_BOOT_TWO();
 i2c_slave(slaadr);

 i2c_slave(slaadr);
 
_delay_us(2500000);

init_SWITCH();
i2c_slave(slaadr);

set_SWITCH(7);
set_SWITCH(6);
set_SWITCH(5);
set_SWITCH(4); 

init_LED();

i2c_slave(slaadr);
send_startup = 0;	
}

void i2c_slave(uint8_t slaadr)
	{
	cli();

	//### TWI 
	init_twi_slave(slaadr);			//TWI als Slave mit Adresse slaveadr starten 
	
	sei();
	}

void TWI_Check_Bus(void)
{
   uint8_t t = 10;
   if (!(TWCR & (1<<TWIE))) return;       // bei einem Repeated Start beenden

   while(t--)
   {
      _delay_us(1);
      if (!(TWI_PORT_PIN & (1<<SCL_PIN))) t = 10;
   }
}
