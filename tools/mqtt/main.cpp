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


/* todo
 * - pwm4 pwm support
 * - evaluate eBusDevRespSetValue for quick input response (response to eBusDevReqSetValue in my_message_callback)
 *
 * */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>

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
#define MAX_LEN_TOPIC        60
#define MAX_LEN_TOPIC_DESC   50
#define BUS_RESPONSE_TIMEOUT 100 /* ms */

#define PATH_LEN             255

/*-----------------------------------------------------------------------------
*  Typedefs
*/
typedef enum {
    e_do31_digout = 0,
    e_do31_shader = 1
} T_do31_output_type;

typedef enum {
    e_sw8_digin = 0,
    e_sw8_digout = 1,
    e_sw8_pulseout = 2
} T_sw8_port_type;

typedef struct {
    char topic[MAX_LEN_TOPIC_DESC];  /* the key */
    TBusDevType devtype;
    union {
        struct {
            uint8_t address;
            uint8_t output;
            T_do31_output_type type;
        } do31;
        struct {
            uint8_t address;
            uint8_t output;
        } pwm4;
        struct {
            uint8_t address;
            uint8_t port;
            T_sw8_port_type type;
        } sw8;
    } io;
    UT_hash_handle hh;
} T_topic_desc;

typedef struct {
    uint32_t phys_io;  /* the key consists of: device type (8 bit),
                        *                      device address (8 bit),
                        *                      device specific data e.g. port number (16 bit)
                        */
    char topic[MAX_LEN_TOPIC_DESC];
    UT_hash_handle hh;
} T_io_desc;

typedef struct {
    uint32_t phys_dev;  /* the key consists of: device type (8 bit),
                         *                      device address (8 bit)
                         */
    union {
        TBusDevActualValueDo31 do31;
        TBusDevActualValuePwm4 pwm4;
        TBusDevActualValueSw8  sw8;
    } io;
    UT_hash_handle hh;
} T_dev_desc;

/*-----------------------------------------------------------------------------
*  Variables
*/
static T_topic_desc     *topic_desc;
static T_io_desc        *io_desc;
static T_dev_desc       *dev_desc;
static uint8_t          my_addr;
static uint8_t          event_addr;
static struct mosquitto *mosq;
static bool             mosq_connected;

/*-----------------------------------------------------------------------------
*  Functions
*/


/*-----------------------------------------------------------------------------
*  get the current time in ms
*/
static unsigned long get_tick_count(void) {

    struct timespec ts;
    unsigned long time_ms;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    time_ms = (unsigned long)((unsigned long long)ts.tv_sec * 1000ULL +
              (unsigned long long)ts.tv_nsec / 1000000ULL);

    return time_ms;
}

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

    mosq_connected = true;
    printf("cb connect\n");
}

void my_disconnect_callback(struct mosquitto *mq, void *obj, int rc) {

    mosq_connected = false;
    printf("cb disconnect\n");
}

void my_log_callback(struct mosquitto *mq, void *obj, int level, const char *str) {
//    printf("log: %s\n", str);
}

/*-----------------------------------------------------------------------------
*  set a DO31 output using ReqSetValue telegram
*/
static int do31_ReqSetValue(uint8_t addr, uint8_t *digout, uint8_t *shader) {

    TBusTelegram        tx_msg;
    TBusTelegram        *rx_msg;
    unsigned long       start_time;
    unsigned long       cur_time;
    uint8_t             ret;
    TBusDevSetValueDo31 *sv;
    bool                response_ok = false;
    bool                timeout = false;

    tx_msg.type = eBusDevReqSetValue;
    tx_msg.senderAddr = my_addr;
    tx_msg.msg.devBus.receiverAddr = addr;
    tx_msg.msg.devBus.x.devReq.setValue.devType = eBusDevTypeDo31;
    sv = &tx_msg.msg.devBus.x.devReq.setValue.setValue.do31;
    memcpy(sv->digOut, digout, sizeof(sv->digOut));
    memcpy(sv->shader, shader, sizeof(sv->shader));

    BusSend(&tx_msg);

    /* response */
    start_time = get_tick_count();
    do {
        cur_time = get_tick_count();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            rx_msg = BusMsgBufGet();
            if ((rx_msg->type == eBusDevRespSetValue)     &&
                (rx_msg->msg.devBus.receiverAddr == my_addr) &&
                (rx_msg->senderAddr == addr)) {
                response_ok = true;
            }
        } else {
            if ((cur_time - start_time) > BUS_RESPONSE_TIMEOUT) {
                timeout = true;
            }
        }
        usleep(10000);
    } while (!response_ok && !timeout);

    return response_ok ? 0 : -1;
}

