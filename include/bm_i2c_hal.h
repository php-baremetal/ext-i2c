/*
   +----------------------------------------------------------------------+
   | php-baremetal: ext-i2c                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2026 Gianfrancesco Aurecchia                           |
   +----------------------------------------------------------------------+
   | This source file is subject to the MIT license, that is bundled with |
   | this package in the file LICENSE.                                    |
   +----------------------------------------------------------------------+
   | Authors: Gianfrancesco Aurecchia <gianfri@php-baremetal.com>         |
   +----------------------------------------------------------------------+
*/

/*
 * Neutral I2C HAL: the platform side of ext-i2c, implemented by esp32-i2c-hal on ESP32. It owns the wire
 * handles, the bus locking, the routing to the bus owner, the background poller and the error names.
 */
#pragma once

#include "i2c_registry.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Open the wire for bus->port/sda/scl and store the HAL state in bus->hal. With bus->port ==
 * I2C_PORT_AUTO the HAL stores the port it picked in bus->port (or leaves AUTO if it cannot tell). */
bm_err_t bm_i2c_hal_bus_open(i2c_bus_t *bus);
void bm_i2c_hal_bus_close(i2c_bus_t *bus);

/* Attach d->addr at d->hz on d->bus and store the handle in d->hal. */
bm_err_t bm_i2c_hal_dev_open(i2c_dev_t *d);
void bm_i2c_hal_dev_close(i2c_dev_t *d);

typedef enum {
    BM_I2C_READ,
    BM_I2C_WRITE,
    BM_I2C_READ_REG,
    BM_I2C_WRITE_REG,
    BM_I2C_WRITE_REG1,
    BM_I2C_PROBE,
} bm_i2c_op_t;

typedef struct {
    bm_i2c_op_t    op;
    uint8_t       *rbuf;
    const uint8_t *wbuf;
    size_t         n;
    uint8_t        reg;
    uint8_t        val;
} bm_i2c_txn_t;

/* One transaction on d, routed by d->bus->owner (under the bus lock, or on the owner task). */
bm_err_t bm_i2c_hal_transfer(i2c_dev_t *d, const bm_i2c_txn_t *t);

/* Probe first..last on bus, setting bit a of bitmap (16 bytes, zeroed by the caller) per answer.
 * Returns an error (timeout) when the bus looks stuck and the sweep was aborted. */
bm_err_t bm_i2c_hal_scan(i2c_bus_t *bus, uint8_t first, uint8_t last, uint8_t bitmap[16]);

/* Background poller: d->driver->poll.sample(d, out) at hz into a ring of depth samples. A second call
 * on the same device updates the rate. NULL on failure. */
void *bm_i2c_hal_poll_start(i2c_dev_t *d, uint32_t hz, size_t depth);
/* notify(arg) after each stored sample, from the poller's task; must not block or touch PHP. NULL clears it.
 * On return no call of the previous notify is running or can start. */
void bm_i2c_hal_poll_on_sample(void *poller, void (*notify)(void *arg), void *arg);
bool bm_i2c_hal_poll_latest(void *poller, void *out);
size_t bm_i2c_hal_poll_drain(void *poller, void *out, size_t max_samples);
/* Pause (true) or resume (false) the poller: paused, it stores no samples; unread samples stay. Since i2c-hal 1.1. */
void bm_i2c_hal_poll_pause(void *poller, bool paused);

/* Interrupt-driven service (since i2c-hal 1.2), for a driver whose descriptor has irq.supported: the HAL
 * services the device's INT line and runs d->driver->irq.service(d) on the task that owns the I2C bus (the
 * executor), so the service may call the device's read and write functions. It must not touch PHP.
 *
 * bm_i2c_hal_irq_start: configures `pin` as an input with pull-up and an interrupt on the falling edge. Each edge
 * kicks the service from the interrupt handler; kicks that arrive before the service runs merge into one run, so
 * the service must read everything the chip has pending. The bm_err_t the service returns is for logging only; it
 * does not stop the service. Returns the handle, or NULL when it cannot.
 * A handle is for one device; ext-i2c starts a device once and keeps the handle. */
void *bm_i2c_hal_irq_start(i2c_dev_t *d, int pin);
/* One more service run after `ms` milliseconds, for a driver that must re-check without an edge (a chip that
 * holds its line low, or a gesture that finishes later). It replaces a delayed run that is still pending; it does
 * not touch the edge-driven runs. It may be called from inside the service. */
void bm_i2c_hal_irq_after(void *h, uint32_t ms);
/* Pause (true): new edges are dropped, not queued, and a delayed run (after) or an edge run that is pending but
 * not yet taken is dropped; a run already taken by the executor completes. Resume (false): only un-pauses; on a
 * handle that is not paused it is a no-op (a pending run or delayed run survives). A driver pauses and resumes
 * freely. */
void bm_i2c_hal_irq_pause(void *h, bool paused);
/* For ext-i2c only; a driver never calls it (it uses after / pause), and ext-i2c does not either: the service
 * lives until the process ends, so the device's handle is never left dangling.
 * Stop the service and release the pin. It waits out a service run that is in progress; once it returns,
 * irq.service is never called again for this handle, and the handle is invalid. Called from inside the service
 * itself, it runs inline (it does not wait for itself). */
void bm_i2c_hal_irq_stop(void *h);

/* Printable name of a bm_err_t. */
const char *bm_i2c_hal_err_name(bm_err_t err);
