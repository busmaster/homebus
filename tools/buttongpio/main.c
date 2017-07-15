/*
 * main.c
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
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "sio.h"
#include "bus.h"

/*-----------------------------------------------------------------------------
*  Macros
*/
#define SIZE_COMPORT         100
#define BUFFER_MAX           3
#define DIRECTION_MAX        35
#define VALUE_MAX            30
#define BUTTON_PRESS_TIMEOUT 2000
#define BUS_TIMEOUT          60000

/*-----------------------------------------------------------------------------
*  Functions
*/
static int gpio_export(int pin) {
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open export for writing!\n");
        return -1;
     }

    bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);

    return 0;
}

static int gpio_set_output_direction(int pin) {
    char path[DIRECTION_MAX];
    int fd;

    snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open gpio direction for writing!\n");
        return -1;
    }

    if (-1 == write(fd, "out", 3)) {
        fprintf(stderr, "Failed to set direction!\n");
        return -1;
    }
    close(fd);

    return 0;
}

static int gpio_write(int pin, int value) {
    char path[VALUE_MAX];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open gpio value for writing!\n");
        return -1;
    }

    if (1 != write(fd, value == 0 ? "0" : "1", 1)) {
        fprintf(stderr, "Failed to write value!\n");
        return -1;
    }

    close(fd);
    return 0;
}

/*-----------------------------------------------------------------------------
*  sio open and bus init
*/
static int init_bus(const char *com_port) {

    uint8_t ch;
    int     handle;

    SioInit();
    handle = SioOpen(com_port, eSioBaud9600, eSioDataBits8, eSioParityNo, eSioStopBits1, eSioModeHalfDuplex);
    if (handle == -1) {
        return -1;
    }
    while (SioGetNumRxChar(handle) > 0) {
        SioRead(handle, &ch, sizeof(ch));
    }
    BusInit(handle);

    return handle;
}


/*-----------------------------------------------------------------------------
*  show help
*/
static void print_usage(void) {

   printf("\r\nUsage:\r\n");
   printf("buttongpio -c port -a buttonaddress -i buttoninput -g gpio\n");
}



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
*  program start
*/
int main(int argc, char *argv[]) {

    int           i;
    char          com_port[SIZE_COMPORT] = "";
    uint8_t       button_addr;
    bool          button_addr_valid = false;
    uint8_t       button_input = 0;
    uint16_t      gpio_output;
    bool          gpio_output_valid = false;
    uint8_t       bus_ret;
    TBusTelegram  *rx_bus_msg;
    int           sio_handle;
    int           sio_fd;
    fd_set        rfds;
    int           ret;
    struct timeval tv;
    unsigned int  start_timestamp;
    unsigned int  curr_timestamp;
    bool          gpio_on = false;

    /* get com interface */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            if (argc > i) {
                strncpy(com_port, argv[i + 1], sizeof(com_port) - 1);
                com_port[sizeof(com_port) - 1] = '\0';
            }
            break;
        }
    }
    /* button address to listen */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            if (argc > i) {
                button_addr = atoi(argv[i + 1]);
                button_addr_valid = true;
            }
            break;
        }
    }

    /* button input */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0) {
            if (argc > i) {
                button_input = atoi(argv[i + 1]);
            }
            break;
        }
    }
    /* gpio output */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-g") == 0) {
            if (argc > i) {
                gpio_output = atoi(argv[i + 1]);
                gpio_output_valid = true;
            }
            break;
        }
    }

    if ((strlen(com_port) == 0) ||
        !button_addr_valid       ||
        ((button_input != 1) && (button_input != 2)) ||
        !gpio_output_valid) {
        print_usage();
        return 0;
    }

    if (gpio_export(gpio_output) != 0) {
        return -1;
    }
    if (gpio_set_output_direction(gpio_output) != 0) {
        return -1;
    }
    /* set inactive (active low) */
    if (gpio_write(gpio_output, 1) != 0) {
        return -1;
    }

    start_timestamp = get_tick_count();
    do {
        sio_handle = init_bus(com_port);
        curr_timestamp = get_tick_count();
        sleep(1);
    }  while ((sio_handle == -1) && ((curr_timestamp - start_timestamp) < BUS_TIMEOUT));
    if (sio_handle == -1) {
        printf("cannot open %s\r\n", com_port);
        return -1;
    }
    sio_fd = SioGetFd(sio_handle);

    for (;;) {
        FD_ZERO(&rfds);
        FD_SET(sio_fd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        ret = select(sio_fd + 1, &rfds, 0, 0, &tv);
        if ((ret > 0) && FD_ISSET(sio_fd, &rfds)) {
            bus_ret = BusCheck();
            if (bus_ret == BUS_MSG_OK) {
                rx_bus_msg = BusMsgBufGet();
                if (rx_bus_msg->senderAddr == button_addr) {
                    if  (((button_input == 1) && ((rx_bus_msg->type == eBusButtonPressed1) || (rx_bus_msg->type == eBusButtonPressed1_2))) ||
                         ((button_input == 2) && ((rx_bus_msg->type == eBusButtonPressed2) || (rx_bus_msg->type == eBusButtonPressed1_2)))) {
                        /* set gpio to 0 (active low) */
                        if (!gpio_on) {
                            gpio_write(gpio_output, 0);
                            gpio_on = true;
                        }
                        start_timestamp = get_tick_count();
                    }
                }
            }
        } else if (ret == 0) {
            if (gpio_on) {
                /* timeout check */
                curr_timestamp = get_tick_count();
                if ((curr_timestamp - start_timestamp) > BUTTON_PRESS_TIMEOUT) {
                    gpio_write(gpio_output, 1);
                    gpio_on = false;
                }
            }
        }
    }

    SioClose(sio_handle);
    return ret;
}