/*-----------------------------------------------------------------------------
*  set a DO31 output
*/
static void do31_set_output(uint8_t address, uint8_t output, T_do31_output_type type, uint8_t value) {

    int        byteIdx;
    int        bitPos;
    uint8_t    digout[BUS_DO31_DIGOUT_SIZE_SET_VALUE];
    uint8_t    shader[BUS_DO31_SHADER_SIZE_SET_VALUE];
    const char *str = "";

    memset(digout, 0, sizeof(digout));
    memset(shader, 254, sizeof(shader));
    if (type == e_do31_digout) {
        /* calculate the bit position (2 bits for each output) */
        byteIdx = output / 4;
        bitPos = (output % 4) * 2;
        if (value == 0) {
            digout[byteIdx] = 2 << bitPos;
        } else {
            digout[byteIdx] = 3 << bitPos;
        }
        str = "DO";
    } else if (type == e_do31_shader) {
        shader[output] = value;
        str = "SH";
    }
    if (do31_ReqSetValue(address, digout, shader) != 0) {
        syslog(LOG_ERR, "DO31 %d: can't set value %s%d to %d", address, str, output, value);
    }
}

/*-----------------------------------------------------------------------------
*  set a PWM4 pwm output using ReqSetValue telegram
*/
static int pwm4_ReqSetValue(uint8_t addr, uint8_t set, uint8_t *pwm) {

    TBusTelegram        tx_msg;
    TBusTelegram        *rx_msg;
    unsigned long       start_time;
    unsigned long       cur_time;
    uint8_t             ret;
    TBusDevSetValuePwm4 *sv;
    bool                response_ok = false;
    bool                timeout = false;

    tx_msg.type = eBusDevReqSetValue;
    tx_msg.senderAddr = my_addr;
    tx_msg.msg.devBus.receiverAddr = addr;
    tx_msg.msg.devBus.x.devReq.setValue.devType = eBusDevTypePwm4;
    sv = &tx_msg.msg.devBus.x.devReq.setValue.setValue.pwm4;
    sv->set = set;
    memcpy(sv->pwm, pwm, sizeof(sv->pwm));

    BusSend(&tx_msg);

    /* response */
    start_time = get_tick_count();
    do {
        cur_time = get_tick_count();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            rx_msg = BusMsgBufGet();
            if ((rx_msg->type == eBusDevRespSetValue)     &&
                (rx_msg->msg.devBus.receiverAddr == my_addr) &&
                (rx_msg->senderAddr == addr)) {
                response_ok = true;
            }
        } else {
            if ((cur_time - start_time) > BUS_RESPONSE_TIMEOUT) {
                timeout = true;
            }
        }
        usleep(10000);
    } while (!response_ok && !timeout);

    return response_ok ? 0 : -1;
}

/*-----------------------------------------------------------------------------
*  set a PWM4 output
*/
static void pwm4_set_output(uint8_t address, uint8_t output, bool on) {

    int        bitPos;
    uint8_t    set;
    uint8_t    pwm[BUS_PWM4_PWM_SIZE_SET_VALUE];

    set = 0;
    memset(pwm, 0, sizeof(pwm));
    /* calculate the bit position (2 bits for each output) */
    bitPos = output * 2;
    if (on) {
       set = 1 << bitPos;
    } else {
       set = 3 << bitPos;
    }
    if (pwm4_ReqSetValue(address, set, pwm) != 0) {
        syslog(LOG_ERR, "PWM4 %d: can't set value %d to %d", address, output, on);
    }
}

