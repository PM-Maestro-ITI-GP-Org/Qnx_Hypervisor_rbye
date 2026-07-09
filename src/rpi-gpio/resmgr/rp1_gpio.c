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

/* RaspberryPi 5 GPIO RP1 specific functionality */

#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <sys/mman.h>
#include <hw/inout.h>
#include <stdio.h>
#include "rpi_gpio.h"
#include "rpi_gpio_priv.h"



/* GPIO registers */
static uintptr_t gpio_base = (uintptr_t)MAP_FAILED;

/* pwm registers */
static uintptr_t  pwm_base = (uintptr_t)MAP_FAILED;
static uintptr_t  clk_pwm_base = (uintptr_t)MAP_FAILED;

/* pointer to MSIX, used to enable up interrupts */
static uintptr_t pcie_msix = (uintptr_t)MAP_FAILED;

extern int verbose;

/* RP1 GPIO io read*/
#define RP1_GPIO_IO_REG_STATUS_OFFSET(offset) ((((offset) * 2) + 0) * sizeof(uint32_t))
#define RP1_GPIO_IO_REG_CTRL_OFFSET(offset)   ((((offset) * 2) + 1) * sizeof(uint32_t))
#define RP1_GPIO_IO_REG_PCIE_INTE_OFFSET  0x11c
#define RP1_GPIO_IO_REG_PCIE_INTS_OFFSET  0x124

#define RP1_PADS_OD_SET       (1 << 7)
#define RP1_PADS_IE_SET       (1 << 6)
#define RP1_PADS_PUE_SET      (1 << 3)
#define RP1_PADS_PDE_SET      (1 << 2)

/* RP1 IRQ events*/
#define RP1_CTRL_IRQMASK                 0x0f  // Masks for the high/low level and high/low edge interrupt output
#define RP1_CTRL_IRQMASK_SHIFT           0x14  // Shift bit for mask

#define RP1_CTRL_IRQ_RESET_BIT           0x1c  // Reset interrupts - gpio_ctrl register set to 1 to reset the interrupt edge detector
#define RP1_CTRL_IRQMASK_LEVEL_HIGH      0x17  // Masks high level interrupt into the interrupt output
#define RP1_CTRL_IRQMASK_LEVEL_LOW       0x16  // Masks low level interrupt into the interrupt output
#define RP1_CTRL_IRQMASK_EDGE_HIGH       0x15  // Masks edge high interrupt into the interrupt output
#define RP1_CTRL_IRQMASK_EDGE_LOW        0x14  // Masks edge low interrupt into the interrupt output

#define RP1_STATUS_EVENT_LEVEL_HIGH_BIT  0x17  // reset = 0x0
#define RP1_STATUS_EVENT_LEVEL_LOW_BIT   0x16  // reset = 0x0
#define RP1_STATUS_EVENT_EDGE_HIGH_BIT   0x15  // reset with irqreset
#define RP1_STATUS_EVENT_EDGE_LOW_BIT    0x14  // reset with irqreset

/* RP1 GPIO pads read*/
#define RP1_GPIO_PADS_REG_OFFSET(offset)      (sizeof(uint32_t) + ((offset) * sizeof(uint32_t)))

/* RP1 GPIO sys_io read*/
#define RP1_GPIO_SYS_RIO_REG_OUT_OFFSET        0x0
#define RP1_GPIO_SYS_RIO_REG_OE_OFFSET         0x4
#define RP1_GPIO_SYS_RIO_REG_SYNC_IN_OFFSET    0x8

#define RP1_SET_OFFSET 0x2000
#define RP1_CLR_OFFSET 0x3000

// RP1 GPIO base address
#define RP1_GPIO_BASE              0x1f000d0000
#define BLOCK_SIZE                 (0x2c000)
#define RP1_IO_BANK0_OFFSET        0x00000000
#define RP1_IO_BANK1_OFFSET        0x00004000
#define RP1_IO_BANK2_OFFSET        0x00008000
#define RP1_SYS_RIO_BANK0_OFFSET   0x00010000
#define RP1_SYS_RIO_BANK1_OFFSET   0x00014000
#define RP1_SYS_RIO_BANK2_OFFSET   0x00018000
#define RP1_PADS_BANK0_OFFSET      0x00020000
#define RP1_PADS_BANK1_OFFSET      0x00024000
#define RP1_PADS_BANK2_OFFSET      0x00028000

