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
 *  USB Device Configuration Descriptor processing routines, to determine the correct pipe configurations
 *  needed to communication with an attached USB device. Descriptors are special  computer-readable structures
 *  which the host requests upon device enumeration, to determine the device's capabilities and functions.
 */

#include "ConfigDescriptor.h"

uint8_t DComp_NextDataInterfaceEndpoint(void* CurrentDescriptor);
uint8_t DComp_NextDataInterface(void* currentDescriptor);

/** Reads and processes an attached device's descriptors, to determine compatibility and pipe configurations. This
 *  routine will read in the entire configuration descriptor, and configure the hosts pipes to correctly communicate
 *  with compatible devices.
 *
 *  This routine searches for a FT232 interface descriptor containing bulk data IN and OUT endpoints.
 *
 *  \return An error code from the \ref Host_GetConfigDescriptorDataCodes_t enum.
 */
uint8_t ProcessConfigDescriptor(void)
{
    uint8_t  confDescBuf[0x20];
    void*    currConfLocation = confDescBuf;
    uint16_t currConfBytesRem;
    uint8_t  rc;

    USB_Descriptor_Endpoint_t  *dataINEndpoint    = 0;
    USB_Descriptor_Endpoint_t  *dataOUTEndpoint   = 0;
    USB_Descriptor_Endpoint_t  *endpointData;

//eeprom_write_byte((uint8_t *)0x70, 1);

    /* Retrieve the entire configuration descriptor into the allocated buffer */
    switch (USB_Host_GetDeviceConfigDescriptor(1, &currConfBytesRem, confDescBuf, sizeof(confDescBuf))) {
    case HOST_GETCONFIG_Successful:
//eeprom_write_byte((uint8_t *)0x71, 1);
        break;
    case HOST_GETCONFIG_InvalidData:
//eeprom_write_byte((uint8_t *)0x72, 1);
        return InvalidConfigDataReturned;
    case HOST_GETCONFIG_BuffOverflow:
//eeprom_write_byte((uint8_t *)0x73, 1);
        return DescriptorTooLarge;
    default:
//eeprom_write_byte((uint8_t *)0x74, 1);
        return ControlError;
    }

//eeprom_write_byte((uint8_t *)0x75, 1);

    do {
        rc = USB_GetNextDescriptorComp(&currConfBytesRem, &currConfLocation, DComp_NextDataInterface);
    } while ((rc != DESCRIPTOR_SEARCH_COMP_Found) && (rc != DESCRIPTOR_SEARCH_COMP_EndOfDescriptor));

//eeprom_write_byte((uint8_t *)0x76, 1);


    if (rc != DESCRIPTOR_SEARCH_COMP_Found) {
        return NoCompatibleInterfaceFound;
    }
    
//eeprom_write_byte((uint8_t *)0x77, 1);
    
    if (USB_GetNextDescriptorComp(&currConfBytesRem, &currConfLocation, DComp_NextDataInterfaceEndpoint) != DESCRIPTOR_SEARCH_COMP_Found) {
        return NoCompatibleInterfaceFound;
    }
//eeprom_write_byte((uint8_t *)0x78, 1);
    
    /* Retrieve the endpoint address from the endpoint descriptor */
    endpointData = DESCRIPTOR_PCAST(currConfLocation, USB_Descriptor_Endpoint_t);
    /* If the endpoint is a IN type endpoint */
    if ((endpointData->EndpointAddress & ENDPOINT_DIR_MASK) == ENDPOINT_DIR_IN) {
//eeprom_write_byte((uint8_t *)0x79, 1);
        
        dataINEndpoint = endpointData;
    } else {
//eeprom_write_byte((uint8_t *)0x7a, 1);
        
        dataOUTEndpoint = endpointData;
    }
//eeprom_write_byte((uint8_t *)0x7b, 1);

    
    if (USB_GetNextDescriptorComp(&currConfBytesRem, &currConfLocation, DComp_NextDataInterfaceEndpoint) != DESCRIPTOR_SEARCH_COMP_Found) {
        return NoCompatibleInterfaceFound;
    }
//eeprom_write_byte((uint8_t *)0x7c, 1);
    
    /* Retrieve the endpoint address from the endpoint descriptor */
    endpointData = DESCRIPTOR_PCAST(currConfLocation, USB_Descriptor_Endpoint_t);
    /* If the endpoint is a IN type endpoint */
    if ((endpointData->EndpointAddress & ENDPOINT_DIR_MASK) == ENDPOINT_DIR_IN) {
//eeprom_write_byte((uint8_t *)0x7d, 1);
        dataINEndpoint = endpointData;
    } else {
//eeprom_write_byte((uint8_t *)0x7e, 1);
        dataOUTEndpoint = endpointData;
    }
//eeprom_write_byte((uint8_t *)0x7f, 1);

    if ((dataINEndpoint == 0) || (dataOUTEndpoint == 0)) {
        return NoCompatibleInterfaceFound;
    }
//eeprom_write_byte((uint8_t *)0x80, 1);

    /* Configure the data IN pipe */
    Pipe_ConfigurePipe(FT232_DATA_IN_PIPE, EP_TYPE_BULK, dataINEndpoint->EndpointAddress, dataINEndpoint->EndpointSize, 1);

    /* Configure the data OUT pipe */
    Pipe_ConfigurePipe(FT232_DATA_OUT_PIPE, EP_TYPE_BULK, dataOUTEndpoint->EndpointAddress, dataOUTEndpoint->EndpointSize, 1);

//eeprom_write_byte((uint8_t *)0x81, 1);

    /* Valid data found, return success */
    return SuccessfulConfigRead;
}


