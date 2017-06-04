/*
 * main.cpp
 *
 * Copyright 2017 Klaus Gusenleitner <klaus.gusenleitner@gmail.com>
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
#define MAX_LEN_TOPIC        50
#define BUS_RESPONSE_TIMEOUT 100 /* ms */

/*-----------------------------------------------------------------------------
*  Typedefs
*/
typedef enum {
    e_do31_digout = 0,
    e_do31_shader = 1
} T_do31_output_type;

typedef struct {
    char topic[MAX_LEN_TOPIC];  /* the key */
    TBusDevType devtype;
    union {
        struct {
            uint8_t address;
            uint8_t output;
            T_do31_output_type type;
        } do31;
    } io;
    UT_hash_handle hh;    
} T_topic_desc;

typedef struct {
    uint32_t phys_io;  /* the key consists of: device type (8 bit), 
                        *                      device address (8 bit), 
                        *                      device specific data e.g. port number (16 bit) 
                        */
    char topic[MAX_LEN_TOPIC];
    UT_hash_handle hh;    
} T_io_desc;

typedef struct {
    uint32_t phys_dev;  /* the key consists of: device type (8 bit), 
                         *                      device address (8 bit)
                         */
    union {
        TBusDevActualValueDo31 do31;
        TBusDevActualValuePwm4 pwm4;
    } io;
    UT_hash_handle hh;    
} T_dev_desc;

/*-----------------------------------------------------------------------------
*  Variables
*/
static T_topic_desc     *topic_desc;
static T_io_desc        *io_desc;
static T_dev_desc       *dev_desc;
static uint8_t          my_addr = 250;
static struct mosquitto *mosq;

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

void my_connect_callback(struct mosquitto *mq, void *obj, int result) {
    printf("connect\n");
}

void my_disconnect_callback(struct mosquitto *mq, void *obj, int rc) {
    printf("disconnect\n");
}

void my_log_callback(struct mosquitto *mq, void *obj, int level, const char *str) {
    printf("log: %s\n", str);
}

void my_message_callback(struct mosquitto *mq, void *obj, const struct mosquitto_message *message) {

    TBusTelegram    txBusMsg;
    T_topic_desc    *cfg;
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
    T_topic_desc *topic_entry;    
    T_io_desc    *io_entry;    
    T_io_desc    *io_tmp;    
    T_dev_desc   *dev_entry;
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
        topic_entry = (T_topic_desc *)malloc(sizeof(T_topic_desc));
        io_entry = (T_io_desc *)malloc(sizeof(T_io_desc));
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
                topic_entry->io.do31.type = e_do31_digout;
                topic_entry->io.do31.output = (uint8_t)strtoul(physical["digout"].as<std::string>().c_str(), 0, 0);
                io_entry->phys_io |= topic_entry->io.do31.output << 16;
                io_entry->phys_io |= topic_entry->io.do31.type << 24;
            } else if (physical["shader"]) {
                topic_entry->io.do31.type = e_do31_shader;
                topic_entry->io.do31.output = (uint8_t)strtoul(physical["shader"].as<std::string>().c_str(), 0, 0);
                io_entry->phys_io |= topic_entry->io.do31.output << 16;
                io_entry->phys_io |= topic_entry->io.do31.type << 24;
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
            dev_entry = (T_dev_desc *)malloc(sizeof(T_dev_desc));
            dev_entry->phys_dev = type_addr;
            memset(&dev_entry->io, 0, sizeof(dev_entry->io));
            HASH_ADD_INT(dev_desc, phys_dev, dev_entry);
        }
    }
    
    return num_topics;
}

#define MAX_LEN_PAYLOAD 10