// RP1 MSIX
#define RP1_PCIE_MSIX_ADDR                0x1f00108000
#define RP1_PCIE_MSIX_SIZE                0x4000
#define RP1_PCIE_MSIX_CFG(irq)            (0x08 + ((irq) * 0x04))
#define RP1_PCIE_MSIX_CFG_IACK            (1u << 2)
#define RP1_PCIE_MSIX_CFG_SET             0x800

/* PWM Clock
 */
#define RP1_CLOCK_MAIN_BASE             (0x1f00018000ULL)
#define RP1_CLOCK_MAIN_SIZE             (0x400ULL)
#define RP1_TIMER_BASE                  (0x107c003000)
#define RP1_CLK_SET_ENABLE              0x11000840

#define RP1_PWM_MODE_PULSE_DENSITY      0x03
#define RP1_PWM_MODE_TRAILING_EDGE_MS   0x01

/* RPi5 pwm clock runs at 50MHz */
#define RP1_PWM_CLOCK_OSCILLATOR        50000000

#define RP1_CLK_PWM0_CTRL               0x74
#define RP1_CLK_PWM0_DIV_INT            0x78
#define RP1_CLK_PWM0_DIV_FRAC           0x7c
#define RP1_CLK_PWM0_SEL                0x80

#define RP1_CLK_CTRL_ENABLE             0x800  /* bit 11 -- 1 == enable , 0 == false */

#define RP1_CLK_PWM1_CTRL               0x84
#define RP1_CLK_PWM1_DIV_INT            0x88
#define RP1_CLK_PWM1_DIV_FRAC           0x8c
#define RP1_CLK_PWM1_SEL                0x90

/* RP1 PWM registers */
#define RP1_PWM0_BASE                   (0x1f00098000ULL)
#define RP1_PWM_SIZE                    0x100

#define RP1_PWM_GLOBAL_CTRL             0x00  /* Global control bits */
#define RP1_PWM_FIFO_CTRL               0x04  /* FIFO thresholding and status */
#define RP1_PWM_COMMON_RANGE            0x08
#define RP1_PWM_COMMON_DUTY             0x0c

/* Channel register offsets for RPi5.
 * RP1 supports 4 channels
 */
#define RP1_PWM_CHAN_CTRL(channel)     ((((channel) - 1) * 0x10) + 0x14)
#define RP1_PWM_CHAN_RANGE(channel)    ((((channel) - 1) * 0x10) + 0x18)
#define RP1_PWM_CHAN_PHASE(channel)    ((((channel) - 1) * 0x10) + 0x1C)
#define RP1_PWM_CHAN_DUTY(channel)     ((((channel) - 1) * 0x10) + 0x20)

/* RP1 pwm global control register */
#define RP1_PWM_GLOBAL_CTRL_UPDATE          (0x80000000)
#define RP1_PWM_GLOBAL_CTRL_CHANNEL_EN(channel)   (1 << ((channel) - 1))  /* channel enable bit */

#define RP1_GPIO_FUNC_MAX 10

#define RP1_FSEL_SYS_RIO   (RP1_GPIO_FUNC_ALT_5)

/* boolean to keep track if the pwm clock has been
 * initialized or not
 */
bool  init_pwm_clk = true;



/**
 * Pin function. Different from RPi4
 */
enum
{
    RP1_GPIO_FUNC_ALT_0 = 0,
    RP1_GPIO_FUNC_ALT_1 = 1,
    RP1_GPIO_FUNC_ALT_2 = 2,
    RP1_GPIO_FUNC_ALT_3 = 3,
    RP1_GPIO_FUNC_ALT_4 = 4,
    RP1_GPIO_FUNC_ALT_5 = 5,
    RP1_GPIO_FUNC_ALT_6 = 6,
    RP1_GPIO_FUNC_ALT_7 = 7,
    RP1_GPIO_FUNC_ALT_8 = 8,
    RP1_GPIO_FUNC_IN = 20,
    RP1_GPIO_FUNC_OUT = 21,
    RP1_GPIO_FUNC_GP = 22,
    RP1_GPIO_FUNC_NO = 23,
    RP1_GPIO_FUNC_NULL = 0x1f
};

/**
 * Pull up/down values.
 */
