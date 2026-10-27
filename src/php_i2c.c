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
 * i2c extension module: at MINIT it resets the registries and registers Baremetal\I2c\Bus,
 * Baremetal\I2c\Device and the driver classes of the i2c_drivers registry.
 */
#include "php_i2c.h"

PHP_MINIT_FUNCTION(i2c)
{
    i2c_registry_init();
    i2c_capabilities_register();
    i2c_events_register();
    i2c_bus_class_register();
    i2c_device_class_register();
    i2c_driver_classes_register();
    return SUCCESS;
}

static const zend_function_entry i2c_functions[] = {
    PHP_FE_END
};

zend_module_entry i2c_module_entry = {
    STANDARD_MODULE_HEADER,
    "i2c",
    i2c_functions,
    PHP_MINIT(i2c),
    NULL,   /* MSHUTDOWN */
    NULL,   /* RINIT */
    NULL,   /* RSHUTDOWN */
    NULL,   /* MINFO */
    "1.0",
    STANDARD_MODULE_PROPERTIES
};
