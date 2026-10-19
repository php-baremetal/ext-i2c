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
 * Neutral I2C transport: what a device driver uses to reach its chip, free of any HAL. The error type,
 * the register-level transport and the delay live here; the platform's i2c-hal package implements them.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "bm_err.h"   /* bm_err_t and the BM_ERR_* codes (php-baremetal/bm-common) */

/* The per-device registry entry; opaque to drivers (its layout is the backend's concern). */
typedef struct i2c_dev i2c_dev_t;

/* Register-level transport; the HAL routes each call (under the bus lock, or to the bus owner task). */
bm_err_t i2c_dev_read(i2c_dev_t *d, uint8_t *buf, size_t n);
bm_err_t i2c_dev_write(i2c_dev_t *d, const uint8_t *buf, size_t n);
bm_err_t i2c_dev_read_reg(i2c_dev_t *d, uint8_t reg, uint8_t *buf, size_t n);
bm_err_t i2c_dev_write_reg(i2c_dev_t *d, uint8_t reg, const uint8_t *buf, size_t n);
bm_err_t i2c_dev_write_reg1(i2c_dev_t *d, uint8_t reg, uint8_t val);

/* ACK test of the device's address; BM_OK if it answers. */
bm_err_t i2c_dev_probe(i2c_dev_t *d);

/* Sleep the current context for `ms` milliseconds (yields on an RTOS backend). */
void bm_delay_ms(uint32_t ms);
