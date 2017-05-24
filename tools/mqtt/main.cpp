/*
 * main.cpp
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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <mosquitto.h>

#include <uthash.h>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
#define MAX_LEN_TOPIC 50

/*-----------------------------------------------------------------------------
*  Typedefs
*/

typedef struct {
    char topic[MAX_LEN_TOPIC];  /* the key */
    TBusDevType devtype;
    union {
        struct {
            uint8_t address;
            uint8_t output;
        } do31;
    } io;
    UT_hash_handle hh;    
} TTopicDesc;

typedef struct {
    uint32_t phys_io;  /* the key consists of: device type (8 bit), 
                        *                      device address (8 bit), 
                        *                      device specific data e.g. port number (16 bit) 
                        */
    char topic[MAX_LEN_TOPIC];
    UT_hash_handle hh;    
} TIoDesc;

typedef struct {
    uint32_t phys_dev;  /* the key consists of: device type (8 bit), 
                         *                      device address (8 bit)
                         */
    union {
        TBusDevActualValueDo31 do31;
    } io;
    UT_hash_handle hh;    
} TDevDesc;

/*-----------------------------------------------------------------------------
*  Variables
*/
static TTopicDesc *topic_desc;
static TIoDesc    *io_desc;
static TDevDesc   *dev_desc;

/*-----------------------------------------------------------------------------
*  Functions
*/

/*-----------------------------------------------------------------------------
*  sio open and bus init
*/
static int InitBus(const char *comPort) {

    uint8_t ch;
    int     handle;

    SioInit();
    handle = SioOpen(comPort, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);
    if (handle == -1) {
        return -1;
    }
    while (SioGetNumRxChar(handle) > 0) {
        SioRead(handle, &ch, sizeof(ch));
    }
    BusInit(handle);

    return handle;
}

void my_connect_callback(struct mosquitto *mosq, void *obj, int result) {
    printf("connect\n");
}

void my_disconnect_callback(struct mosquitto *mosq, void *obj, int rc) {
    printf("disconnect\n");
}

void my_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str) {
    printf("log: %s\n", str);
}

void my_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {

    TBusTelegram    txBusMsg;
    TTopicDesc      *cfg;
    int             byteIdx;
    int             bitPos;
    char            topic[MAX_LEN_TOPIC];
    char            *ch;
    int             len;
    
    ch = strrchr(message->topic, '/');
    len = ch - message->topic;
    memcpy(topic, message->topic, len);
    topic[len] = '\0';
    HASH_FIND_STR(topic_desc, topic, cfg);
    if (!cfg) {
        printf("error: topic %s not found\n", message->topic);
        return;
    }
    
    txBusMsg.type = eBusDevReqSetValue;
    txBusMsg.senderAddr = 250;
    txBusMsg.msg.devBus.receiverAddr = cfg->io.do31.address;
    txBusMsg.msg.devBus.x.devReq.setValue.devType = cfg->devtype;
    memset(&txBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.digOut, 0, sizeof(txBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.digOut));
    memset(&txBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.shader, 254, sizeof(txBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.shader));

    /* calculate the bit position (2 bits for each output) */
    byteIdx = cfg->io.do31.output / 4;
    bitPos = (cfg->io.do31.output % 4) * 2;
    if (*(char *)(message->payload) == '0') {
        txBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.digOut[byteIdx] = 2 << bitPos;
    } else {
        txBusMsg.msg.devBus.x.devReq.setValue.setValue.do31.digOut[byteIdx] = 3 << bitPos;
    }
    BusSend(&txBusMsg);
}

