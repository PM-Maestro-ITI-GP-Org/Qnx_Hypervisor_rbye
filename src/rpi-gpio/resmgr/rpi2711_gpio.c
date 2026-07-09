/*
 * $QNXLicenseC:
 * Copyright 2025 QNX Software Systems. All Rights Reserved.
 *
 * You must obtain a written license from and pay applicable license fees to QNX
 * Software Systems before you may reproduce, modify or distribute this
 * software, or any work that includes all or part of this software.   Free
 * development licenses are available for evaluation and non-commercial
 * purposes.  For more information visit http://licensing.qnx.com or email
 * licensing@qnx.com.
 *
 * This file may contain contributions from others.  Please review this entire
 * file for other proprietary rights or license notices, as well as the QNX
 * Development Suite License Guide at http://licensing.qnx.com/license-guide/
 * for other information.
 * $
 */

/* RaspberryPi 4 GPIO specific functionality */

#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include "rpi_gpio.h"
#include "rpi_gpio_priv.h"

#define RPI_3_PERIPHERALS   0x3f000000
#define RPI_4_PERIPHERALS   0xfe000000

static uint32_t volatile *rpi_gpio_regs;
#ifndef __RPI_GPIO_REGS
#define __RPI_GPIO_REGS rpi_gpio_regs
#endif

/* RPi4 */
#define PWM_REG_OFFSET   0x20c000
#define CLK_REG_OFFSET   0x101000
#define TIMER_REG_OFFSET   0x3000

/*
 * PWM registers.
 */
enum
{
    REG_PWMCTL = 0,
    REG_PWMSTA = 1,
    REG_PWMDMAC = 2,
    REG_PWMRNG1 = 4,
    REG_PWMDATA1 = 5,
    REG_PWMFIF1 = 6,
    REG_PWMRNG2 = 8,
    REG_PWMDATA2 = 9
};

/**
 * RaspberryPi GPIO registers.
 */
enum
{
    RPI_GPIO_REG_GPSET0 = 7,
    RPI_GPIO_REG_GPCLR0 = 10,
    RPI_GPIO_REG_GPLEV0 = 13,
    RPI_GPIO_REG_GPEDS0 = 16,
    RPI_GPIO_REG_GPEDS1 = 17,
    RPI_GPIO_REG_GPREN0 = 19,
    RPI_GPIO_REG_GPREN1 = 20,
    RPI_GPIO_REG_GPFEN0 = 22,
    RPI_GPIO_REG_GPFEN1 = 23,
    RPI_GPIO_REG_GPHEN0 = 25,
    RPI_GPIO_REG_GPHEN1 = 26,
    RPI_GPIO_REG_GPLEN0 = 28,
    RPI_GPIO_REG_GPLEN1 = 29,
    // RaspberryPi 3
    RPI_GPIO_REG_GPPUD = 37,
    RPI_GPIO_REG_GPPUDCLK1 = 38,
    RPI_GPIO_REG_GPPUDCLK2 = 39,
    // RaspberryPi 4
    RPI_GPIO_REG_GPPUD0 = 57,
};

/**
 * Mapped GPIO registers.
 * An executable including this header is responsible for defining this global
 * in a source file.
 */
extern uint32_t volatile   *__RPI_GPIO_REGS;

/* pwm registers */
static uint32_t volatile    *pwm_regs;
static uint32_t volatile    *clk_regs;

/* RPi4 pwm hardware GPIOs */
static pwm_t   rpi2711_pwm_gpio_12 = {
    .gpio = 12,
    .channel = 1,
    .func = RPI_GPIO_FUNC_ALT_0
};

static pwm_t    rpi2711_pwm_gpio_13 = {
    .gpio = 13,
    .channel = 2,
    .func = RPI_GPIO_FUNC_ALT_0
};

static pwm_t    rpi2711_pwm_gpio_18 = {
    .gpio = 18,
    .channel = 1,
    .func = RPI_GPIO_FUNC_ALT_5
};

static pwm_t    rpi2711_pwm_gpio_19 = {
    .gpio = 19,
    .channel = 2,
    .func = RPI_GPIO_FUNC_ALT_5
};

/**
 * Programs the FSEL register for the given GPIO.
 * The value is one of the RPI_GPIO_FUNC_* constants.
 * @param   gpio    Pin number
 * @param   value   New pin function
 */