enum
{
    RP1_GPIO_PUD_OFF = 0,
    RP1_GPIO_PUD_DOWN = 1,
    RP1_GPIO_PUD_UP = 2
};


/* RPi5 pwm hardware GPIOs */
static pwm_t rp1_pwm_gpio_12 = {
    .gpio = 12,
    .channel = 1,
    .func = RPI_GPIO_FUNC_ALT_0
};

static pwm_t rp1_pwm_gpio_13 = {
    .gpio = 13,
    .channel = 2,
    .func = RPI_GPIO_FUNC_ALT_0
};

static pwm_t    rp1_pwm_gpio_14 = {
    .gpio = 14,
    .channel = 3,
    .func = RPI_GPIO_FUNC_ALT_0
};

static pwm_t    rp1_pwm_gpio_15 = {
    .gpio = 15,
    .channel = 4,
    .func = RPI_GPIO_FUNC_ALT_0
};

static pwm_t    rp1_pwm_gpio_18 = {
    .gpio = 18,
    .channel = 3,
    .func = RPI_GPIO_FUNC_ALT_3
};

static pwm_t    rp1_pwm_gpio_19 = {
    .gpio = 19,
    .channel = 4,
    .func = RPI_GPIO_FUNC_ALT_3
};

/**
 * Mapped GPIO registers.
 * An executable including this header is responsible for defining this global
 * in a source file.
 */
static const uint32_t gpio_io_bank_offset[] = { RP1_IO_BANK0_OFFSET, RP1_IO_BANK1_OFFSET, RP1_IO_BANK2_OFFSET };
static const uint32_t gpio_pads_bank_offset[] = { RP1_PADS_BANK0_OFFSET, RP1_PADS_BANK1_OFFSET, RP1_PADS_BANK2_OFFSET };
static const uint32_t gpio_sys_rio_bank_offset[] = { RP1_SYS_RIO_BANK0_OFFSET, RP1_SYS_RIO_BANK1_OFFSET, RP1_SYS_RIO_BANK2_OFFSET };
static const uint32_t rp1_bank_base[] = {0, 28, 34};

static void rp1_gpio_get_bank_offset(const uint32_t gpio, uint32_t *bank, uint32_t *offset)
{
    *bank = 0;
    *offset = 0;

    if (gpio < rp1_bank_base[1]) {
        *bank = 0;
    }
    else if (gpio < rp1_bank_base[2]) {
        *bank = 1;
    }
    else {
        *bank = 2;
    }

   *offset = gpio - rp1_bank_base[*bank];
}


/**
 * Programs the FSEL register for the given GPIO.
 * The value is one of the RPI_GPIO_FUNC_* constants.
 * @param   gpio    Pin number
 * @param   value   New pin function
 */