static void publish_do31(
    T_dev_desc             *dev_entry, 
    TBusDevActualValueDo31 *av,
    bool                   publish_unconditional
    ) {
    int                    i;
    int                    byteIdx;
    int                    bitPos;
    uint8_t                actval8;
    uint32_t               phys_io;
    T_io_desc              *io_entry;
    char                   topic[MAX_LEN_TOPIC];
    char                   payload[MAX_LEN_PAYLOAD];
    int                    payloadlen;

    /* digout */
    for (i = 0; i < 31; i++) {
        byteIdx = i / 8;
        bitPos = i % 8;    
        if (publish_unconditional ||
            ((dev_entry->io.do31.digOut[byteIdx] & (1 << bitPos)) != 
             (av->digOut[byteIdx] & (1 << bitPos)))) {
            phys_io = dev_entry->phys_dev | (i << 16) | (e_do31_digout << 24);
            HASH_FIND_INT(io_desc, &phys_io, io_entry);
            if (io_entry) {
                actval8 = (av->digOut[byteIdx] & (1 << bitPos)) != 0;
                snprintf(topic, sizeof(topic), "%s/actual", io_entry->topic);
                mosquitto_publish(mosq, 0, topic, 1, actval8 ? "1" : "0", 1, true);
            }
        }
    }
    memcpy(dev_entry->io.do31.digOut, av->digOut, sizeof(dev_entry->io.do31.digOut));

    /* shader */
    for (i = 0; i < 15; i++) {
        if (publish_unconditional ||
            (dev_entry->io.do31.shader[i] != av->shader[i])) {
            phys_io = dev_entry->phys_dev | (i << 16) | (e_do31_shader << 24);
            HASH_FIND_INT(io_desc, &phys_io, io_entry);
            if (io_entry) {
                snprintf(topic, sizeof(topic), "%s/actual", io_entry->topic);
                payloadlen = 0;
                actval8 = av->shader[i];
                if (actval8 <= 100) {
                    payloadlen = snprintf(payload, sizeof(payload), "%d", actval8);
                } else {
                    switch (actval8) {
                    case 252:
                        payloadlen = snprintf(payload, sizeof(payload), "not configured");
                        break;
                    case 253:
                        payloadlen = snprintf(payload, sizeof(payload), "opening");
                        break;
                    case 254:
                        payloadlen = snprintf(payload, sizeof(payload), "closing");
                        break;
                    case 255:
                        payloadlen = snprintf(payload, sizeof(payload), "error");
                        break;
                    default:
                        printf("unsupported shader state %d\n", actval8);
                        break;
                    }
                }
                if (payloadlen) {
                    mosquitto_publish(mosq, 0, topic, payloadlen, payload, 1, true);
                }
            }
        }
    }
    memcpy(dev_entry->io.do31.shader, av->shader, sizeof(dev_entry->io.do31.shader));
}

static void serve_bus(void) {
    uint8_t                     busRet;
    TBusTelegram                *pRxBusMsg;    
    TBusDevReqActualValueEvent  *ave;
    T_dev_desc                  *dev_entry;
    uint32_t                    phys_dev;    
    TBusDevType                 dev_type;

    busRet = BusCheck();
    if (busRet != BUS_MSG_OK) {
        return;
    }
    pRxBusMsg = BusMsgBufGet();
    if ((pRxBusMsg->type != eBusDevReqActualValueEvent) ||
        ((pRxBusMsg->msg.devBus.receiverAddr != my_addr))) {
        return;
    }
    ave = &pRxBusMsg->msg.devBus.x.devReq.actualValueEvent;
    dev_type = ave->devType;
    /* find changed io  */
    phys_dev = dev_type + (pRxBusMsg->senderAddr << 8);
    HASH_FIND_INT(dev_desc, &phys_dev, dev_entry);
    if (!dev_entry) {
        return;
    }
    if (phys_dev != dev_entry->phys_dev) {
        printf("configuration mismatch at address %d: conf %d, recv %d\n", pRxBusMsg->senderAddr, dev_entry->phys_dev & 0xff, dev_type);
    }
    switch (dev_type) {
    case eBusDevTypeDo31:
        publish_do31(dev_entry, &ave->actualValue.do31, false);
        break;
    default:
        break;
    }
}

