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

#ifndef __RPI_GPIO_H__
#define __RPI_GPIO_H__

#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/rpi_gpio.h>


/**
 * GPIO configuration structure
 * contains pointer to the hardware specific functionality for
 * GPIO.
 */
 typedef struct gpio_config {
    /**
     * RPi version
     */
    const int      version;
    /**
     * Maximum number of GPIOs supprted
     */
    const uint32_t num_gpios;
    /**
     * Maximum number of functions per GPIO
     */
    const uint32_t max_funcs;
    /**
     * Event interrupt offset
     */
    const uint32_t intr_offset;
    /**
     * Base register offset
     */
    const uint64_t base_paddr;
    /**
     * Timer interrupt offset. Used for soft pwm support
     */
    const uint32_t timer_intr_offset;
    /**
     * Address for the timer registers used for soft pwm support
     */
    const uint64_t base_timer_paddr;

    /**
     * Programs the FSEL register for the given GPIO.
     * The value is one of the RPI_GPIO_FUNC_* constants.
     * @param   gpio    Pin number
     * @param   value   New pin function
     */
    void (*set_select)(uint32_t const gpio, uint32_t const value);

    /**
     * Reads the FSEL register for the given GPIO.
     * @param   gpio    Pin number
     * @return  Current pin function
     */
    uint32_t (*get_select)(uint32_t const gpio);

    /**
     * Turns the pin on.  It is expected that the pin is in output mode.
     * This should be checked prior to calling this function
     * @param   gpio    Pin number
     */
    void (*set)(uint32_t const gpio);

    /**
     * Turns the pin off.  It is expected that the pin is in output mode.
     * This should be checked prior to calling this function
     * @param   gpio    Pin number
     */
    void (*clear)(uint32_t gpio);

    /**
     * Sets or clears a pin.  It is expected that the pin is in output mode.
     * This should be checked prior to calling this function
     * @param   gpio    Pin number
     * @param   value   0 to clear, any other value to set
     */
    void (*write)(uint32_t const gpio, uint32_t const value);

    /**
     * Reads the level of a pin in input mode.
     * @param   gpio    Pin number
     * @return  0 if the pin is low, 1 if high
     */
    uint32_t (*read)(uint32_t const gpio);

    /**
     * Detect rising edge events on the given pin.
     * @param   gpio    Pin number
     * @param   enable  true to enable, false to disable
     */
    void (*detect_rising_edge)(uint32_t const gpio, bool const enable);

    /**
     * Detect falling edge events on the given pin.
     * @param   gpio    Pin number
     * @param   enable  true to enable, false to disable
     */
    void (*detect_falling_edge)(uint32_t const gpio, bool const enable);

    /**
     * Detect a high level on the given pin.
     * @param   gpio    Pin number
     * @param   enable  true to enable, false to disable
     */
    void (*detect_level_high)(uint32_t const gpio, bool const enable);

    /**
     * Detect a low level on the given pin.
     * @param   gpio    Pin number
     * @param   enable  true to enable, false to disable
     */
    void (*detect_level_low)(uint32_t const gpio, bool const enable);

    /* Clear the event detect for the GPIO
     * @param   gpio    GPIO number
     */
    void (*event_clear_detect)(uint32_t const gpio);

    /* Clear all events and masks for all GPIOs.
     */
    void (*event_clear_all_events)(void);

    /**
     * Checks if an interrupt occurred for the given GPIO.
     * @param   gpio    Pin number
     * @return  true if interrupt occurred, false otherwise
     */
    bool (*event_detected)(uint32_t const gpio);

    /**
     * Enables pull-up/pull-down detection on the given GPIO.
     * @param   gpio    GPIO number
     * @param   pud     One of the RPI_GPIO_PUD_* constants
     * @return  true if successful, false otherwise
     */
    bool (*set_pud)(uint32_t const gpio, uint32_t const pud);

    /**
     * Map the GPIO registers into the process' address space.
     * @param   base_paddr  Base physical address of the I/O space
     * @return  true if successful, false otherwise
     */
    bool (*gpio_map_regs)(uintptr_t const base_paddr);

    /**
     * Unmap the GPIO registers.
     * @return  true if successful, false otherwise
     */
    bool (*gpio_unmap_regs)(void);

    /* mmaps the pwm registers and sets up the supported
     * hard pwm values in pwm_map.
     * @param   pwm_map   Pointer to pwm_t structure.
     * @return EOK if successful, errno otherwise
     */
    int  (*pwm_init)(void **pwm_map);

    /* Sets up the range, mode and enables pwm for the GPIO
     * @param   p         Pointer to pwm_t structure.
     * @param   msg       Pointer to rpi_gpio_pwm_t msg containing the information
     *                    to be set.
     * @return EOK if successful, errno otherwise
     */
    int  (*setup_pwm_hard)(void *pwm, rpi_gpio_pwm_t const * const msg);

    /* Sets up the range, mode and enables pwm for the GPIO
     * @param   p         Pointer to pwm_t structure.
     * @param   msg       Pointer to rpi_gpio_pwm_t msg containing the information
     *                    to be set.
     * @return EOK if successful, errno otherwise
     */
    void (*set_pwm_duty)(void *p, unsigned const duty);

} gpio_config_t;