static void
rp1_gpio_set_select(uint32_t const gpio, uint32_t const value)
{
    uint32_t bank, offset;
    int fsel;
    uint32_t io_ctrl_val;
    uint32_t orig_pad_val, pad_val;
    uint32_t reg;
    uint32_t rpi5_value;

    /* Convert the value to an RPi5 value.
     */
    switch(value) {
        case RPI_GPIO_FUNC_IN:
            rpi5_value = RP1_GPIO_FUNC_IN;
            break;
        case RPI_GPIO_FUNC_OUT:
            rpi5_value = RP1_GPIO_FUNC_OUT;
            break;
        case RPI_GPIO_FUNC_ALT_0:
            rpi5_value = RP1_GPIO_FUNC_ALT_0;
            break;
        case RPI_GPIO_FUNC_ALT_1:
            rpi5_value = RP1_GPIO_FUNC_ALT_1;
            break;
        case RPI_GPIO_FUNC_ALT_2:
            rpi5_value = RP1_GPIO_FUNC_ALT_2;
            break;
        case RPI_GPIO_FUNC_ALT_3:
            rpi5_value = RP1_GPIO_FUNC_ALT_3;
            break;
        case RPI_GPIO_FUNC_ALT_4:
            rpi5_value = RP1_GPIO_FUNC_ALT_4;
            break;
        case RPI_GPIO_FUNC_ALT_5:
            rpi5_value = RP1_GPIO_FUNC_ALT_5;
            break;
        case RPI_GPIO_FUNC_ALT_6:
            rpi5_value = RP1_GPIO_FUNC_ALT_6;
            break;
        case RPI_GPIO_FUNC_ALT_7:
            rpi5_value = RP1_GPIO_FUNC_ALT_7;
            break;
        case RPI_GPIO_FUNC_ALT_8:
            rpi5_value = RP1_GPIO_FUNC_ALT_8;
            break;
        default:
            printf("Unsupported input %d\n", value);
            return;
    }

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);

    if (rpi5_value == RP1_GPIO_FUNC_IN) { /* input */
        reg = gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OE_OFFSET + RP1_CLR_OFFSET;
        out32(gpio_base + reg, (1U << offset));

    } else if (rpi5_value == RP1_GPIO_FUNC_OUT) { /* output */
        reg = gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OE_OFFSET + RP1_SET_OFFSET;
        out32(gpio_base + reg, (1U << offset));
    }

    fsel = rpi5_value;
    if (rpi5_value == RP1_GPIO_FUNC_IN || rpi5_value == RP1_GPIO_FUNC_OUT || rpi5_value == RP1_GPIO_FUNC_GP) {
        fsel = RP1_FSEL_SYS_RIO;
    }
    else if (rpi5_value == RP1_GPIO_FUNC_NO) {
        fsel = RP1_GPIO_FUNC_NULL;
    }
    reg = gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset);
    io_ctrl_val = in32(gpio_base + reg);
    io_ctrl_val &= ~(0x1f);
    io_ctrl_val |= (fsel & 0x1f);
    out32(gpio_base + reg, io_ctrl_val);

    pad_val = in32(gpio_base + gpio_pads_bank_offset[bank] + RP1_GPIO_PADS_REG_OFFSET(offset));

    orig_pad_val = pad_val;
    reg = gpio_pads_bank_offset[bank] + RP1_GPIO_PADS_REG_OFFSET(offset);
    if (fsel == RP1_GPIO_FUNC_NULL) {
        /* Disable input */
        pad_val &= ~RP1_PADS_IE_SET;
        // Disable peripheral func output
        pad_val |= RP1_PADS_OD_SET;

    } else {
        // Enable input
        pad_val |= RP1_PADS_IE_SET;
        // Enable peripheral func output
        pad_val &= ~RP1_PADS_OD_SET;
    }
    if (pad_val != orig_pad_val) {
        out32(gpio_base + reg, pad_val);
    }
}

/**
 * Checks if an interrupt occurred for the given GPIO.
 * @param   gpio    Pin number
 * @return  true if interrupt occurred, false otherwise
 */
static bool
rp1_gpio_event_detected(uint32_t const gpio)
{
    uint32_t bank, offset;
    uint32_t reg;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);
    reg = gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_PCIE_INTS_OFFSET;

    // Status for interrupts.
    uint32_t pci_ints = in32(gpio_base + reg);

    if ( pci_ints & (1U << offset) ) {
        return true;
    } else {
        return false;
    }
}

/**
 * Reads the FSEL register for the given GPIO.
 * @param   gpio    Pin number
 * @return  Current pin function.
 */
static uint32_t
rp1_gpio_get_select(uint32_t const gpio)
{
    uint32_t bank, offset;
    uint32_t io_val;
    uint32_t sys_rio_oe_read;


    rp1_gpio_get_bank_offset(gpio, &bank, &offset);

    uint32_t reg = gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset);

    io_val = in32(gpio_base + reg);
    io_val &= 0x1f;

    if (io_val != RP1_FSEL_SYS_RIO) {
        /* convert to be backwards compatible */
        uint32_t val;
        switch(io_val) {
            case RP1_GPIO_FUNC_ALT_0:
                val = RPI_GPIO_FUNC_ALT_0;
                break;
            case RP1_GPIO_FUNC_ALT_1:
                val = RPI_GPIO_FUNC_ALT_1;
                break;
            case RP1_GPIO_FUNC_ALT_2:
                val = RPI_GPIO_FUNC_ALT_2;
                break;
            case RP1_GPIO_FUNC_ALT_3:
                val = RPI_GPIO_FUNC_ALT_3;
                break;
            case RP1_GPIO_FUNC_ALT_4:
                val = RPI_GPIO_FUNC_ALT_4;
                break;
            case RP1_GPIO_FUNC_ALT_5:
                val = RPI_GPIO_FUNC_ALT_5;
                break;
            case RP1_GPIO_FUNC_ALT_6:
                val = RPI_GPIO_FUNC_ALT_6;
                break;
            case RP1_GPIO_FUNC_ALT_7:
                val = RPI_GPIO_FUNC_ALT_7;
                break;
            case RP1_GPIO_FUNC_ALT_8:
                val = RPI_GPIO_FUNC_ALT_8;
                break;
            case RP1_GPIO_FUNC_IN:
                val = RPI_GPIO_FUNC_IN;
                break;
            case RP1_GPIO_FUNC_OUT:
                val = RPI_GPIO_FUNC_OUT;
                break;
            case RP1_GPIO_FUNC_NULL:
                val = RPI_GPIO_FUNC_NULL;
                break;
            default:
                val = io_val;
                break;
        }
        return val;
    }

    reg = gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OE_OFFSET;
    sys_rio_oe_read = in32(gpio_base + reg);

    if (sys_rio_oe_read & (1U << offset)) {
        return RPI_GPIO_FUNC_OUT;
    } else {
        return RPI_GPIO_FUNC_IN;
    }
}