static int ReadConfig(const char *pFile)  {

    int num_topics = 0;
    TTopicDesc *topic_entry;    
    TIoDesc    *io_entry;    
    TIoDesc    *io_tmp;    
    TDevDesc   *dev_entry;
    uint32_t   type_addr;
    YAML::Node ymlcfg = YAML::LoadFile(pFile);

    topic_desc = 0;
    io_desc = 0;
    for (YAML::iterator it = ymlcfg.begin(); it != ymlcfg.end(); ++it) {
        const YAML::Node& node = *it;
//        std::cout << "topic: " << node["topic"].as<std::string>() << "\n";
//        printf("%s\n", node["topic"].as<std::string>().c_str());
        
        const YAML::Node& physical = node["physical"];
/*
        if (physical["type"]) {
            std::cout << "type: " << physical["type"].as<std::string>() << "\n";
        }
        std::cout << "addr: " << physical["address"].as<std::string>() << "\n";
        std::cout << "do  : " << physical["digout"].as<std::string>() << "\n";

        unsigned long i = strtoul(physical["digout"].as<std::string>().c_str(), 0, 0);
        printf("%lu\n", i);
*/
        topic_entry = (TTopicDesc *)malloc(sizeof(TTopicDesc));
        io_entry = (TIoDesc *)malloc(sizeof(TIoDesc));
        if (!topic_entry || !io_entry) {
            break;
        }
        io_entry->phys_io = 0;
        if (node["topic"]) {
            snprintf(topic_entry->topic, sizeof(topic_entry->topic), "%s", node["topic"].as<std::string>().c_str());
            snprintf(io_entry->topic, sizeof(io_entry->topic), "%s", topic_entry->topic);
        } else {
            printf("topic missing\n");
            break;
        }
        if (physical["type"] && (physical["type"].as<std::string>().compare("do31") == 0)) {
            /* for DO31 */
            topic_entry->devtype = eBusDevTypeDo31;
            io_entry->phys_io = (uint8_t)topic_entry->devtype;
            if (physical["address"]) {
                topic_entry->io.do31.address = (uint8_t)strtoul(physical["address"].as<std::string>().c_str(), 0, 0);
                io_entry->phys_io |= (topic_entry->io.do31.address << 8);
            } else {
               printf("missing address at topic %s\n", node["topic"].as<std::string>().c_str());
               break;
            }
            if (physical["digout"]) {
                topic_entry->io.do31.output = (uint8_t)strtoul(physical["digout"].as<std::string>().c_str(), 0, 0);
                io_entry->phys_io |= (topic_entry->io.do31.output << 16);
            } else {
               printf("missing digout at topic %s\n", node["topic"].as<std::string>().c_str());
               break;
            }
        } else {
            printf("unknown or missing type at topic %s\n", node["topic"].as<std::string>().c_str());
            break;
        }
        HASH_ADD_STR(topic_desc, topic, topic_entry);
        HASH_ADD_INT(io_desc, phys_io, io_entry);
        num_topics++;
    }
    
    /* create a device table */
    dev_desc = 0;
    HASH_ITER(hh, io_desc, io_entry, io_tmp) {
        type_addr = io_entry->phys_io & 0xffff;
        HASH_FIND_INT(dev_desc, &type_addr, dev_entry);
        if (!dev_entry) {
            dev_entry = (TDevDesc *)malloc(sizeof(TDevDesc));
            dev_entry->phys_dev = type_addr;
            memset(&dev_entry->io, 0, sizeof(dev_entry->io));
            HASH_ADD_INT(dev_desc, phys_dev, dev_entry);
        }
    }
    
    return num_topics;
}

