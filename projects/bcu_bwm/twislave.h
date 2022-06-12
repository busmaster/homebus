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
#ifndef _TWISLAVE_H
#define _TWISLAVE_H

#include <avr/io.h>
#include <util/delay.h>

#define TWI_PORT_PIN    PINC                       // an diesen Pins wird auf low getestet
#define SDA_PIN         PC4
#define SCL_PIN         PC5

//Bei zu alten AVR-GCC-Versionen werden die Interrupts anders genutzt, in diesem Fall Fehlermeldung
#if (__GNUC__ * 100 + __GNUC_MINOR__) < 304
	#error "This library requires AVR-GCC 3.4.5 or later, update to newer AVR-GCC compiler !"
#endif

//#################################### von Benutzer konfigurierbare Einstellung 

#define buffer_size 20 								//Größe der Buffer in Byte (2..254)

//#################################### Schutz vor unsinnigen Buffergrößen
#if (buffer_size > 254)
	#error Buffer zu groß gewählt! Maximal 254 Bytes erlaubt.
#endif

#if (buffer_size < 2)
	#error Buffer muss mindestens zwei Byte groß sein!
#endif





/** defines the data direction (reading from I2C device) in i2c_start(),i2c_rep_start() */
#define I2C_READ    1

/** defines the data direction (writing to I2C device) in i2c_start(),i2c_rep_start() */
#define I2C_WRITE   0




//#################################### Globale Variablen, die vom Hauptprogramm genutzt werden 

volatile uint8_t rxbuffer[buffer_size];				//Der Empfangsbuffer, der vom Slave ausgelesen werden kann.
volatile uint8_t buffer_adr; 						//"Adressregister" für den Buffer
volatile uint16_t rxcrc[buffer_size];
volatile uint8_t buffer_crc;
volatile uint8_t bwm_on_register;
volatile uint8_t switch_register;
volatile uint16_t time_poti;
volatile uint8_t lux_poti;
volatile uint8_t lux_register;
volatile uint16_t crc_rx;
volatile uint8_t send_startup;

void init_twi_slave(uint8_t adr);
uint16_t crc_ccitt(uint16_t crc, const uint8_t *buffer, size_t len);

// static inline uint16_t crc_ccitt_byte(uint16_t crc, const uint8_t c);
uint16_t crc_ccitt_byte(uint16_t crc, const uint8_t c);

uint16_t crc_ccitt_table(uint8_t index);

void init_BOOT_ONE(void);
void init_BOOT_TWO(void);
void init_SWITCH(void);
void set_SWITCH1(void);
void set_SWITCH2(void);
void set_SWITCH(void);
void init_LED(void);
void stop_KNX(void);
void init_BJ(uint8_t slaadr);
void i2c_slave(uint8_t slaadr);
void TWI_Check_Bus(void);

/**
 @brief initialize the I2C master interace. Need to be called only once 
 @param  void
 @return none
 */
extern void i2c_init(void);


/** 
 @brief Terminates the data transfer and releases the I2C bus 
 @param void
 @return none
 */
extern void i2c_stop(void);


/** 
 @brief Issues a start condition and sends address and transfer direction 
  
 @param    addr address and transfer direction of I2C device
 @retval   0   device accessible 
 @retval   1   failed to access device 
 */
extern unsigned char i2c_start(unsigned char addr);


/**
 @brief Issues a repeated start condition and sends address and transfer direction 

 @param   addr address and transfer direction of I2C device
 @retval  0 device accessible
 @retval  1 failed to access device
 */
extern unsigned char i2c_rep_start(unsigned char addr);


/**
 @brief Issues a start condition and sends address and transfer direction 
   
 If device is busy, use ack polling to wait until device ready 
 @param    addr address and transfer direction of I2C device
 @return   none
 */
extern void i2c_start_wait(unsigned char addr);

 
/**
 @brief Send one byte to I2C device
 @param    data  byte to be transfered
 @retval   0 write successful
 @retval   1 write failed
 */
extern unsigned char i2c_write(unsigned char data);


/**
 @brief    read one byte from the I2C device, request more data from device 
 @return   byte read from I2C device
 */
extern unsigned char i2c_readAck(void);

/**
 @brief    read one byte from the I2C device, read is followed by a stop condition 
 @return   byte read from I2C device
 */
extern unsigned char i2c_readNak(void);

/** 
 @brief    read one byte from the I2C device
 
 Implemented as a macro, which calls either i2c_readAck or i2c_readNak
 
 @param    ack 1 send ack, request more data from device<br>
               0 send nak, read is followed by a stop condition 
 @return   byte read from I2C device
 */
extern unsigned char i2c_read(unsigned char ack);
#define i2c_read(ack)  (ack) ? i2c_readAck() : i2c_readNak(); 


#endif //#ifdef _TWISLAVE_H