/**
 * Turns the pin on.  It is expected that the pin is in output mode.
 * This should be checked prior to calling this function
 * @param   gpio    Pin number
 */
static void
rp1_gpio_set(uint32_t const gpio)
{
    uint32_t bank, offset;
    uint32_t reg;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);
    reg = gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OUT_OFFSET;
    out32(gpio_base + reg + RP1_SET_OFFSET, (1U << offset));
}

/**
 * Turns the pin off.  It is expected that the pin is in output mode.
 * This should be checked prior to calling this function
 * @param   gpio    Pin number
 */
static void
rp1_gpio_clear(uint32_t gpio)
{
    uint32_t bank, offset;
    uint32_t reg;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);
    reg = gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OUT_OFFSET;
    out32(gpio_base + reg + RP1_CLR_OFFSET, (1U << offset));
}

/**
 * Sets or clears a pin.  It is expected that the pin is in output mode.
 * This should be checked prior to calling this function
 * @param   gpio    Pin number
 * @param   value   0 to clear, any other value to set
 */
static void
rp1_gpio_write(uint32_t const gpio, uint32_t const value)
{
    if (value) {
        rp1_gpio_set(gpio);
    } else {
        rp1_gpio_clear(gpio);
    }
}

/**
 * Reads the level of a pin in input mode.
 * @param   gpio    Pin number
 * @return  0 if the pin is low, 1 if high
 */
static uint32_t
rp1_gpio_read(uint32_t const gpio)
{
    uint32_t bank, offset;
    uint32_t reg;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);

    reg = gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_SYNC_IN_OFFSET;
    uint value = in32(gpio_base + reg);

    return ( (value & (1U << offset)) ? 1 : 0 );
}

static void
rp1_gpio_detect(uint32_t const gpio, bool const enable, uint32_t mask)
{
    uint32_t bank, offset;
    uint32_t reg;
    uint32_t io_pcie_val;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);

    reg = gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset);

    if(enable) {
        out32(gpio_base + reg + RP1_SET_OFFSET, (1U << mask));
    } else {
        out32(gpio_base + reg + RP1_CLR_OFFSET, (1U << mask));
        out32(gpio_base + reg + RP1_SET_OFFSET, (1U << RP1_CTRL_IRQ_RESET_BIT));
    }
    // set the pcie value
    reg = gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_PCIE_INTE_OFFSET;
    io_pcie_val = in32(gpio_base + reg);
    if (enable) {
        io_pcie_val |= (1U << offset);
    } else {
        io_pcie_val &= ~(1U << offset);
    }
    out32(gpio_base + reg, io_pcie_val);
}
/**
 * Detect rising edge events on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static void
rp1_gpio_detect_rising_edge(uint32_t const gpio, bool const enable)
{
    rp1_gpio_detect(gpio,enable,RP1_CTRL_IRQMASK_EDGE_HIGH);
}

/**
 * Detect falling edge events on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static void
rp1_gpio_detect_falling_edge(uint32_t const gpio, bool const enable)
{
    rp1_gpio_detect(gpio,enable, RP1_CTRL_IRQMASK_EDGE_LOW);
}

/**
 * Detect a high level on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static void
rp1_gpio_detect_level_high(uint32_t const gpio, bool const enable)
{
    rp1_gpio_detect(gpio,enable, RP1_CTRL_IRQMASK_LEVEL_HIGH);
}

/**
 * Detect a low level on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static void
rp1_gpio_detect_level_low(uint32_t const gpio, bool const enable)
{
    rp1_gpio_detect(gpio,enable, RP1_CTRL_IRQMASK_LEVEL_LOW);

}

/* Clear the event detect for the given GPIO
 * @param   gpio    GPIO number
 */
