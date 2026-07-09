/*
 * $QNXLicenseC:
 * Copyright 2019, QNX Software Systems. All Rights Reserved.
 *
 * You must obtain a written license from and pay applicable license fees to QNX
 * Software Systems before you may reproduce, modify or distribute this software,
 * or any work that includes all or part of this software.   Free development
 * licenses are available for evaluation and non-commercial purposes.  For more
 * information visit http://licensing.qnx.com or email licensing@qnx.com.
 *
 * This file may contain contributions from others.  Please review this entire
 * file for other proprietary rights or license notices, as well as the QNX
 * Development Suite License Guide at http://licensing.qnx.com/license-guide/
 * for other information.
 * $
 */

/**
 * @file    rpi_gpio.h
 * @brief   Private header for the resource manager implementation
 */

#ifndef RPI_GPIO_PRIV_H
#define RPI_GPIO_PRIV_H

#include <stdint.h>
#include <sys/neutrino.h>
#include <sys/rpi_gpio.h>


extern uint64_t base_paddr;
extern int      verbose;

enum {
    RPI_VER_UNKNOWN,
    RPI_VER_3,
    RPI_VER_4,
    RPI_VER_5
};

typedef struct client_info {
    rcvid_t rcvid;
    int32_t scoid;
    int32_t coid;
} client_info_t;

static inline int
client_info_match(const client_info_t *ci1, const client_info_t *ci2) {
    return ci1->scoid == ci2->scoid && ci1->coid == ci2->coid;
}

static inline int
client_info_is_unused(const client_info_t *ci) {
    return (ci->rcvid == 0 && ci->scoid == -1 && ci->coid == -1);
}

static inline void
client_info_reset(client_info_t *ci) {
    ci->rcvid = 0;
    ci->scoid = -1;
    ci->coid = -1;
}

#define   RPI5_BASE_PADDR   0x1f000d0000
#define   RPI4_BASE_PADDR   0xfe000000
#define   RPI3_BASE_PADDR   0x3f000000

typedef struct pwm  pwm_t;

/**
 * PWM state for a single GPIO.
 * Whether the GPIO is driven by hardware or software PWM is determined by the
 * channel field (non zero or zero, respectively). Most fields are only used for
 * software PWM.
 */
struct pwm
{
    client_info_t ci;
    unsigned    gpio;
    unsigned    channel;
    unsigned    func;
    unsigned    frequency;
    unsigned    range;
    unsigned    duty;
    unsigned    adj_range; /* range used for RPi5 hard pwm gpios.  It is adjusted to work with the default clock divisor value */
    unsigned    adj_duty;  /* duty used for RPi5 hard pwm gpios.  It is adjusted to work with the default clock divisor value */
    unsigned    mode;
    int         state;
    unsigned    time_on;
    unsigned    time_off;
    unsigned    last_change;
};

int     event_init(unsigned priority, int intr);
int     event_add(client_info_t const *ci, rpi_gpio_event_t const *msg);
void    event_remove_client(client_info_t const *ci);
int     pwm_init(void);
int     pwm_setup(client_info_t const *ci, rpi_gpio_pwm_t const *msg);
int     pwm_set_duty_cycle(client_info_t const *ci, unsigned gpio, unsigned duty);
void    pwm_remove_client(client_info_t const *ci);
void    pwm_debug(unsigned gpio);

#endif
