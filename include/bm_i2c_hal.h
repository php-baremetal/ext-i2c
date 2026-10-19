/*
   +----------------------------------------------------------------------+
   | php-baremetal: ext-i2c                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2026 Gianfrancesco Aurecchia                           |
   +----------------------------------------------------------------------+
   | This source file is subject to the MIT license, that is bundled with |
   | this package in the file LICENSE.                                    |
   +----------------------------------------------------------------------+
   | Authors: Gianfrancesco Aurecchia <gianfri@aurecchia.com>             |
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

/* Printable name of a bm_err_t. */
const char *bm_i2c_hal_err_name(bm_err_t err);
