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

/* Driver-facing SDK: all a device driver includes; nothing backend-specific (no ESP-IDF, no FreeRTOS). */
#pragma once

#include "php.h"
#include "bm_i2c.h"
#include "i2c_driver_desc.h"

/* The device entry behind $this (a Device or a driver instance); throws and returns NULL if unset. */
i2c_dev_t *i2c_device_this(zval *zthis);

/* Shared driver constructor: (Baremetal\I2c\Bus $bus, int $address = default, int $hz = default). */
void i2c_driver_ctor(INTERNAL_FUNCTION_PARAMETERS, const i2c_driver_desc_t *desc);