/*-----------------------------------------------------------------------------
*  set a SW8 output using ReqSetValue telegram
*/
static int sw8_ReqSetValue(uint8_t addr, uint8_t *digout) {

    TBusTelegram        tx_msg;
    TBusTelegram        *rx_msg;
    unsigned long       start_time;
    unsigned long       cur_time;
    uint8_t             ret;
    TBusDevSetValueSw8  *sv;
    bool                response_ok = false;
    bool                timeout = false;

    tx_msg.type = eBusDevReqSetValue;
    tx_msg.senderAddr = my_addr;
    tx_msg.msg.devBus.receiverAddr = addr;
    tx_msg.msg.devBus.x.devReq.setValue.devType = eBusDevTypeSw8;
    sv = &tx_msg.msg.devBus.x.devReq.setValue.setValue.sw8;
    memcpy(sv->digOut, digout, sizeof(sv->digOut));

    BusSend(&tx_msg);

    /* response */
    start_time = get_tick_count();
    do {
        cur_time = get_tick_count();
        ret = BusCheck();
        if (ret == BUS_MSG_OK) {
            rx_msg = BusMsgBufGet();
            if ((rx_msg->type == eBusDevRespSetValue)     &&
                (rx_msg->msg.devBus.receiverAddr == my_addr) &&
                (rx_msg->senderAddr == addr)) {
                response_ok = true;
            }
        } else {
            if ((cur_time - start_time) > BUS_RESPONSE_TIMEOUT) {
                timeout = true;
            }
        }
        usleep(10000);
    } while (!response_ok && !timeout);

    return response_ok ? 0 : -1;
}

/*-----------------------------------------------------------------------------
*  set a SW8 output
*/
static void sw8_set_output(uint8_t address, uint8_t output, T_sw8_port_type type, uint8_t value) {

    int        byteIdx;
    int        bitPos;
    uint8_t    digout[BUS_SW8_DIGOUT_SIZE_SET_VALUE];

    memset(digout, 0, sizeof(digout));
    /* calculate the bit position (2 bits for each output) */
    byteIdx = output / 4;
    bitPos = (output % 4) * 2;
    switch (type) {
    case e_sw8_digout:
        if (value == 0) {
            digout[byteIdx] = 2 << bitPos;
        } else {
            digout[byteIdx] = 3 << bitPos;
        }
        break;
    case e_sw8_pulseout:
        if (value != 0) {
            digout[byteIdx] = 1 << bitPos;
        }
        break;
    default:
        break;
    }
    if (sw8_ReqSetValue(address, digout) != 0) {
        syslog(LOG_ERR, "SW8 %d: can't set value %d to %d", address, output, value);
    }
}

