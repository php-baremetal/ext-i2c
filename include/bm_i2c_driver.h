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

/* Driver-facing SDK: all a device driver includes; nothing backend-specific (no ESP-IDF, no FreeRTOS). */
#pragma once

#include "php.h"
#include "bm_i2c.h"
#include "i2c_driver_desc.h"

/* The device entry behind $this (a Device or a driver instance); throws and returns NULL if unset. */
i2c_dev_t *i2c_device_this(zval *zthis);

/* Shared driver constructor: (Baremetal\I2c\Bus $bus, int $address = default, int $hz = default). */
void i2c_driver_ctor(INTERNAL_FUNCTION_PARAMETERS, const i2c_driver_desc_t *desc);

/* The registry device of a Device object (a driver class instance), or NULL when it is not initialised. Does not
 * throw. */
i2c_dev_t *bm_i2c_device_dev(struct _zend_object *dev);

/* The driver's per-instance state: state_size bytes (zeroed, allocated with the device before init), or NULL
 * when state_size is 0. It is how irq.service(d) and poll.sample(d) reach their context. */
void *bm_i2c_dev_state(i2c_dev_t *d);

/* For a driver with irq.supported: starts its interrupt service on `pin` (an input with pull-up, falling edge;
 * at each edge the executor runs irq.service). Once per device: a second call returns the same handle. Returns
 * the HAL handle (see bm_i2c_hal_irq_* in bm_i2c_hal.h), or NULL after throwing \Error when the driver has no
 * interrupt service or the HAL cannot start it. Call it on the PHP task only. The service lives until shutdown. The pin of the first call is the one
 * in use. */
void *bm_i2c_device_irq(struct _zend_object *dev, int pin);
