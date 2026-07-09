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
 * @file    event.c
 * @brief   GPIO interrupt handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/rpi_gpio.h>
#include <aarch64/inline.h>
#include "rpi_gpio.h"
#include "rpi_gpio_priv.h"

extern int  verbose;

typedef struct event_entry  event_entry_t;

struct event_entry
{
    /** Client information. */
    client_info_t   ci;
    /** Event detection type (level/edge, rising/falling). */
    uint32_t        detect;
    /** Total number of changes detected. */
    uint32_t        count;
    /** Number of detected changes after which to deliver the event. */
    uint32_t        match;
    /** Number of detected changes towards the match value. */
    uint32_t        match_count;
    /** Event to deliver to the client. */
    struct sigevent sigev;
};

static pthread_t            ist_tid;
static sem_t                ist_sem;
static int                  ist_status;
static pthread_mutex_t      event_mutex = PTHREAD_MUTEX_INITIALIZER;
static event_entry_t        event_table[RPI_GPIO_NUM];

/**
 * Deliver a GPIO event to a registered client.
 * @param   gpio    The GPIO for which the event occurred
 */
static void
dispatch_event(unsigned const gpio)
{
    if (event_table[gpio].detect == RPI_EVENT_NONE) {
        return;
    }

    // Get the GPIO value.
    unsigned const  value = rpi_gpio_read(gpio);
    if (value) {
        event_table[gpio].sigev.sigev_value.sival_int = gpio;
    } else {
        event_table[gpio].sigev.sigev_value.sival_int = -gpio;
    }

    // Update counts.
    event_table[gpio].count++;
    event_table[gpio].match_count++;
    if (event_table[gpio].match_count < event_table[gpio].match) {
        return;
    }

    event_table[gpio].match_count = 0;

    if (event_table[gpio].sigev.sigev_notify == SIGEV_NONE) {
        return;
    }

    // Deliver the event to the client thread.
    if (MsgDeliverEvent(event_table[gpio].ci.rcvid,
                        &event_table[gpio].sigev) == -1) {
        // Disable future event deliveries to this receive ID.
        event_table[gpio].detect = RPI_EVENT_NONE;
        if (verbose) {
            fprintf(stderr, "Failed to deliver event to %lx: %s\n",
                    event_table[gpio].ci.rcvid,
                    strerror(errno));
        }
    }
}

/**
 * Service thread for GPIO interrupts.
 * Waits for the interrupt, detects which GPIOs have changed state and then
 * dispatches registered events for each GPIO.
 * @param   arg     Interrupt number
 * @return  Always NULL
 */
static void *
gpio_ist(void *arg)
{
    int const intr = (int)(uintptr_t)arg;

    // Register an interrupt event.
    struct sigevent ev;
    SIGEV_INTR_INIT(&ev);

    int const   intid = InterruptAttachEvent(intr, &ev, 0);
    if (intid == -1) {
        ist_status = errno;
        sem_post(&ist_sem);
        return NULL;
    }

    sem_post(&ist_sem);

    // Handle interrupt events.
    for (;;) {
        // Wait for an interrupt event.
        if (InterruptWait(_NTO_INTR_WAIT_FLAGS_FAST, NULL) != 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("InterruptWait");
            abort();
        }

        // Clear any detected events before unmasking the interrupt.
        int const   rc = pthread_mutex_lock(&event_mutex);
        if (rc != EOK) {
            // Should never happen.
            abort();
        }
        for (unsigned gpio = 0; gpio < RPI_GPIO_NUM; gpio++) {
            bool detected = false;
            if ((event_table[gpio].detect != RPI_EVENT_NONE) ) {
                if (verbose >= 2) {
                    fprintf(stderr, "Event detected gpio = %d\n", gpio);
                }
                detected = rpi_gpio_event_detected(gpio);
                if ((event_table[gpio].detect & RPI_EVENT_EDGE_RISING) || (event_table[gpio].detect & RPI_EVENT_EDGE_FALLING)) {
                    rpi_gpio_event_clear_detect(gpio);
                }
                if (detected) {
                    dispatch_event(gpio);
                }
                if ((event_table[gpio].detect & RPI_EVENT_LEVEL_HIGH) || (event_table[gpio].detect & RPI_EVENT_LEVEL_LOW)) {
                    rpi_gpio_event_clear_detect(gpio);
                }
            }
        }
        pthread_mutex_unlock(&event_mutex);

        // Unmask the interrupt to get the next event.
        if (InterruptUnmask(intr, intid) == -1) {
            perror("InterruptUnmask");
            abort();
        }
    }

    return NULL;
}