/*-----------------------------------------------------------------------------
*  subscription callback for .../set
*/
static void my_message_callback(struct mosquitto *mq, void *obj, const struct mosquitto_message *message) {

    T_topic_desc *cfg;
    char         topic[MAX_LEN_TOPIC];
    char         *ch;
    int          len;
    uint8_t      value8;

printf("subscribed: %s %s\n", message->topic, (char *)message->payload);

    ch = strrchr(message->topic, '/');
    len = ch - message->topic;
    memcpy(topic, message->topic, len);
    topic[len] = '\0';
    HASH_FIND_STR(topic_desc, topic, cfg);
    if (!cfg) {
        printf("error: topic %s not found\n", message->topic);
        return;
    }

    switch(cfg->devtype) {
    case eBusDevTypeDo31:
        value8 = (uint8_t)strtoul((char *)message->payload, 0, 0);
        do31_set_output(cfg->io.do31.address, cfg->io.do31.output, cfg->io.do31.type, value8);
        break;
    case eBusDevTypePwm4:
        value8 = (uint8_t)strtoul((char *)message->payload, 0, 0);
        pwm4_set_output(cfg->io.pwm4.address, cfg->io.pwm4.output, value8 != 0);
        break;
    case eBusDevTypeSw8:
        value8 = (uint8_t)strtoul((char *)message->payload, 0, 0);
        sw8_set_output(cfg->io.sw8.address, cfg->io.sw8.port, cfg->io.sw8.type, value8 != 0);
        break;
    default:
        break;
    }
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
            /* DO31 */
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
                printf("missing digout or shader at topic %s\n", node["topic"].as<std::string>().c_str());
                break;
            }
        } else if (physical["type"] && (physical["type"].as<std::string>().compare("pwm4") == 0)) {
            /* PWM4 */
            topic_entry->devtype = eBusDevTypePwm4;
            io_entry->phys_io = (uint8_t)topic_entry->devtype;
            if (physical["address"]) {
                topic_entry->io.pwm4.address = (uint8_t)strtoul(physical["address"].as<std::string>().c_str(), 0, 0);
                io_entry->phys_io |= (topic_entry->io.pwm4.address << 8);
            } else {
               printf("missing address at topic %s\n", node["topic"].as<std::string>().c_str());
               break;
            }
            if (physical["pwmout"]) {
                topic_entry->io.pwm4.output = (uint8_t)strtoul(physical["pwmout"].as<std::string>().c_str(), 0, 0);
                io_entry->phys_io |= topic_entry->io.pwm4.output << 16;
            } else {
                printf("missing pwmout at topic %s\n", node["topic"].as<std::string>().c_str());
                break;
            }
        } else if (physical["type"] && (physical["type"].as<std::string>().compare("sw8") == 0)) {
            /* SW8 */
            topic_entry->devtype = eBusDevTypeSw8;
            io_entry->phys_io = (uint8_t)topic_entry->devtype;
            if (physical["address"]) {
                topic_entry->io.sw8.address = (uint8_t)strtoul(physical["address"].as<std::string>().c_str(), 0, 0);
                io_entry->phys_io |= (topic_entry->io.sw8.address << 8);
            } else {
               printf("missing address at topic %s\n", node["topic"].as<std::string>().c_str());
               break;
            }
            if (physical["digin"]) {
                topic_entry->io.sw8.port = (uint8_t)strtoul(physical["digin"].as<std::string>().c_str(), 0, 0);
                topic_entry->io.sw8.type = e_sw8_digin;
                io_entry->phys_io |= topic_entry->io.sw8.port << 16;
                io_entry->phys_io |= topic_entry->io.sw8.type << 24;
            } else if (physical["digout"]) {
                topic_entry->io.sw8.port = (uint8_t)strtoul(physical["digout"].as<std::string>().c_str(), 0, 0);
                topic_entry->io.sw8.type = e_sw8_digout;
                io_entry->phys_io |= topic_entry->io.sw8.port << 16;
                io_entry->phys_io |= topic_entry->io.sw8.type << 24;
            } else if (physical["pulseout"]) {
                topic_entry->io.sw8.port = (uint8_t)strtoul(physical["pulseout"].as<std::string>().c_str(), 0, 0);
                topic_entry->io.sw8.type = e_sw8_pulseout;
                io_entry->phys_io |= topic_entry->io.sw8.port << 16;
                io_entry->phys_io |= topic_entry->io.sw8.type << 24;
            } else {
                printf("missing digin/digout/pulseout at topic %s\n", node["topic"].as<std::string>().c_str());
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

#define MAX_LEN_PAYLOAD 20

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
printf("publish %s %d\n", topic, actval8);
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
                        payloadlen = snprintf(payload, sizeof(payload), "closing");
                        break;
                    case 254:
                        payloadlen = snprintf(payload, sizeof(payload), "opening");
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
printf("publish %s %s\n", topic, payload);
                    mosquitto_publish(mosq, 0, topic, payloadlen, payload, 1, true);
                }
            }
        }
    }
    memcpy(dev_entry->io.do31.shader, av->shader, sizeof(dev_entry->io.do31.shader));
}