static void
rp1_gpio_event_clear_detect(uint32_t const gpio)
{
    uint32_t bank, offset;
    uint32_t reg;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);

    /* clear the edge event by setting the ctrl irqreset bit to 1 */
    reg = gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset);
    out32(gpio_base + reg + RP1_SET_OFFSET, (1U << RP1_CTRL_IRQ_RESET_BIT));

    /* Reset the IACK bit */
    uint32_t val = in32(pcie_msix + RP1_PCIE_MSIX_CFG_SET + RP1_PCIE_MSIX_CFG(0));
    val |= RP1_PCIE_MSIX_CFG_IACK;
    out32(pcie_msix + RP1_PCIE_MSIX_CFG_SET + RP1_PCIE_MSIX_CFG(0), val);
}

/* Clear the interrupt mask for the given GPIO
 * @param   gpio    GPIO number
 */
static void
rp1_gpio_event_clear_event_mask(uint32_t const gpio)
{
    uint32_t bank, offset;
    uint32_t reg;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);
    reg = gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset);
    // clear mask.
    out32(gpio_base + reg + RP1_CLR_OFFSET, (RP1_CTRL_IRQMASK << RP1_CTRL_IRQMASK_SHIFT));
}

/* Clear all events and masks for all GPIOs.
 */
static void
rp1_gpio_clear_all_events(void) 
{
    for (unsigned gpio = 0; gpio < RPI_GPIO_NUM; gpio++) {
        rp1_gpio_event_clear_detect(gpio);
        rp1_gpio_event_clear_event_mask(gpio);
    }
}

/**
 * Enables pull-up/pull-down detection on the given GPIO.
 * @param   gpio    GPIO number
 * @param   pud     One of the RPI_GPIO_PUD_* constants
 * @return  true if successful, false otherwise
 */
static bool
rp1_gpio_set_pud(uint32_t const gpio, uint32_t const pud)
{
    switch (pud) {
    case RP1_GPIO_PUD_OFF:
    case RP1_GPIO_PUD_UP:
    case RP1_GPIO_PUD_DOWN:
        break;
    default:
        return false;
    }

    uint32_t bank, offset;
    uint32_t pud_val;
    rp1_gpio_get_bank_offset(gpio, &bank, &offset);

    uint32_t const  reg = gpio_pads_bank_offset[bank] + RP1_GPIO_PADS_REG_OFFSET(offset);

    pud_val = in32(gpio_base + reg);

    pud_val &= ~(RP1_PADS_PDE_SET | RP1_PADS_PUE_SET);

    if (pud == RP1_GPIO_PUD_UP){
        pud_val |= RP1_PADS_PUE_SET;
    }
    else if (pud == RP1_GPIO_PUD_DOWN) {
        pud_val |= RP1_PADS_PUE_SET;
    }
    out32(gpio_base + reg, pud_val);

    return true;
}


/**
 * Map the GPIO registers into the process' address space.
 * @param   base_paddr  Base physical address of the I/O space
 * @return  true if successful, false otherwise
 */
static bool
rp1_gpio_map_regs(uintptr_t const base_paddr)
{

    if (gpio_base != (uintptr_t)MAP_FAILED) {
        return true;
    }
    gpio_base = (uintptr_t)mmap_device_memory(NULL, BLOCK_SIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, RP1_GPIO_BASE);
    if (gpio_base == (uintptr_t) MAP_FAILED) {
        return false;
    }
    /* Enable interrupts for rp1 events. */
    pcie_msix = (uintptr_t) mmap_device_memory(NULL,
                RP1_PCIE_MSIX_SIZE,
                PROT_NOCACHE|PROT_READ|PROT_WRITE,
                0,
                RP1_PCIE_MSIX_ADDR);
    if (pcie_msix == (uintptr_t) MAP_FAILED) {
        printf("mmap PCIE MSIX failed\n");
        return false;
    }
    out32(pcie_msix + RP1_PCIE_MSIX_CFG_SET + RP1_PCIE_MSIX_CFG(0), RP1_PCIE_MSIX_CFG_IACK);

    return true;
}