/** Descriptor comparator function. This comparator function is can be called while processing an attached USB device's
 *  configuration descriptor, to search for a specific sub descriptor. It can also be used to abort the configuration
 *  descriptor processing if an incompatible descriptor configuration is found.
 *
 *  This comparator searches for the next Interface descriptor of the correct data Class, Subclass and Protocol values.
 *
 *  \return A value from the DSEARCH_Return_ErrorCodes_t enum
 */
uint8_t DComp_NextDataInterface(void* currentDescriptor) {
    USB_Descriptor_Header_t* header = DESCRIPTOR_PCAST(currentDescriptor, USB_Descriptor_Header_t);
    USB_Descriptor_Interface_t* interface;

    if (header->Type == DTYPE_Interface) {
        interface = DESCRIPTOR_PCAST(currentDescriptor, USB_Descriptor_Interface_t);
        if (interface->TotalEndpoints == 2) {
            return DESCRIPTOR_SEARCH_Found;
        }
    }
    return DESCRIPTOR_SEARCH_NotFound;
}

/** Descriptor comparator function. This comparator function is can be called while processing an attached USB device's
 *  configuration descriptor, to search for a specific sub descriptor. It can also be used to abort the configuration
 *  descriptor processing if an incompatible descriptor configuration is found.
 *
 *  This comparator searches for the next bulk IN or OUT endpoint, or interrupt IN endpoint within the current interface,
 *  aborting the search if another interface descriptor is found before the required endpoint (so that it may be compared
 *  using a different comparator to determine if it is another CDC class interface).
 *
 *  \return A value from the DSEARCH_Return_ErrorCodes_t enum
 */
uint8_t DComp_NextDataInterfaceEndpoint(void* currentDescriptor) {

    USB_Descriptor_Header_t* header = DESCRIPTOR_PCAST(currentDescriptor, USB_Descriptor_Header_t);
    USB_Descriptor_Endpoint_t* endpoint;

    if (header->Type == DTYPE_Endpoint) {
        endpoint = DESCRIPTOR_PCAST(currentDescriptor, USB_Descriptor_Endpoint_t);
        if ((endpoint->Attributes & EP_TYPE_MASK) == EP_TYPE_BULK) {
            return DESCRIPTOR_SEARCH_Found;
        }
    }
    return DESCRIPTOR_SEARCH_NotFound;
}
/*
 * config desc
 * 09 02 20 00 01 01 00 a0 2d 09 04 00 00 02 ff ff 
 * ff 02 07 05 81 02 40 00 00 07 05 02 02 40 00 00
 * 
Device Descriptor:
  bLength                18
  bDescriptorType         1
  bcdUSB               2.00
  bDeviceClass            0 (Defined at Interface level)
  bDeviceSubClass         0 
  bDeviceProtocol         0 
  bMaxPacketSize0         8
  idVendor           0x0403 Future Technology Devices International, Ltd
  idProduct          0x6001 FT232 Serial (UART) IC
  bcdDevice            6.00
  iManufacturer           1 
  iProduct                2 
  iSerial                 3 
  bNumConfigurations      1
  Configuration Descriptor:
    bLength                 9
    bDescriptorType         2
    wTotalLength           32
    bNumInterfaces          1
    bConfigurationValue     1
    iConfiguration          0 
    bmAttributes         0xa0
      (Bus Powered)
      Remote Wakeup
    MaxPower               90mA
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        0
      bAlternateSetting       0
      bNumEndpoints           2
      bInterfaceClass       255 Vendor Specific Class
      bInterfaceSubClass    255 Vendor Specific Subclass
      bInterfaceProtocol    255 Vendor Specific Protocol
      iInterface              2 
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x81  EP 1 IN
        bmAttributes            2
          Transfer Type            Bulk
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0040  1x 64 bytes
        bInterval               0
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x02  EP 2 OUT
        bmAttributes            2
          Transfer Type            Bulk
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0040  1x 64 bytes
        bInterval               0
*/
