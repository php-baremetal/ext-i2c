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
 * Internal glue shared by the i2c extension's C files: the class entries, the object wrappers (each a
 * handle into a registry entry) and the registration entry points called from MINIT.
 */
#pragma once

#include "php.h"
#include "i2c_registry.h"
#include "bm_i2c_driver.h"   /* i2c_device_this + i2c_driver_ctor (the driver-facing glue) */

extern zend_module_entry i2c_module_entry;

extern zend_class_entry *i2c_bus_ce;
extern zend_class_entry *i2c_device_ce;

typedef struct {
    i2c_bus_t   *bus;
    zend_object  std;
} i2c_bus_object;

typedef struct {
    i2c_dev_t   *dev;
    zend_object  std;
} i2c_device_object;

/* Argument checks shared by Bus::device(), new Device() and the driver constructors: a 7-bit address
 * outside the reserved ranges, and a clock the controllers support. false (with a ValueError) otherwise. */
static inline bool i2c_check_addr_hz(uint32_t addr_arg, zend_long addr, uint32_t hz_arg, zend_long hz)
{
    if (addr < I2C_SCAN_FIRST || addr > I2C_SCAN_LAST) {
        zend_argument_value_error(addr_arg, "must be a 7-bit address between 0x08 and 0x77");
        return false;
    }
    if (hz < 1 || hz > (zend_long) I2C_MAX_HZ) {
        zend_argument_value_error(hz_arg, "must be between 1 and 1000000 (Hz)");
        return false;
    }
    return true;
}

static inline i2c_bus_object *i2c_bus_object_from(zend_object *o)
{
    return (i2c_bus_object *)((char *)o - XtOffsetOf(i2c_bus_object, std));
}

static inline i2c_device_object *i2c_device_object_from(zend_object *o)
{
    return (i2c_device_object *)((char *)o - XtOffsetOf(i2c_device_object, std));
}

void i2c_bus_class_register(void);
void i2c_device_class_register(void);

/* Register the capability interfaces (Sensor\Imu, Input\Touch, Sensor\Magnetometer). */
void i2c_capabilities_register(void);

/* Register the driver-emitted event classes (Baremetal\Sensor\Imu\SamplesReady, extends Event). */
void i2c_events_register(void);

/* Register the selected driver classes (Baremetal\I2c\Driver\*), each extending Device. */
void i2c_driver_classes_register(void);

/* Wrap a registered device entry in a Baremetal\I2c\Device object, into out. */
void i2c_device_object_wrap(zval *out, i2c_dev_t *dev);