/**
 * Unmap the GPIO registers.
 * @return  true if successful, false otherwise
 */
static bool
rp1_gpio_unmap_regs(void)
{
    bool unmap = true;

    if (gpio_base != (uintptr_t)MAP_FAILED) {
        unmap &= (munmap_device_memory((void *)gpio_base, BLOCK_SIZE) == 0);
        gpio_base = (uintptr_t)MAP_FAILED;
    }
    if (pcie_msix != (uintptr_t)MAP_FAILED) {
        unmap &= (munmap_device_memory((void *)pcie_msix, RP1_PCIE_MSIX_SIZE) == 0);
        pcie_msix = (uintptr_t)MAP_FAILED;
    }
    if (clk_pwm_base != (uintptr_t)MAP_FAILED) {
        unmap &= (munmap_device_io(clk_pwm_base, RP1_CLOCK_MAIN_SIZE) == 0);
        clk_pwm_base = (uintptr_t)MAP_FAILED;
    }
    if (pwm_base != (uintptr_t)MAP_FAILED) {
        unmap &= (munmap_device_io(pwm_base, RP1_PWM_SIZE) == 0);
        pwm_base = (uintptr_t)MAP_FAILED;
    }
    return unmap;
}

/* Initializes the pwm clock.  Needed for pwm support.
 * @return EOK if successful, errno otherwise
 */
static int pwm_clock_init(void)
{
    /*
     * set RP1 PWM1 clock
     */
    if (clk_pwm_base != (uintptr_t)MAP_FAILED) {
        return true;
    }
    clk_pwm_base = mmap_device_io(RP1_CLOCK_MAIN_SIZE, RP1_CLOCK_MAIN_BASE );
    if (clk_pwm_base == (uintptr_t)MAP_FAILED) {
        printf("Couldn't mmap rp1 clock");
        return errno;
    }

    // Need to enable the clock
    out32(clk_pwm_base + RP1_CLK_PWM0_CTRL, RP1_CLK_SET_ENABLE);

    return EOK;
}

/* mmaps the pwm registers and sets up the supported
 * hard pwm values in pwm_map.
 * @param   pwm_map   Pointer to pwm_t structure.
 * @return EOK if successful, errno otherwise
 */
static int rp1_pwm_init(void **pwm_map) 
{
    pwm_t **map = (pwm_t **)pwm_map;

    if (pwm_base == (uintptr_t)MAP_FAILED) {
        pwm_base = mmap_device_io(RP1_PWM_SIZE, RP1_PWM0_BASE);
        if (pwm_base == (uintptr_t)MAP_FAILED) {
            perror("mmap");
            return 1;
        }
    }
    if(pwm_clock_init()) {
        return errno;  
    }

    /* Setup the hard gpio pins */
    map[12] = &rp1_pwm_gpio_12;
    map[13] = &rp1_pwm_gpio_13;
    map[14] = &rp1_pwm_gpio_14;
    map[15] = &rp1_pwm_gpio_15;
    map[18] = &rp1_pwm_gpio_18;
    map[19] = &rp1_pwm_gpio_19;

    return 0;
} 

/* Sets up the range, mode and enables pwm for the GPIO.
 * @param   p         Pointer to pwm_t structure.
 * @param   msg       Pointer to rpi_gpio_pwm_t msg containing the information
 *                    to be set.
 * @return EOK if successful, errno otherwise
 */
