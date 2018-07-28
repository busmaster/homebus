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
#include <syslog.h>
#include <errno.h>

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
#define BUS_RESPONSE_TIMEOUT 100 /* ms */

#define PATH_LEN                255
#define MAX_LEN_NAME            32

#define MAX_NUM_VAR             256 /* index is uint8_t */

/*-----------------------------------------------------------------------------
*  Typedefs
*/


typedef struct {
    bool           is_valid;
    char           name[MAX_LEN_NAME];
    TBusVarType    type;
    TBusVarMode    mode;
    uint8_t        data[BUS_MAX_VAR_SIZE];
} T_var_desc;

/*-----------------------------------------------------------------------------
*  Variables
*/
static uint8_t     my_addr;
static T_var_desc  var_tab[MAX_NUM_VAR];

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


static int read_config(const char *file)  {

    YAML::Node     ymlcfg = YAML::LoadFile(file);
    YAML::iterator it;
    uint64_t       val_u64;
    int64_t        val_s64;
    char           *end;
    unsigned long  index;
    T_var_desc     *var_desc;

    for (it = ymlcfg.begin(); it != ymlcfg.end(); ++it) {
        const YAML::Node& node = *it;
//        std::cout << "var: " << node["name"].as<std::string>() << "\n";
//        printf("%s\n", node["topic"].as<std::string>().c_str());

        index = strtoul(node["index"].as<std::string>().c_str(), &end, 0);
        if (index < MAX_NUM_VAR) {
            var_desc = &var_tab[index];
        } else {
            syslog(LOG_ERR, "max index is %d (configured index is %lu)", MAX_NUM_VAR, index);
            break;
        }

        var_desc->is_valid = true;
        /* name */
        snprintf(var_desc->name, sizeof(var_desc->name), "%s", node["name"].as<std::string>().c_str());
        /* type */
        if (strcmp(node["type"].as<std::string>().c_str(), "uint8") == 0) {
            var_desc->type = eBusVarType_uint8;
        } else if (strcmp(node["type"].as<std::string>().c_str(), "uint16") == 0) {
            var_desc->type = eBusVarType_uint16;
        } else if (strcmp(node["type"].as<std::string>().c_str(), "string") == 0) {
            var_desc->type = eBusVarType_string;
        } else {
            var_desc->type = eBusVarType_invalid;
            syslog(LOG_ERR, "unknown type %s of var %s", node["type"].as<std::string>().c_str(), var_desc->name);
            break;
        }
        /* mode */
        if (strcmp(node["mode"].as<std::string>().c_str(), "rw") == 0) {
            var_desc->mode = eBusVarMode_rw;
        } else if (strcmp(node["mode"].as<std::string>().c_str(), "ro") == 0) {
            var_desc->mode = eBusVarMode_ro;
        } else if (strcmp(node["mode"].as<std::string>().c_str(), "const") == 0) {
            var_desc->mode = eBusVarMode_const;
        } else {
            var_desc->mode = eBusVarMode_invalid;
            syslog(LOG_ERR, "unknown mode %s of var %s", node["mode"].as<std::string>().c_str(), var_desc->name);
            break;
        }
        /* init value */
        if (var_desc->type == eBusVarType_string) {
            snprintf((char *)var_desc->data, sizeof(var_desc->data), "%s", node["init"].as<std::string>().c_str());
        } else if ((var_desc->type == eBusVarType_uint8)  ||
                   (var_desc->type == eBusVarType_uint16) ||
                   (var_desc->type == eBusVarType_uint32) ||
                   (var_desc->type == eBusVarType_uint64)) {
            val_u64 = strtoull(node["init"].as<std::string>().c_str(), &end, 0);
            // save as little endian
            var_desc->data[0] = val_u64 & 0xff;
            var_desc->data[1] = (val_u64 >> 8) & 0xff;
            var_desc->data[2] = (val_u64 >> 16) & 0xff;
            var_desc->data[3] = (val_u64 >> 24) & 0xff;
            var_desc->data[4] = (val_u64 >> 32) & 0xff;
            var_desc->data[5] = (val_u64 >> 40) & 0xff;
            var_desc->data[6] = (val_u64 >> 48) & 0xff;
            var_desc->data[7] = (val_u64 >> 56) & 0xff;
        } else if ((var_desc->type == eBusVarType_int8)  ||
                   (var_desc->type == eBusVarType_int16) ||
                   (var_desc->type == eBusVarType_int32) ||
                   (var_desc->type == eBusVarType_int64)) {
            val_s64 = strtoll(node["init"].as<std::string>().c_str(), &end, 0);
            // save as little endian
            var_desc->data[0] = val_s64 & 0xff;
            var_desc->data[1] = (val_s64 >> 8) & 0xff;
            var_desc->data[2] = (val_s64 >> 16) & 0xff;
            var_desc->data[3] = (val_s64 >> 24) & 0xff;
            var_desc->data[4] = (val_s64 >> 32) & 0xff;
            var_desc->data[5] = (val_s64 >> 40) & 0xff;
            var_desc->data[6] = (val_s64 >> 48) & 0xff;
            var_desc->data[7] = (val_s64 >> 56) & 0xff;
        }

        printf("index: %lu\n", index);
        printf("name: %s\n", var_desc->name);
        printf("type: %d\n", var_desc->type);
        printf("mode: %d\n", var_desc->mode);
        for (int i = 0; i < 32; i++) {
            printf("%02x ", var_desc->data[i]);
        }
        printf("\n\n");
    }
    if (it != ymlcfg.end()) {
        syslog(LOG_ERR, "too many vars in config (max num is %d)", MAX_NUM_VAR);
        return -1;
    }

    return 0;
}