int
event_init(unsigned const priority, int const intr)
{
    // Clear and disable all events.
    for (unsigned gpio = 0; gpio < RPI_GPIO_NUM; gpio++) {
        client_info_reset(&event_table[gpio].ci);
    }
    rpi_gpio_clear_all_events();

    sem_init(&ist_sem, 0, 0);

    // Create a high-priority IST.
    pthread_attr_t  attr;
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    struct sched_param  param = { .sched_priority = priority };
    pthread_attr_setschedparam(&attr, &param);

    int rc = pthread_create(&ist_tid, &attr, gpio_ist, (void *)(uintptr_t)intr);
    if (rc != 0) {
        fprintf(stderr, "Failed to create IST: %s\n", strerror(rc));
        return 0;
    }

    // Wait for the IST to signal that it is either ready or has encountered an
    // error.
    sem_wait(&ist_sem);
    if (ist_status != 0) {
        fprintf(stderr, "IST failed to register interrupt: %s\n",
                strerror(ist_status));
        return 0;
    }

    return 1;
}

/**
 * Register an event to be delivered to the client thread.
 * @param   ci      Client information
 * @param   msg     An event message
 * @return  EOK if successful, error code otherwise
 */
int
event_add(client_info_t const *ci, rpi_gpio_event_t const * const msg)
{
    unsigned const  gpio = msg->gpio;
    unsigned const  detect = msg->detect;

    // Only the main thread can update the table, so we can examine it without
    // locking.
    if (!client_info_is_unused(&event_table[gpio].ci) &&
        !client_info_match(&event_table[gpio].ci, ci))
    {
        // Only one event per GPIO for now.
        return EBUSY;
    }

    // Disable all event detection for this GPIO.
    rpi_gpio_detect_rising_edge(gpio, false);
    rpi_gpio_detect_falling_edge(gpio, false);
    rpi_gpio_detect_level_high(gpio, false);
    rpi_gpio_detect_level_low(gpio, false);

    // Update the table.
    int const   rc = pthread_mutex_lock(&event_mutex);
    if (rc != 0) {
        abort();
    }

    event_table[gpio].ci = *ci;
    event_table[gpio].detect = detect;
    event_table[gpio].count = 0;
    event_table[gpio].match = msg->match == 0 ? 1 : msg->match;
    event_table[gpio].match_count = 0;
    memcpy(&event_table[gpio].sigev, &msg->event, sizeof(struct sigevent));

    pthread_mutex_unlock(&event_mutex);

    // Enable events.
    if (detect & RPI_EVENT_EDGE_RISING) {
        rpi_gpio_detect_rising_edge(gpio, true);
    }
    if (detect & RPI_EVENT_EDGE_FALLING) {
        rpi_gpio_detect_falling_edge(gpio, true);
    }
    if (detect & RPI_EVENT_LEVEL_HIGH) {
        rpi_gpio_detect_level_high(gpio, true);
    }
    if (detect & RPI_EVENT_LEVEL_LOW) {
        rpi_gpio_detect_level_low(gpio, true);
    }

    if (verbose) {
        fprintf(stderr, "%d/%d(%lx) added event %u/%u for GPIO %u\n",
                ci->scoid, ci->coid, ci->rcvid, detect,
                event_table[gpio].sigev.sigev_notify, gpio);
    }

    return 0;
}

void
event_remove_client(client_info_t const *ci)
{
    // Disable all events registered by this client.
    // We don't need the mutex for that.
    for (unsigned gpio = 0; gpio < RPI_GPIO_NUM; gpio++) {
        if (client_info_match(&event_table[gpio].ci, ci)) {
            rpi_gpio_detect_rising_edge(gpio, false);
            rpi_gpio_detect_falling_edge(gpio, false);
            rpi_gpio_detect_level_high(gpio, false);
            rpi_gpio_detect_level_low(gpio, false);
        }
    }
    // Remove events from the table.
    int const   rc = pthread_mutex_lock(&event_mutex);
    if (rc != 0) {
        abort();
    }

    for (unsigned gpio = 0; gpio< RPI_GPIO_NUM; gpio++) {
        if (client_info_match(&event_table[gpio].ci, ci)) {
            client_info_reset(&event_table[gpio].ci);
        }
    }

    pthread_mutex_unlock(&event_mutex);

    if (verbose) {
        fprintf(stderr, "Removed events for %d/%d\n", ci->scoid, ci->coid);
    }
}