static void
rpi2711_gpio_set_select(uint32_t const gpio, uint32_t const value)
{
    uint32_t const  reg = (gpio / 10);
    uint32_t const  off = (gpio % 10) * 3;
    uint32_t val = __RPI_GPIO_REGS[reg];
    val &= ~(7 << off);
    val |= (value << off);
    __RPI_GPIO_REGS[reg] = val;
}

/**
 * Reads the FSEL register for the given GPIO.
 * @param   gpio    Pin number
 * @return  Current pin function
 */
static uint32_t
rpi2711_gpio_get_select(uint32_t const gpio)
{
    uint32_t const  reg = gpio / 10;
    uint32_t const  off = (gpio % 10) * 3;
    return (rpi_gpio_regs[reg] >> off) & 7;
}

/**
 * Turns the pin on.  It is expected that the pin is in output mode.
 * This should be checked prior to calling this function
 * @param   gpio    Pin number
 */
static void
rpi2711_gpio_set(uint32_t const gpio)
{
    uint32_t const  reg = RPI_GPIO_REG_GPSET0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    __RPI_GPIO_REGS[reg] = (1 << off);
}

/**
 * Turns the pin off.  It is expected that the pin is in output mode.
 * This should be checked prior to calling this function
 * @param   gpio    Pin number
 */
static void
rpi2711_gpio_clear(uint32_t gpio)
{
    uint32_t const  reg = RPI_GPIO_REG_GPCLR0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    __RPI_GPIO_REGS[reg] = (1 << off);
}

/**
 * Sets or clears a pin.  It is expected that the pin is in output mode.
 * This should be checked prior to calling this function
 * @param   gpio    Pin number
 * @param   value   0 to clear, any other value to set
 */
static void
rpi2711_gpio_write(uint32_t const gpio, uint32_t const value)
{
    if (value) {
        rpi_gpio_set(gpio);
    } else {
        rpi_gpio_clear(gpio);
    }
}

/**
 * Reads the level of a pin in input mode.
 * @param   gpio    Pin number
 * @return  0 if the pin is low, 1 if high
 */
static uint32_t
rpi2711_gpio_read(uint32_t const gpio)
{
    uint32_t const  reg = RPI_GPIO_REG_GPLEV0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    return ((__RPI_GPIO_REGS[reg] >> off) & 1);
}

/**
 * Detect rising edge events on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static void
rpi2711_gpio_detect_rising_edge(uint32_t const gpio, bool const enable)
{
    uint32_t const  reg = RPI_GPIO_REG_GPREN0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    if (enable) {
        __RPI_GPIO_REGS[reg] |= (1 << off);
    } else {
        __RPI_GPIO_REGS[reg] &= ~(1 << off);
    }
}

/**
 * Detect falling edge events on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static void
rpi2711_gpio_detect_falling_edge(uint32_t const gpio, bool const enable)
{
    uint32_t const  reg = RPI_GPIO_REG_GPFEN0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    if (enable) {
        __RPI_GPIO_REGS[reg] |= (1 << off);
    } else {
        __RPI_GPIO_REGS[reg] &= ~(1 << off);
    }
}

/**
 * Detect a high level on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static void
rpi2711_gpio_detect_level_high(uint32_t const gpio, bool const enable)
{
    uint32_t const  reg = RPI_GPIO_REG_GPHEN0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    if (enable) {
        __RPI_GPIO_REGS[reg] |= (1 << off);
    } else {
        __RPI_GPIO_REGS[reg] &= ~(1 << off);
    }
}

/**
 * Detect a low level on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static void
rpi2711_gpio_detect_level_low(uint32_t const gpio, bool const enable)
{
    uint32_t const  reg = RPI_GPIO_REG_GPLEN0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    if (enable) {
        __RPI_GPIO_REGS[reg] |= (1 << off);
    } else {
        __RPI_GPIO_REGS[reg] &= ~(1 << off);
    }
}

/* Clear the event detect for the gpio
 * @param   gpio    GPIO number
 */
static void
rpi2711_gpio_event_clear_detect(uint32_t const gpio)
{
    if (gpio < 32) {
        rpi_gpio_regs[RPI_GPIO_REG_GPEDS0] = 1 << gpio;
    } else {
        rpi_gpio_regs[RPI_GPIO_REG_GPEDS1] = 1 < (gpio - 32);
    }
}

/* Clear all events and masks for all gpios.
 */
