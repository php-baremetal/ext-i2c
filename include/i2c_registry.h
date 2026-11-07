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
 * The two process-lifetime tables (buses, devices), never torn down: a PHP object is a handle into them
 * and its destructor never touches the hardware, so `new Bus(...)` / `new Device(...)` on the same key
 * return the same entry, also across web-server per-request teardown. The `hal` handles belong to the
 * i2c HAL (bm_i2c_hal.h).
 */
#pragma once

#include "bm_i2c.h"            /* bm_err_t + the driver-facing transport decls */
#include "i2c_driver_desc.h"
#include <stdint.h>
#include <stdatomic.h>

#include <stdbool.h>

/* Ring depth the executor gives poll(): 0 means 64, above 256 or negative means 256 (size_t wraps) (esp32-executor, executor_poll). */
#define I2C_POLL_DEFAULT_DEPTH 64u
#define I2C_POLL_MAX_DEPTH     256u

#define I2C_PORT_AUTO      (-1)
#define I2C_DEFAULT_HZ     (100000u)
#define I2C_SCAN_FIRST     (0x08)
#define I2C_SCAN_LAST      (0x77)
#define I2C_MAX_HZ         (1000000u)

typedef enum {
    I2C_OWNER_SYNC  = 0,   /* the caller owns the wire; a per-bus lock guards each transaction */
    I2C_OWNER_CORE1 = 1,   /* the executor task owns the wire; callers hand transactions to it */
} i2c_owner_t;

typedef struct i2c_bus {
    bool         in_use;
    int          port;
    int          sda;
    int          scl;
    i2c_owner_t  owner;
    void        *hal;            /* HAL bus state (handle + lock) */
    uint32_t     addr_used[4];   /* bitmap of the 128 7-bit addresses */
} i2c_bus_t;

struct i2c_dev {
    bool                     in_use;
    i2c_bus_t               *bus;
    uint16_t                 addr;
    uint32_t                 hz;
    void                    *hal;      /* HAL device handle */
    const i2c_driver_desc_t *driver;   /* NULL for a raw device */
    void                    *state;    /* driver per-instance state (state_size), or NULL */
    void                    *poller;   /* HAL poller once poll() is called, or NULL */
    void                    *irq;      /* HAL interrupt service once bm_i2c_device_irq() started it, or NULL; PHP task */
    void                    *emit_obj; /* the PHP device object (zend_object*) $e->device when emitting */
    uint16_t                 emit_tag; /* event source tag emitted on each new sample */
    uint32_t                 depth;    /* ring depth of the running poller (PHP task) */
    atomic_uint              stored;   /* samples stored since poll(); incremented on the poller task */
    atomic_uint              read;     /* samples taken by drain(); PHP task */
    atomic_uint              every;    /* one event every `every` stored samples */
    atomic_bool              emitting; /* an event class is set: the notify raises events */
    atomic_uint              since;    /* samples since the last event; poller task (poll() zeroes it) */
    struct _zend_object     *poller_obj;   /* the live Baremetal\I2c\Poller handle, or NULL; weak (no reference held) */
    uint32_t                 poll_hz;  /* sampling rate asked for (poll(), setHz()); PHP task */
    bool                     paused;   /* the poller is stopped by Poller::stop(); PHP task */
};

/* Reset the tables. Runs once at MINIT; creates nothing on the wire. */
void i2c_registry_init(void);

/* Idempotent lookup-or-create for the bus keyed by (port, sda, scl). Returns the entry, or NULL with
 * *err set. owner is honoured only on first creation. */
i2c_bus_t *i2c_bus_get(int port, int sda, int scl, i2c_owner_t owner, bm_err_t *err);

/* Idempotent lookup-or-create for the device keyed by (bus, addr). On first creation it opens the HAL
 * device, allocates driver->state_size and runs driver->init. Returns the entry, or NULL with *err. */
i2c_dev_t *i2c_dev_get(i2c_bus_t *bus, uint16_t addr, uint32_t hz,
                       const i2c_driver_desc_t *driver, bm_err_t *err);

/* The device mounted at addr on bus, or NULL. Used by scan() to annotate answered addresses. */
i2c_dev_t *i2c_dev_find(i2c_bus_t *bus, uint16_t addr);

/* Probe I2C_SCAN_FIRST..LAST, setting bit a of bitmap (16 bytes) per answering address. */
bm_err_t i2c_bus_scan_sweep(i2c_bus_t *bus, uint8_t bitmap[16]);