extern gpio_config_t *config;

/**
 * Programs the FSEL register for the given GPIO.
 * The value is one of the RPI_GPIO_FUNC_* constants.
 * @param   gpio    Pin number
 * @param   value   New pin function
 */
static inline void
rpi_gpio_set_select(uint32_t const gpio, uint32_t const value)
{
    config->set_select(gpio, value);
}

/**
 * Reads the FSEL register for the given GPIO.
 * @param   gpio    Pin number
 * @return  Current pin function
 */
static inline uint32_t
rpi_gpio_get_select(uint32_t const gpio)
{
    return config->get_select(gpio);
}

/**
 * Turns the pin on.  It is expected that the pin is in output mode.
 * This should be checked prior to calling this function
 * @param   gpio    Pin number
 */
static inline void
rpi_gpio_set(uint32_t const gpio)
{
    config->set(gpio);
}

/**
 * Turns the pin off.  It is expected that the pin is in output mode.
 * This should be checked prior to calling this function
 * @param   gpio    Pin number
 */
static inline void
rpi_gpio_clear(uint32_t gpio)
{
    config->clear(gpio);
}

/**
 * Sets or clears a pin.  It is expected that the pin is in output mode.
 * This should be checked prior to calling this function
 * @param   gpio    Pin number
 * @param   value   0 to clear, any other value to set
 */
static inline void
rpi_gpio_write(uint32_t const gpio, uint32_t const value)
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
static inline uint32_t
rpi_gpio_read(uint32_t const gpio)
{
    return config->read(gpio);
}

/**
 * Detect rising edge events on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static inline void
rpi_gpio_detect_rising_edge(uint32_t const gpio, bool const enable)
{
    config->detect_rising_edge(gpio, enable);
}

/**
 * Detect falling edge events on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static inline void
rpi_gpio_detect_falling_edge(uint32_t const gpio, bool const enable)
{
    config->detect_falling_edge(gpio, enable);
}

/**
 * Detect a high level on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static inline void
rpi_gpio_detect_level_high(uint32_t const gpio, bool const enable)
{
    config->detect_level_high(gpio, enable);
}

/**
 * Detect a low level on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static inline void
rpi_gpio_detect_level_low(uint32_t const gpio, bool const enable)
{
    config->detect_level_low(gpio, enable);
}

/* Clear the event detect for the GPIO
 * @param   gpio    GPIO number
 */
static inline void
rpi_gpio_event_clear_detect(uint32_t const gpio)
{
    config->event_clear_detect(gpio);
}

/* Clear all events and masks for all GPIOs.
 */
static inline void
rpi_gpio_clear_all_events(void)
{
    config->event_clear_all_events();
}

/**
 * Checks if an interrupt occurred for the given GPIO.
 * @param   gpio    Pin number
 * @return  true if interrupt occurred, false otherwise
 */
static inline bool
rpi_gpio_event_detected(uint32_t const gpio)
{
    return config->event_detected(gpio);
}

/**
 * Enables pull-up/pull-down detection on the given GPIO.
 * @param   gpio    GPIO number
 * @param   pud     One of the RPI_GPIO_PUD_* constants
 * @return  true if successful, false otherwise
 */
static inline bool
rpi_gpio_set_pud(uint32_t const gpio, uint32_t const pud)
{
    return config->set_pud(gpio, pud);
}

/**
 * Map the GPIO registers into the process' address space.
 * @param   base_paddr  Base physical address of the I/O space
 * @return  true if successful, false otherwise
 */
static inline bool
rpi_gpio_map_regs(uintptr_t const base_paddr)
{
    return config->gpio_map_regs(base_paddr);
}

/**
 * Unmap the GPIO registers.
 * @return  true if successful, false otherwise
 */
static inline bool
rpi_gpio_unmap_regs(void)
{
    return config->gpio_unmap_regs();
}

/* mmaps the pwm registers and sets up the supported
 * hard pwm values in pwm_map.
 * @param   pwm_map   Pointer to pwm_t structure.
 * @return EOK if successful, errno otherwise
 */
static inline int
rpi_pwm_init(void **pwm_map)
{
    return config->pwm_init(pwm_map);
}

/* Sets up the range, mode and enables pwm for the GPIO
 * @param   p         Pointer to pwm_t structure.
 * @param   msg       Pointer to rpi_gpio_pwm_t msg containing the information
 *                    To be set.
 * @return EOK if successful, errno otherwise
 */
static inline int
rpi_setup_pwm_hard(void * pwm, rpi_gpio_pwm_t const * const msg)
{
    return config->setup_pwm_hard(pwm, msg);
}

/* Sets up the range, mode and enables pwm for the GPIO
 * @param   p         Pointer to pwm_t structure.
 * @param   msg       Pointer to rpi_gpio_pwm_t msg containing the information
 *                    to be set.
 * @return EOK if successful, errno otherwise
 */
static inline void
rpi_set_pwm_duty(void *pwm, unsigned const duty)
{
    config->set_pwm_duty(pwm, duty);
}

#endif