static void
rpi2711_gpio_clear_all_events(void)
{
    rpi_gpio_regs[RPI_GPIO_REG_GPEDS0] = 0xffffffff;
    rpi_gpio_regs[RPI_GPIO_REG_GPEDS1] = 0xffffffff;
    rpi_gpio_regs[RPI_GPIO_REG_GPREN0] = 0;
    rpi_gpio_regs[RPI_GPIO_REG_GPREN1] = 0;
    rpi_gpio_regs[RPI_GPIO_REG_GPFEN0] = 0;
    rpi_gpio_regs[RPI_GPIO_REG_GPFEN1] = 0;
    rpi_gpio_regs[RPI_GPIO_REG_GPHEN0] = 0;
    rpi_gpio_regs[RPI_GPIO_REG_GPHEN1] = 0;
    rpi_gpio_regs[RPI_GPIO_REG_GPLEN0] = 0;
    rpi_gpio_regs[RPI_GPIO_REG_GPLEN1] = 0;
}

/**
 * Checks if an interrupt occurred for the given gpio.
 * @param   gpio    Pin number
 * @return  true if interrupt occurred, false otherwise
 */
static bool
rpi2711_gpio_event_detected(uint32_t const gpio)
{
    if (gpio < 32) {
        unsigned const events = rpi_gpio_regs[RPI_GPIO_REG_GPEDS0];
        return (events & (1 << gpio));
    } else {
        unsigned const events = rpi_gpio_regs[RPI_GPIO_REG_GPEDS1];
        return (events & (1 << (gpio - 32)));
    }
}

/**
 * Enables pull-up/pull-down detection on the given GPIO.
 * BCM2835 version (used in RaspberryPi 3).
 * @param   gpio    GPIO number
 * @param   pud     One of the RPI_GPIO_PUD_* constants
 * @return  true if successful, false otherwise
 */
static bool
rpi_bcm2835_gpio_set_pud(uint32_t const gpio, uint32_t const pud)
{
    switch (pud) {
    case RPI_GPIO_PUD_OFF:
    case RPI_GPIO_PUD_UP:
    case RPI_GPIO_PUD_DOWN:
        break;
    default:
        return false;
    }

    static int const pud_value_bcm2835[] = {
        [RPI_GPIO_PUD_OFF]  = 0,
        [RPI_GPIO_PUD_UP]   = 2,
        [RPI_GPIO_PUD_DOWN] = 1
    };

    // Set the PUD value.
    uint32_t const  value = pud_value_bcm2835[pud];
    __RPI_GPIO_REGS[RPI_GPIO_REG_GPPUD] &= ~3;
    __RPI_GPIO_REGS[RPI_GPIO_REG_GPPUD] |= value;

    // Manual says to wait 150 cycles. Assuming a cycle is at most one
    // nanosecond.
    nanospin_ns(150);

    // Enable the PUD clock for the GPIO.
    uint32_t const  clock_reg = RPI_GPIO_REG_GPPUDCLK1 + (gpio / 32);
    uint32_t const  clock_off = gpio % 32;
    __RPI_GPIO_REGS[clock_reg] = (1 << clock_off);

    nanospin_ns(150);

    // Turn off the PUD signal and the clock.
    __RPI_GPIO_REGS[RPI_GPIO_REG_GPPUD] &= ~3;
    __RPI_GPIO_REGS[clock_reg] = 0;

    return true;
}

/**
 * Enables pull-up/pull-down detection on the given GPIO.
 * BCM2711 version (used in RaspberryPi 3).
 * @param   gpio    GPIO number
 * @param   pud     One of the RPI_GPIO_PUD_* constants
 * @return  true if successful, false otherwise
 */
static bool
rpi2711_gpio_set_pud_bcm2711(uint32_t const gpio, uint32_t const pud)
{
    switch (pud) {
    case RPI_GPIO_PUD_OFF:
    case RPI_GPIO_PUD_UP:
    case RPI_GPIO_PUD_DOWN:
        break;
    default:
        return false;
    }

    static int pud_value_bcm2711[] = {
        [RPI_GPIO_PUD_OFF]  = 0,
        [RPI_GPIO_PUD_UP]   = 1,
        [RPI_GPIO_PUD_DOWN] = 2
    };

    uint32_t const  reg = RPI_GPIO_REG_GPPUD0 + (gpio / 16);
    uint32_t const  off = (gpio % 16) * 2;
    uint32_t const  value = pud_value_bcm2711[pud];
    __RPI_GPIO_REGS[reg] &= ~(3 << off);
    __RPI_GPIO_REGS[reg] |= (value << off);
    return true;
}