static int rp1_setup_pwm_hard(void * p, rpi_gpio_pwm_t const * const msg)
{
    pwm_t *pwm = (pwm_t*) p;
    unsigned const  frequency = msg->frequency;
    unsigned const  range = msg->range;

    if (range > 4095) {
        return EINVAL;
    }

    uint32_t    pwm_global_ctrl;
    uint32_t    pwm_ctrl;

    switch (msg->mode) {
    case RPI_PWM_MODE_PWM:
        pwm_ctrl = RP1_PWM_MODE_PULSE_DENSITY;
        break;

    case RPI_PWM_MODE_MS:
        pwm_ctrl = RP1_PWM_MODE_TRAILING_EDGE_MS;
        break;

    default:
        fprintf(stderr, "Unsupported PWM mode: %d\n", msg->mode);
        return EINVAL;
    }

    uint32_t const oscillator = RP1_PWM_CLOCK_OSCILLATOR;

    /* Enable the clk_pwm */
    if (init_pwm_clk) {
        uint32_t pwm0_clk_ctrl = in32(clk_pwm_base + RP1_CLK_PWM0_CTRL);
        pwm0_clk_ctrl |= RP1_CLK_CTRL_ENABLE;
        out32(clk_pwm_base + RP1_CLK_PWM0_CTRL, pwm0_clk_ctrl);

        /* Reset the divisor.
         * For RPi5 we use the default divisor values
         * and adjust the range and duty accordingly.
         */
        out32(clk_pwm_base + RP1_CLK_PWM0_DIV_INT, 1);
        out32(clk_pwm_base + RP1_CLK_PWM0_DIV_FRAC, 0);
        init_pwm_clk = false;
    }

    /* Set the range based on the clock frequency and requested frequency.
     */
    uint32_t const adj_range = oscillator / frequency;
    out32(pwm_base +  RP1_PWM_CHAN_RANGE((pwm->channel)), adj_range);
    pwm->adj_range = adj_range;

    /* Set the pwm channel ctrl bind bit to use the range and duty
     * set below.  If not set the global channel and range value will
     * be used by default.
     */
    out32(pwm_base + RP1_PWM_CHAN_CTRL((pwm->channel)), pwm_ctrl);

    /* Set phase to 0 */
    out32(pwm_base + RP1_PWM_CHAN_PHASE((pwm->channel)), 0);

    /* Set duty to 0 */
    out32(pwm_base + RP1_PWM_CHAN_DUTY((pwm->channel)), 0);

    /* enable the channel */
    pwm_global_ctrl = in32(pwm_base + RP1_PWM_GLOBAL_CTRL);
    pwm_global_ctrl |= RP1_PWM_GLOBAL_CTRL_UPDATE | RP1_PWM_GLOBAL_CTRL_CHANNEL_EN((pwm->channel));
    out32(pwm_base + RP1_PWM_GLOBAL_CTRL, pwm_global_ctrl);

    rpi_gpio_set_select(pwm->gpio, pwm->func);

    return 0;
}

/* Sets duty value for the given pwm
 * @param   p         Pointer to pwm_t structure.
 * @param   duty      Duty value to set.
 *                    to be set.
 * @return EOK if successful, errno otherwise
 */
static void
rp1_set_pwm_duty(void *p, unsigned const duty)
{
    pwm_t *pwm = (pwm_t *)p;
    uint32_t const adj_duty = (uint32_t)((uint64_t)(pwm->adj_range) * duty / pwm->range);
    out32(pwm_base + RP1_PWM_CHAN_DUTY((pwm->channel)), adj_duty);
    pwm->adj_duty = adj_duty;
    pwm->duty = duty;
}

/* RPi5 GPIO configuration
 */
gpio_config_t gpio_config_rpi5 = {
  .version = RPI_VER_5,
  .num_gpios = RPI_GPIO_NUM,
  .max_funcs = 10,
  .intr_offset = 160,
  .timer_intr_offset = 97,
  .base_paddr = RPI5_BASE_PADDR,
  .base_timer_paddr = RP1_TIMER_BASE,

  .set_select = rp1_gpio_set_select,
  .get_select = rp1_gpio_get_select,
  .set = rp1_gpio_set,
  .clear = rp1_gpio_clear,
  .read = rp1_gpio_read,
  .write = rp1_gpio_write,
  .detect_rising_edge = rp1_gpio_detect_rising_edge,
  .detect_falling_edge = rp1_gpio_detect_falling_edge,
  .detect_level_high = rp1_gpio_detect_level_high,
  .detect_level_low = rp1_gpio_detect_level_low,
  .event_clear_detect = rp1_gpio_event_clear_detect,
  .event_clear_all_events = rp1_gpio_clear_all_events,
  .event_detected = rp1_gpio_event_detected,
  .set_pud = rp1_gpio_set_pud,
  .gpio_map_regs = rp1_gpio_map_regs,
  .gpio_unmap_regs = rp1_gpio_unmap_regs,
  .pwm_init = rp1_pwm_init,
  .setup_pwm_hard = rp1_setup_pwm_hard,
  .set_pwm_duty = rp1_set_pwm_duty
};