static unsigned long get_tick_count(void) {

    struct timespec ts;
    unsigned long time_ms;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    time_ms = (unsigned long)((unsigned long long)ts.tv_sec * 1000ULL + 
              (unsigned long long)ts.tv_nsec / 1000000ULL);

    return time_ms;
}

static TBusTelegram *request_actval(uint8_t address) {
    
    TBusTelegram    tx_msg;
    TBusTelegram    *rx_msg;
    uint8_t         ret;
    unsigned long   start_time;
    unsigned long   cur_time;
    bool            response_ok = false;
    bool            timeout = false;

    tx_msg.type = eBusDevReqActualValue;
    tx_msg.senderAddr = my_addr;
    tx_msg.msg.devBus.receiverAddr = address;
    BusSend(&tx_msg);
    start_time = get_tick_count();
    do {
        cur_time = get_tick_count();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            rx_msg = BusMsgBufGet();
            if ((rx_msg->type == eBusDevRespActualValue)     &&
                (rx_msg->msg.devBus.receiverAddr == my_addr) &&
                (rx_msg->senderAddr == address)) {
                response_ok = true;
            }
        } else {
            if ((cur_time - start_time) > BUS_RESPONSE_TIMEOUT) {
                timeout = true;
            }
        }
        usleep(10000);
    } while (!response_ok && !timeout);
    
    return response_ok ? rx_msg : 0;
}
    
static int init_state(void) {
    
    T_dev_desc             *dev_entry;
    T_dev_desc             *dev_tmp;
    uint8_t                address;
    uint8_t                dev_type;
    TBusTelegram           *rx_msg;
    TBusDevRespActualValue *av;    
    
    int rc = -1;
   
    HASH_ITER(hh, dev_desc, dev_entry, dev_tmp) {
        dev_type = dev_entry->phys_dev & 0xff;
        address = (dev_entry->phys_dev >> 8) & 0xff;
        rx_msg = request_actval(address);
        if (rx_msg) {
            av = &rx_msg->msg.devBus.x.devResp.actualValue;
            if (dev_type != av->devType) {
                printf("configuartion error devType of %d invalid\n", address);
                break;
            }
            
            switch (dev_type) {
            case eBusDevTypeDo31:
printf("publish init state: DO31 at %d\n", address);
                publish_do31(dev_entry, &av->actualValue.do31, true);
                break;
            case eBusDevTypePwm4:
                break;
            default:
                break;
            }
        }
    }
    return rc;
}

/*-----------------------------------------------------------------------------
*  program start
*/
int main(int argc, char *argv[]) {

    int              busFd;
    int              mosqFd;
    int              maxFd;
    int              busHandle;
    fd_set           rfds;
    int              ret;

    struct timeval   tv;
    char             topic[MAX_LEN_TOPIC];
    T_topic_desc     *topic_entry;
    T_topic_desc     *topic_tmp;
    T_dev_desc       *dev_entry;
    T_dev_desc       *dev_tmp;
    T_io_desc        *io_entry;
    T_io_desc        *io_tmp;

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

    init_state();


    for (int i = 0; i < 100; i++) {
       usleep(100000);
       mosquitto_loop_misc(mosq);
       mosquitto_loop_read(mosq, 1);
    }

    /* subscribe to all configured topic extended by 'set' */
    HASH_ITER(hh, topic_desc, topic_entry, topic_tmp) {
        snprintf(topic, sizeof(topic), "%s/set", topic_entry->topic);
        mosquitto_subscribe(mosq, 0, topic, 1);
    }

    FD_ZERO(&rfds);
    for (;;) {
        FD_SET(busFd, &rfds);
        FD_SET(mosqFd, &rfds);
        FD_SET(STDIN_FILENO, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        ret = select(maxFd + 1, &rfds, 0, 0, &tv);
        if ((ret > 0) && FD_ISSET(busFd, &rfds)) {
            serve_bus();
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