/**
 * Map the GPIO registers into the process' address space.
 * @param   base_paddr  Base physical address of the I/O space
 * @return  true if successful, false otherwise
 */
static bool
rpi2711_gpio_map_regs(uintptr_t const base_paddr)
{
    if (__RPI_GPIO_REGS != NULL) {
        return true;
    }

    void * const ptr = mmap(0, __PAGESIZE, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                            MAP_PHYS | MAP_SHARED, NOFD, base_paddr + 0x200000);
    if (ptr == MAP_FAILED) {
        return false;
    }

    __RPI_GPIO_REGS = ptr;
    return true;
}

/**
 * Unmap the GPIO registers.
 * @return  true if successful, false otherwise
 */
static bool
rpi2711_gpio_unmap_regs(void)
{
    if (__RPI_GPIO_REGS == NULL) {
        return true;
    }

    return (munmap((void *)__RPI_GPIO_REGS, __PAGESIZE) == 0);
}

/* mmaps the pwm registers and sets up the supported
 * hard pwm values in pwm_map.
 * @param   pwm_map   Pointer to pwm_t structure.
 * @return EOK if successful, errno otherwise
 */
static int rpi2711_pwm_init(void **p_map) 
{
    pwm_t **pwm_map = (pwm_t**)p_map;
    pwm_regs = mmap(0, __PAGESIZE,
                PROT_READ | PROT_WRITE | PROT_NOCACHE,
                MAP_SHARED | MAP_PHYS,
                -1, base_paddr + PWM_REG_OFFSET);

    if (pwm_regs == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    // Map the clock registers.
    clk_regs = mmap(0, __PAGESIZE,
                    PROT_READ | PROT_WRITE | PROT_NOCACHE,
                    MAP_SHARED | MAP_PHYS,
                    -1, base_paddr + CLK_REG_OFFSET);

    if (clk_regs == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    /* Setup the hard gpio pins */
    pwm_map[12] = &rpi2711_pwm_gpio_12;
    pwm_map[13] = &rpi2711_pwm_gpio_13;
    pwm_map[18] = &rpi2711_pwm_gpio_18;
    pwm_map[19] = &rpi2711_pwm_gpio_19;

    return 0;
}

/**
 * Enables hardware PWM.
 * Hardware PWM is based on the undocumented PWM clock, with a frequency of
 * 19.2MHz. The clock is associated with a divisor, which is set based on the
 * given frequency and range. The range is also recorded in the PWM range
 * register for the appropriate channel.
 * @param   pwm     A pwm_t structure for a hardware-PWM-enabled GPIO
 * @param   msg     RPI_GPIO_PWM_SETUP message
 * @return  0 if successful, error code otherwise
 */
static int rpi2711_setup_pwm_hard(void  *p, rpi_gpio_pwm_t const * const msg)
{
    pwm_t *pwm = (pwm_t *)p;
    unsigned const  frequency = msg->frequency;
    unsigned const  range = msg->range;

    if (range > 4095) {
        return EINVAL;
    }

    uint32_t    pwm_enable;
    uint32_t    pwm_enable_shift;

    if (pwm->channel == 1) {
        pwm_enable_shift = 0;
    } else {
        pwm_enable_shift = 8;
    }

    // Clear the affected channel bits.
    pwm_enable = pwm_regs[REG_PWMCTL];
    pwm_enable &= (0xff00 >> pwm_enable_shift);

    switch (msg->mode) {
    case RPI_PWM_MODE_PWM:
        pwm_enable |= ((1 << 0) << pwm_enable_shift);
        break;

    case RPI_PWM_MODE_MS:
        pwm_enable |= (((1 << 7) | (1 << 0)) << pwm_enable_shift);
        break;

    default:
        return EINVAL;
    }

    // Calculate a clock divisor.
    double  oscillator;
    if (config->version == RPI_VER_4) {
        oscillator = 54000000.0;
    } else {
        oscillator = 19200000.0;
    }

    double const    divd = oscillator / ((double)frequency * (double)range);
    unsigned const  divi = (unsigned)divd;

    if ((divi < 1) || (divi > 4095)) {
        fprintf(stderr, "invalid divi range: %d\n", divi);
        return ERANGE;
    }

    // Stop PWM.
    pwm_regs[REG_PWMCTL] = 0;

    // Stop clock and wait for it to become idle.
    clk_regs[40] = 0x5A000001;
    nanospin_ns(100000);

    while (clk_regs[40] & 0x80) {
        nanospin_ns(1000);
    }

    // Write a new divisor and restart clock.
    clk_regs[41] = 0x5A000000 | (divi << 12);
    clk_regs[40] = 0x5A000011;

    // Set range.
    pwm_regs[(pwm->channel == 1) ? REG_PWMRNG1 : REG_PWMRNG2] = range;

    // Enable PWM channel 1 in the requested mode.
    // FIXME:
    // Without the short delay PWM doesn't start in M/S mode. It's not clear
    // why.
    nanospin_ns(1000);
    pwm_regs[REG_PWMCTL] = pwm_enable;

    rpi_gpio_set_select(pwm->gpio, pwm->func);

    return 0;
}

/* Sets up the range, mode and enables pwm for the gpio
 * @param   p         Pointer to pwm_t structure.
 * @param   msg       Pointer to rpi_gpio_pwm_t msg containing the information
 *                    to be set.
 * @return EOK if successful, errno otherwise
 */
static void
rpi2711_set_pwm_duty(void *p, unsigned const duty) 
{
    pwm_t *pwm = (pwm_t*)p;
    pwm_regs[(pwm->channel == 1) ? REG_PWMDATA1 : REG_PWMDATA2] = duty;
}

/* Configuration for RPi4 */
gpio_config_t gpio_config_rpi4 = {
  .version = RPI_VER_4,
  .num_gpios = RPI_GPIO_NUM,
  .max_funcs = 7,
  .intr_offset = 145,
  .timer_intr_offset = 97,
  .base_paddr = RPI4_BASE_PADDR,
  .base_timer_paddr = RPI4_BASE_PADDR + TIMER_REG_OFFSET,

  .set_select = rpi2711_gpio_set_select,
  .get_select = rpi2711_gpio_get_select,
  .set = rpi2711_gpio_set,
  .clear = rpi2711_gpio_clear,
  .read = rpi2711_gpio_read,
  .write = rpi2711_gpio_write,
  .detect_rising_edge = rpi2711_gpio_detect_rising_edge,
  .detect_falling_edge = rpi2711_gpio_detect_falling_edge,
  .detect_level_high = rpi2711_gpio_detect_level_high,
  .detect_level_low = rpi2711_gpio_detect_level_low,
  .event_clear_detect = rpi2711_gpio_event_clear_detect,
  .event_clear_all_events = rpi2711_gpio_clear_all_events,
  .event_detected = rpi2711_gpio_event_detected,
  .set_pud = rpi2711_gpio_set_pud_bcm2711,
  .gpio_map_regs = rpi2711_gpio_map_regs,
  .gpio_unmap_regs = rpi2711_gpio_unmap_regs,
  .pwm_init = rpi2711_pwm_init,
  .setup_pwm_hard = rpi2711_setup_pwm_hard,
  .set_pwm_duty = rpi2711_set_pwm_duty
};

/* Configuration for RPi3 
 * Define RPi3 here since there is only a few differences
 * between RPi 3 & 4 
 */
gpio_config_t gpio_config_rpi3 = {
  .version = RPI_VER_3,
  .num_gpios = RPI_GPIO_NUM,
  .max_funcs = 7,
  .intr_offset = 145,
  .timer_intr_offset = 1,
  .base_paddr = RPI3_BASE_PADDR,
  .base_timer_paddr = RPI4_BASE_PADDR + TIMER_REG_OFFSET,

  .set_select = rpi2711_gpio_set_select,
  .get_select = rpi2711_gpio_get_select,
  .set = rpi2711_gpio_set,
  .clear = rpi2711_gpio_clear,
  .read = rpi2711_gpio_read,
  .write = rpi2711_gpio_write,
  .detect_rising_edge = rpi2711_gpio_detect_rising_edge,
  .detect_falling_edge = rpi2711_gpio_detect_falling_edge,
  .detect_level_high = rpi2711_gpio_detect_level_high,
  .detect_level_low = rpi2711_gpio_detect_level_low,
  .event_clear_detect = rpi2711_gpio_event_clear_detect,
  .event_clear_all_events = rpi2711_gpio_clear_all_events,
  .event_detected = rpi2711_gpio_event_detected,
  .set_pud = rpi_bcm2835_gpio_set_pud,
  .gpio_map_regs = rpi2711_gpio_map_regs,
  .gpio_unmap_regs = rpi2711_gpio_unmap_regs,
  .pwm_init = rpi2711_pwm_init,
  .setup_pwm_hard = rpi2711_setup_pwm_hard,
  .set_pwm_duty = rpi2711_set_pwm_duty
};