static void publish_pwm4(
    T_dev_desc             *dev_entry,
    TBusDevActualValuePwm4 *av,
    bool                   publish_unconditional
    ) {
    int                    i;
    int                    bitPos;
    uint8_t                actval8;
    uint32_t               phys_io;
    T_io_desc              *io_entry;
    char                   topic[MAX_LEN_TOPIC];

    for (i = 0; i < 4; i++) {
        bitPos = i;
        if (publish_unconditional ||
            ((dev_entry->io.pwm4.state & (1 << bitPos)) != (av->state & (1 << bitPos)))) {
            phys_io = dev_entry->phys_dev | (i << 16);
            HASH_FIND_INT(io_desc, &phys_io, io_entry);
            if (io_entry) {
                actval8 = (av->state & (1 << bitPos)) != 0;
                snprintf(topic, sizeof(topic), "%s/actual", io_entry->topic);
printf("publish %s %d\n", topic, actval8);
                mosquitto_publish(mosq, 0, topic, 1, actval8 ? "1" : "0", 1, true);
            }
        }
    }
    dev_entry->io.pwm4.state = av->state;
}

static void publish_sw8(
    T_dev_desc             *dev_entry,
    TBusDevActualValueSw8  *av,
    bool                   publish_unconditional
    ) {
    int                    i;
    int                    bitPos;
    uint8_t                actval8;
    uint32_t               phys_io;
    T_io_desc              *io_entry;
    char                   topic[MAX_LEN_TOPIC];

    /* digin */
    for (i = 0; i < 8; i++) {
        bitPos = i;
        if (publish_unconditional ||
            ((dev_entry->io.sw8.state & (1 << bitPos)) != (av->state & (1 << bitPos)))) {
            phys_io = dev_entry->phys_dev | (i << 16) | (e_sw8_digin << 24);
            HASH_FIND_INT(io_desc, &phys_io, io_entry);
            if (io_entry) {
                actval8 = (av->state & (1 << bitPos)) != 0;
                snprintf(topic, sizeof(topic), "%s/actual", io_entry->topic);
printf("publish %s %d\n", topic, actval8);
                mosquitto_publish(mosq, 0, topic, 1, actval8 ? "1" : "0", 1, true);
            }
        }
    }
    /* digout */
    for (i = 0; i < 8; i++) {
        bitPos = i;
        if (publish_unconditional ||
            ((dev_entry->io.sw8.state & (1 << bitPos)) != (av->state & (1 << bitPos)))) {
            phys_io = dev_entry->phys_dev | (i << 16) | (e_sw8_digout << 24);
            HASH_FIND_INT(io_desc, &phys_io, io_entry);
            if (io_entry) {
                actval8 = (av->state & (1 << bitPos)) != 0;
                snprintf(topic, sizeof(topic), "%s/actual", io_entry->topic);
printf("publish %s %d\n", topic, actval8);
                mosquitto_publish(mosq, 0, topic, 1, actval8 ? "1" : "0", 1, true);
            }
        }
    }
    /* pulseout */
    for (i = 0; i < 8; i++) {
        bitPos = i;
        if (publish_unconditional ||
            ((dev_entry->io.sw8.state & (1 << bitPos)) != (av->state & (1 << bitPos)))) {
            phys_io = dev_entry->phys_dev | (i << 16) | (e_sw8_pulseout << 24);
            HASH_FIND_INT(io_desc, &phys_io, io_entry);
            if (io_entry) {
                actval8 = (av->state & (1 << bitPos)) != 0;
                snprintf(topic, sizeof(topic), "%s/actual", io_entry->topic);
printf("publish %s %d\n", topic, actval8);
                mosquitto_publish(mosq, 0, topic, 1, actval8 ? "1" : "0", 1, true);
            }
        }
    }
    dev_entry->io.sw8.state = av->state;
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
        ((pRxBusMsg->msg.devBus.receiverAddr != event_addr))) {
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
    case eBusDevTypePwm4:
        publish_pwm4(dev_entry, &ave->actualValue.pwm4, false);
        break;
    case eBusDevTypeSw8:
        publish_sw8(dev_entry, &ave->actualValue.sw8, false);
        break;
    default:
        break;
    }
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
                printf("configuration error devType of %d invalid\n", address);
                break;
            }

            switch (dev_type) {
            case eBusDevTypeDo31:
printf("publish init state: DO31 at %d\n", address);
                publish_do31(dev_entry, &av->actualValue.do31, true);
                break;
            case eBusDevTypePwm4:
printf("publish init state: PWM4 at %d\n", address);
                publish_pwm4(dev_entry, &av->actualValue.pwm4, true);
                break;
            case eBusDevTypeSw8:
printf("publish init state: SW8 at %d\n", address);
                publish_sw8(dev_entry, &av->actualValue.sw8, true);
                break;
            default:
                break;
            }
        }
    }
    return rc;
}

