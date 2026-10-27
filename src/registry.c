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

/* The bus/device tables and the driver-facing transport, over the neutral i2c HAL. */
#include "bm_i2c_hal.h"

#include <string.h>
#include <stdlib.h>

#define MAX_BUSES    4
#define MAX_DEVICES  32

static i2c_bus_t s_buses[MAX_BUSES];
static i2c_dev_t s_devs[MAX_DEVICES];

void i2c_registry_init(void)
{
    memset(s_buses, 0, sizeof(s_buses));
    memset(s_devs, 0, sizeof(s_devs));
}

static void addr_mark(i2c_bus_t *bus, uint16_t addr)
{
    if (addr < 128) {
        bus->addr_used[addr >> 5] |= (1u << (addr & 31));
    }
}

static void set_err(bm_err_t *err, bm_err_t e)
{
    if (err) {
        *err = e;
    }
}

i2c_bus_t *i2c_bus_get(int port, int sda, int scl, i2c_owner_t owner, bm_err_t *err)
{
    set_err(err, BM_OK);

    for (int i = 0; i < MAX_BUSES; i++) {
        i2c_bus_t *b = &s_buses[i];
        if (!b->in_use) {
            continue;
        }
        bool same_pins = b->sda == sda && b->scl == scl;
        /* b->port is the resolved port when the HAL could tell; AUTO on either side matches any port. */
        bool same_port = port == I2C_PORT_AUTO || b->port == I2C_PORT_AUTO || b->port == port;
        if (same_pins && same_port) {
            return b;
        }
        /* A pin of an open bus, or the same port with other pins: never hand back a bus wired differently. */
        bool shares_pin = b->sda == sda || b->sda == scl || b->scl == sda || b->scl == scl;
        if (shares_pin || (port != I2C_PORT_AUTO && b->port == port)) {
            set_err(err, BM_ERR_BUS_PINS);
            return NULL;
        }
    }

    i2c_bus_t *slot = NULL;
    for (int i = 0; i < MAX_BUSES; i++) {
        if (!s_buses[i].in_use) {
            slot = &s_buses[i];
            break;
        }
    }
    if (!slot) {
        set_err(err, BM_ERR_NO_MEM);
        return NULL;
    }

    memset(slot, 0, sizeof(*slot));
    slot->port = port;
    slot->sda = sda;
    slot->scl = scl;
    slot->owner = owner;
    bm_err_t e = bm_i2c_hal_bus_open(slot);
    if (e != BM_OK) {
        memset(slot, 0, sizeof(*slot));
        set_err(err, e);
        return NULL;
    }
    slot->in_use = true;
    return slot;
}

i2c_dev_t *i2c_dev_find(i2c_bus_t *bus, uint16_t addr)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        i2c_dev_t *d = &s_devs[i];
        if (d->in_use && d->bus == bus && d->addr == addr) {
            return d;
        }
    }
    return NULL;
}

i2c_dev_t *i2c_dev_get(i2c_bus_t *bus, uint16_t addr, uint32_t hz,
                       const i2c_driver_desc_t *driver, bm_err_t *err)
{
    set_err(err, BM_OK);

    i2c_dev_t *found = i2c_dev_find(bus, addr);
    if (found) {
        if (found->driver != driver) {
            set_err(err, BM_ERR_ADDR_IN_USE);
            return NULL;
        }
        return found;
    }

    i2c_dev_t *slot = NULL;
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!s_devs[i].in_use) {
            slot = &s_devs[i];
            break;
        }
    }
    if (!slot) {
        set_err(err, BM_ERR_NO_MEM);
        return NULL;
    }

    memset(slot, 0, sizeof(*slot));
    slot->bus = bus;
    slot->addr = addr;
    slot->hz = hz ? hz : I2C_DEFAULT_HZ;
    slot->driver = driver;
    bm_err_t e = bm_i2c_hal_dev_open(slot);
    if (e != BM_OK) {
        memset(slot, 0, sizeof(*slot));
        set_err(err, e);
        return NULL;
    }

    if (driver && driver->state_size) {
        slot->state = calloc(1, driver->state_size);
        if (!slot->state) {
            bm_i2c_hal_dev_close(slot);
            memset(slot, 0, sizeof(*slot));
            set_err(err, BM_ERR_NO_MEM);
            return NULL;
        }
    }

    slot->in_use = true;
    if (driver && driver->init) {
        e = driver->init(slot);
        if (e != BM_OK) {
            free(slot->state);
            bm_i2c_hal_dev_close(slot);
            memset(slot, 0, sizeof(*slot));
            set_err(err, e);
            return NULL;
        }
    }

    addr_mark(bus, addr);
    return slot;
}

bm_err_t i2c_dev_read(i2c_dev_t *d, uint8_t *buf, size_t n)
{
    bm_i2c_txn_t t = { .op = BM_I2C_READ, .rbuf = buf, .n = n };
    return bm_i2c_hal_transfer(d, &t);
}

bm_err_t i2c_dev_write(i2c_dev_t *d, const uint8_t *buf, size_t n)
{
    bm_i2c_txn_t t = { .op = BM_I2C_WRITE, .wbuf = buf, .n = n };
    return bm_i2c_hal_transfer(d, &t);
}

bm_err_t i2c_dev_read_reg(i2c_dev_t *d, uint8_t reg, uint8_t *buf, size_t n)
{
    bm_i2c_txn_t t = { .op = BM_I2C_READ_REG, .rbuf = buf, .n = n, .reg = reg };
    return bm_i2c_hal_transfer(d, &t);
}

bm_err_t i2c_dev_write_reg(i2c_dev_t *d, uint8_t reg, const uint8_t *buf, size_t n)
{
    bm_i2c_txn_t t = { .op = BM_I2C_WRITE_REG, .wbuf = buf, .n = n, .reg = reg };
    return bm_i2c_hal_transfer(d, &t);
}

bm_err_t i2c_dev_write_reg1(i2c_dev_t *d, uint8_t reg, uint8_t val)
{
    bm_i2c_txn_t t = { .op = BM_I2C_WRITE_REG1, .reg = reg, .val = val };
    return bm_i2c_hal_transfer(d, &t);
}

bm_err_t i2c_dev_probe(i2c_dev_t *d)
{
    bm_i2c_txn_t t = { .op = BM_I2C_PROBE };
    return bm_i2c_hal_transfer(d, &t);
}

bm_err_t i2c_bus_scan_sweep(i2c_bus_t *bus, uint8_t bitmap[16])
{
    memset(bitmap, 0, 16);
    return bm_i2c_hal_scan(bus, I2C_SCAN_FIRST, I2C_SCAN_LAST, bitmap);
}