/*-----------------------------------------------------------------------------
*  program start
*/
int main(int argc, char *argv[]) {

    struct mosquitto *mosq = 0;
    int              busFd;
    int              mosqFd;
    int              maxFd;
    int              busHandle;
    fd_set           rfds;
    int              ret;
    TBusTelegram     *pRxBusMsg;
    uint8_t          busRet;
    uint8_t          myAddr = 250; 
    struct timeval   tv;
    char             topic[MAX_LEN_TOPIC];
    int              byteIdx;
    int              bitPos;
    uint8_t          actval8;
    TTopicDesc       *topic_entry;
    TTopicDesc       *topic_tmp;
    TDevDesc         *dev_entry;
    TDevDesc         *dev_tmp;
    TIoDesc          *io_entry;
    TIoDesc          *io_tmp;
    uint32_t         phys_io;
    uint32_t         phys_dev;
    int              i;
    TBusTelegram     txBusMsg;

    mosquitto_lib_init();
    mosq = mosquitto_new("bus-client", true, 0);
    if (!mosq) {
        printf("mosq 0\n");
        return 0;
    }

    mosquitto_connect_callback_set(mosq, my_connect_callback);
    mosquitto_disconnect_callback_set(mosq, my_disconnect_callback);
    mosquitto_log_callback_set(mosq, my_log_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);


    ret = mosquitto_connect(mosq, "10.0.0.200", 1883, 60);
//    printf("connect ret %d\n", ret);

    busHandle = InitBus("/dev/hausbus");
    if (busHandle == -1) {
        printf("cannot open\n");
        return -1;
    }
    busFd = SioGetFd(busHandle);
    maxFd = busFd;
    mosqFd = mosquitto_socket(mosq);
    if (mosqFd > busFd) {
        maxFd = mosqFd;
    } 
    ReadConfig("config.yaml");

    HASH_ITER(hh, topic_desc, topic_entry, topic_tmp) {
        printf("topic %s %d %d %d\n", topic_entry->topic, topic_entry->devtype,  topic_entry->io.do31.address, topic_entry->io.do31.output);
    }
    HASH_ITER(hh, dev_desc, dev_entry, dev_tmp) {
        printf("dev %04x %02x %02x %02x %02x\n", dev_entry->phys_dev, dev_entry->io.do31.digOut[0],
                                                                      dev_entry->io.do31.digOut[1],
                                                                      dev_entry->io.do31.digOut[2],
                                                                      dev_entry->io.do31.digOut[3]);
    }
    HASH_ITER(hh, io_desc, io_entry, io_tmp) {
        printf("io %04x %s\n", io_entry->phys_io, io_entry->topic);
    }

    /* subscribe to all configured topic extended by 'set' */
    HASH_ITER(hh, topic_desc, topic_entry, topic_tmp) {
        snprintf(topic, sizeof(topic), "%s/set", topic_entry->topic);
        mosquitto_subscribe(mosq, 0, topic, 1);
    }

    txBusMsg.type = eBusDevReqActualValue;
    txBusMsg.senderAddr = 250;
    txBusMsg.msg.devBus.receiverAddr = 240;
    BusSend(&txBusMsg);
    
    txBusMsg.type = eBusDevReqActualValue;
    txBusMsg.senderAddr = 250;
    txBusMsg.msg.devBus.receiverAddr = 241;
    BusSend(&txBusMsg);


 
    FD_ZERO(&rfds);
    for (;;) {
        FD_SET(busFd, &rfds);
        FD_SET(mosqFd, &rfds);
        FD_SET(STDIN_FILENO, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        ret = select(maxFd + 1, &rfds, 0, 0, &tv);
        if ((ret > 0) && FD_ISSET(busFd, &rfds)) {
            busRet = BusCheck();
            if (busRet == BUS_MSG_OK) {
                pRxBusMsg = BusMsgBufGet();
                if (((pRxBusMsg->type == eBusDevReqActualValueEvent) || (pRxBusMsg->type == eBusDevRespActualValue)) &&
                    ((pRxBusMsg->msg.devBus.receiverAddr == myAddr))) {
                    phys_dev = pRxBusMsg->msg.devBus.x.devReq.actualValueEvent.devType + (pRxBusMsg->senderAddr << 8);
                    HASH_FIND_INT(dev_desc, &phys_dev, dev_entry);
                    if (dev_entry) {
                        /* find changed io */
                        /* do31 do */
                        for (i = 0; i < 31; i++) {
                            byteIdx = i / 8;
                            bitPos = i % 8;                                
                            if ((dev_entry->io.do31.digOut[byteIdx] & (1 << bitPos)) != 
                                (pRxBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.digOut[byteIdx] & (1 << bitPos))) {
                                actval8 = (pRxBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.digOut[byteIdx] & (1 << bitPos)) != 0;
                                phys_io = phys_dev | (i << 16);
                                HASH_FIND_INT(io_desc, &phys_io, io_entry);
                                if (io_entry) {
                                    snprintf(topic, sizeof(topic), "%s/actual", io_entry->topic);
                                    mosquitto_publish(mosq, 0, topic, 1, actval8 ? "1" : "0", 1, true);
                                }
                            }
                        }
                        memcpy(dev_entry->io.do31.digOut, pRxBusMsg->msg.devBus.x.devReq.actualValueEvent.actualValue.do31.digOut, 4);
                    }
                }
            }
        }
        if ((ret > 0) && FD_ISSET(mosqFd, &rfds)) {
//printf("mosq rfds\n");            
            mosquitto_loop_read(mosq, 1);
        }
//printf("mosq loop misc\n");            
        mosquitto_loop_misc(mosq);
    }

    mosquitto_lib_cleanup();
}