/*-----------------------------------------------------------------------------
*  show help
*/
static void print_usage(void) {

   printf("\nUsage:\n");
   printf("mqtt -c sio-port -a bus-address -f yaml-cfg -e event-listen-bus-address -m mqtt-broker-ip [-p mqtt-port]\n");
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
    int              i;
    char             com_port[PATH_LEN] = "";
    char             config[PATH_LEN] = "";
    char             broker[PATH_LEN] = "";
    int              port = 1883; /* default */
    bool             my_addr_valid = false;
    bool             event_addr_valid = false;
    struct timeval   tv;
    char             topic[MAX_LEN_TOPIC];
    T_topic_desc     *topic_entry;
    T_topic_desc     *topic_tmp;
    T_dev_desc       *dev_entry;
    T_dev_desc       *dev_tmp;
    T_io_desc        *io_entry;
    T_io_desc        *io_tmp;
    const char       *type;

    for (i = 1; i < argc; i++) {
        /* get com interface */
        if (strcmp(argv[i], "-c") == 0) {
            if (argc > i) {
                snprintf(com_port, sizeof(com_port), "%s", argv[i + 1]);
            }
        }
        /* our bus address */
        if (strcmp(argv[i], "-a") == 0) {
            if (argc > i) {
                my_addr = (uint8_t)strtoul(argv[i + 1], 0, 0);
                my_addr_valid = true;
            }
        }
        /* config file */
        if (strcmp(argv[i], "-f") == 0) {
            if (argc > i) {
                snprintf(config, sizeof(config), "%s", argv[i + 1]);
            }
        }
        /* event listen address */
        if (strcmp(argv[i], "-e") == 0) {
            if (argc > i) {
                event_addr = (uint8_t)strtoul(argv[i + 1], 0, 0);
                event_addr_valid = true;
            }
        }
        /* broker address */
        if (strcmp(argv[i], "-m") == 0) {
            if (argc > i) {
                snprintf(broker, sizeof(broker), "%s", argv[i + 1]);
            }
        }
        /* event listen address */
        if (strcmp(argv[i], "-p") == 0) {
            if (argc > i) {
                port = (int)strtoul(argv[i + 1], 0, 0);
            }
        }
    }

    if ((strlen(com_port) == 0)  ||
        !my_addr_valid           ||
        !event_addr_valid        ||
        (strlen(broker) == 0)    ||
        (strlen(config) == 0)) {
        print_usage();
        return 0;
    }

    if (ReadConfig(config) == 0) {
        syslog(LOG_ERR, "configuration error");
        return -1;
    }

    mosquitto_lib_init();
    mosq = mosquitto_new("bus-client", true, 0);
    if (!mosq) {
        syslog(LOG_ERR, "can't create mosquitto instance");
        return -1;
    }

    mosquitto_connect_callback_set(mosq, my_connect_callback);
    mosquitto_disconnect_callback_set(mosq, my_disconnect_callback);
    mosquitto_log_callback_set(mosq, my_log_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);

    busHandle = InitBus(com_port);
    if (busHandle == -1) {
        syslog(LOG_ERR, "can't open %s", com_port);
        return -1;
    }

    busFd = SioGetFd(busHandle);
    maxFd = busFd;

    HASH_ITER(hh, topic_desc, topic_entry, topic_tmp) {
        printf("topic %s: type " , topic_entry->topic);
        switch (topic_entry->devtype) {
        case eBusDevTypeDo31:
            switch (topic_entry->io.do31.type) {
            case e_do31_digout:
                type = "digin";
                break;
            case e_do31_shader:
                type = "shader";
                break;
            default:
                type = "unknown";
                break;
            }
            printf("DO31, address %d, output %d, type %s", topic_entry->io.do31.address, topic_entry->io.do31.output, type);
            break;
        case eBusDevTypePwm4:
            printf("PWM4, address %d, output %d", topic_entry->io.pwm4.address, topic_entry->io.pwm4.output);
            break;
        case eBusDevTypeSw8:
            switch (topic_entry->io.sw8.type) {
            case e_sw8_digin:
                type = "digin";
                break;
            case e_sw8_digout:
                type = "digout";
                break;
            case e_sw8_pulseout:
                type = "pulseout";
                break;
            default:
                type = "unknown";
                break;
            }
            printf("SW8, address %d, port %d, type %s", topic_entry->io.sw8.address, topic_entry->io.sw8.port, type);
            break;
        default:
            printf("(unknown)");
            break;
        }
        printf("\n");
    }
    HASH_ITER(hh, dev_desc, dev_entry, dev_tmp) {
        printf("dev %04x\n", dev_entry->phys_dev);
    }
    HASH_ITER(hh, io_desc, io_entry, io_tmp) {
        printf("io %08x %s\n", io_entry->phys_io, io_entry->topic);
    }

    while (mosquitto_connect(mosq, broker, port, 60) != MOSQ_ERR_SUCCESS) {
        sleep(30);
    }
    mosq_connected = true;
    mosqFd = mosquitto_socket(mosq);
    init_state();
    /* subscribe to all configured topics extended by 'set' */
    HASH_ITER(hh, topic_desc, topic_entry, topic_tmp) {
        snprintf(topic, sizeof(topic), "%s/set", topic_entry->topic);
        mosquitto_subscribe(mosq, 0, topic, 1);
    }

    FD_ZERO(&rfds);
    for (;;) {
        if (!mosq_connected) {
            while (mosquitto_reconnect(mosq) != MOSQ_ERR_SUCCESS) {
                sleep(30);
            }
            mosqFd = mosquitto_socket(mosq);
            init_state();
            /* subscribe to all configured topics extended by 'set' */
            HASH_ITER(hh, topic_desc, topic_entry, topic_tmp) {
                snprintf(topic, sizeof(topic), "%s/set", topic_entry->topic);
                mosquitto_subscribe(mosq, 0, topic, 1);
            }
        }

        FD_SET(busFd, &rfds);
        FD_SET(mosqFd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        maxFd = max(mosqFd, busFd);
        ret = select(maxFd + 1, &rfds, 0, 0, &tv);
        if ((ret > 0) && FD_ISSET(busFd, &rfds)) {
            serve_bus();
        }
        if ((ret > 0) && FD_ISSET(mosqFd, &rfds)) {
            mosquitto_loop_read(mosq, 1);
        }
        mosquitto_loop_misc(mosq);
    }

    mosquitto_lib_cleanup();
}