static int set_busvar(void) {

    T_var_desc  *var;
    int         size;
    TBusVarResult result;
    int i;

    var = var_tab;
    for (i = 0; i < MAX_NUM_VAR; i++, var++) {
        if (!var->is_valid) {
        	continue;
        }
        if (var->type == eBusVarType_string) {
            size = strlen((char *)var->data) + 1;
        } else {
            size = BusVarTypeToSize(var->type);
        }
        if (size >= 0) {
            BusVarAdd(size, i, false);
            BusVarSetInfo(i, var->name, var->type, var->mode);
            BusVarWrite(i, var->data, size, &result);
        }
    }
    return 0;
}

static void serve_bus(void) {
    TBusTelegram  *rx_msg;
    TBusMsgType   msg_type;
    bool          msg_for_me = false;
    static TBusTelegram    tx_msg;
    static bool            tx_retry = false;
    uint8_t val8;

    if (tx_retry) {
        tx_retry = BusSend(&tx_msg) != BUS_SEND_OK;
        return;
    }

    if (BusCheck() != BUS_MSG_OK) {
        return;
    }
    rx_msg = BusMsgBufGet();

    msg_type = rx_msg->type;
    switch (msg_type) {
    case eBusDevReqGetVar:
    case eBusDevReqSetVar:
    case eBusDevRespGetVar:
    case eBusDevRespSetVar:
        if (rx_msg->msg.devBus.receiverAddr == my_addr) {
            msg_for_me = true;
        }
        break;
    default:
        break;
    }
    if (!msg_for_me) {
        return;
    }

    switch (msg_type) {
    case eBusDevReqGetVar:
        val8 = rx_msg->msg.devBus.x.devReq.getVar.index;
        tx_msg.msg.devBus.x.devResp.getVar.length =
            BusVarRead(val8, tx_msg.msg.devBus.x.devResp.getVar.data,
                       sizeof(tx_msg.msg.devBus.x.devResp.getVar.data),
                       &tx_msg.msg.devBus.x.devResp.getVar.result);
        tx_msg.senderAddr = my_addr;
        tx_msg.type = eBusDevRespGetVar;
        tx_msg.msg.devBus.receiverAddr = rx_msg->senderAddr;
        tx_msg.msg.devBus.x.devResp.getVar.index = val8;
        tx_retry = BusSend(&tx_msg) != BUS_SEND_OK;
        break;
    case eBusDevReqSetVar:
        val8 = rx_msg->msg.devBus.x.devReq.setVar.index;
        BusVarWrite(val8, rx_msg->msg.devBus.x.devReq.setVar.data,
                    rx_msg->msg.devBus.x.devReq.setVar.length,
                    &tx_msg.msg.devBus.x.devResp.setVar.result);
        tx_msg.senderAddr = my_addr;
        tx_msg.type = eBusDevRespSetVar;
        tx_msg.msg.devBus.receiverAddr = rx_msg->senderAddr;
        tx_msg.msg.devBus.x.devResp.setVar.index = val8;
        tx_retry = BusSend(&tx_msg) != BUS_SEND_OK;
        break;
    default:
        break;
    }
}

/*-----------------------------------------------------------------------------
*  show help
*/
static void print_usage(void) {

   printf("\nUsage:\n");
   printf("varserver -c sio-port -a bus-address -f yaml-cfg\n");
}

/*-----------------------------------------------------------------------------
*  program start
*/
int main(int argc, char *argv[]) {

    int              busFd;
    int              maxFd;
    int              busHandle;
    fd_set           rfds;
    int              ret;
    int              i;
    char             com_port[PATH_LEN] = "";
    char             config[PATH_LEN] = "";
    bool             my_addr_valid = false;
    struct timeval   tv;

    for (i = 1; i < argc; i++) {
        /* get com interface */
        if (strcmp(argv[i], "-c") == 0) {
            if (argc > i) {
            	i++;
                snprintf(com_port, sizeof(com_port), "%s", argv[i]);
                continue;
            }
            break;
        }
        /* our bus address */
        if (strcmp(argv[i], "-a") == 0) {
            if (argc > i) {
            	i++;
                my_addr = (uint8_t)strtoul(argv[i], 0, 0);
                my_addr_valid = true;
                continue;
            }
            break;
        }
        /* config file */
        if (strcmp(argv[i], "-f") == 0) {
            if (argc > i) {
            	i++;
                snprintf(config, sizeof(config), "%s", argv[i]);
                continue;
            }
            break;
        }
    }

    if ((strlen(com_port) == 0)  ||
        !my_addr_valid           ||
        (strlen(config) == 0)) {
        print_usage();
        return 0;
    }

    if (read_config(config) != 0) {
        syslog(LOG_ERR, "configuration error");
        return -1;
    }

    BusVarInit(my_addr, 0);

    if (set_busvar() != 0) {
        syslog(LOG_ERR, "can't set configuration to busvars");
        return -1;
    }

    busHandle = InitBus(com_port);
    if (busHandle == -1) {
        syslog(LOG_ERR, "can't open %s", com_port);
        return -1;
    }

    busFd = SioGetFd(busHandle);
    maxFd = busFd;

    FD_ZERO(&rfds);
    for (;;) {
        FD_SET(busFd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        maxFd = busFd;
        ret = select(maxFd + 1, &rfds, 0, 0, &tv);
        if ((ret > 0) && FD_ISSET(busFd, &rfds)) {
            serve_bus();
        }
    }
}
