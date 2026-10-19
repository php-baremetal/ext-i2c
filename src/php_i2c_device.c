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

/* Baremetal\I2c\Device: raw transport over one device (probe/read/write/readReg/writeReg) and polling. */
#include "php_i2c.h"
#include "bm_i2c_hal.h"
#include "event_bus_php.h"
#include "zend_exceptions.h"

#define POLL_DRAIN_CAP   256

zend_class_entry *i2c_device_ce;
static zend_object_handlers i2c_device_handlers;

static zend_object *i2c_device_create(zend_class_entry *ce)
{
    i2c_device_object *o = zend_object_alloc(sizeof(i2c_device_object), ce);
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &i2c_device_handlers;
    o->dev = NULL;
    return &o->std;
}

static void i2c_device_free(zend_object *obj)
{
    zend_object_std_dtor(obj);   /* the registry entry outlives the object */
}

void i2c_device_object_wrap(zval *out, i2c_dev_t *dev)
{
    object_init_ex(out, i2c_device_ce);
    i2c_device_object_from(Z_OBJ_P(out))->dev = dev;
}

i2c_dev_t *i2c_device_this(zval *zthis)
{
    i2c_device_object *o = i2c_device_object_from(Z_OBJ_P(zthis));
    if (!o->dev) {
        zend_throw_exception(zend_ce_exception, "I2c\\Device is not initialised", 0);
        return NULL;
    }
    return o->dev;
}
#define this_dev i2c_device_this

ZEND_BEGIN_ARG_INFO_EX(arginfo_device_ctor, 0, 0, 2)
    ZEND_ARG_OBJ_INFO(0, bus, Baremetal\\I2c\\Bus, 0)
    ZEND_ARG_TYPE_INFO(0, address, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hz, IS_LONG, 0, "100000")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_probe, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_read, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, length, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_write, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_readreg, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, reg, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, length, IS_LONG, 0, "1")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_writereg, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, reg, IS_LONG, 0)
    ZEND_ARG_TYPE_MASK(0, value, MAY_BE_STRING|MAY_BE_LONG, NULL)
ZEND_END_ARG_INFO()

/* Device is instantiated through Bus::device(); a direct `new` needs a bus and address. */
PHP_METHOD(I2cDevice, __construct)
{
    zval *zbus;
    zend_long addr, hz = I2C_DEFAULT_HZ;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_OBJECT_OF_CLASS(zbus, i2c_bus_ce)
        Z_PARAM_LONG(addr)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(hz)
    ZEND_PARSE_PARAMETERS_END();

    i2c_bus_object *bo = i2c_bus_object_from(Z_OBJ_P(zbus));
    if (!bo->bus) {
        zend_throw_exception(zend_ce_exception, "I2c\\Bus is not initialised", 0);
        RETURN_THROWS();
    }

    bm_err_t err = BM_OK;
    i2c_dev_t *dev = i2c_dev_get(bo->bus, (uint16_t) addr, (uint32_t) hz, NULL, &err);
    if (!dev && err == BM_ERR_ADDR_IN_USE) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "address 0x%02X on this bus is already used by another driver", (unsigned) addr);
        RETURN_THROWS();
    }
    if (!dev) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "cannot add I2C device 0x%02X: %s", (unsigned) addr, bm_i2c_hal_err_name(err));
        RETURN_THROWS();
    }
    i2c_device_object_from(Z_OBJ_P(ZEND_THIS))->dev = dev;
}

PHP_METHOD(I2cDevice, probe)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = this_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    RETURN_BOOL(i2c_dev_probe(d) == BM_OK);
}

PHP_METHOD(I2cDevice, read)
{
    zend_long len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(len)
    ZEND_PARSE_PARAMETERS_END();
    if (len <= 0) {
        zend_argument_value_error(1, "must be greater than 0");
        RETURN_THROWS();
    }
    i2c_dev_t *d = this_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }

    zend_string *buf = zend_string_alloc(len, 0);
    bm_err_t err = i2c_dev_read(d, (uint8_t *) ZSTR_VAL(buf), len);
    if (err != BM_OK) {
        zend_string_release(buf);
        zend_throw_exception_ex(zend_ce_exception, 0, "I2C read failed: %s", bm_i2c_hal_err_name(err));
        RETURN_THROWS();
    }
    ZSTR_VAL(buf)[len] = '\0';
    RETURN_STR(buf);
}

PHP_METHOD(I2cDevice, write)
{
    zend_string *data;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(data)
    ZEND_PARSE_PARAMETERS_END();
    i2c_dev_t *d = this_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }

    bm_err_t err = i2c_dev_write(d, (const uint8_t *) ZSTR_VAL(data), ZSTR_LEN(data));
    if (err != BM_OK) {
        zend_throw_exception_ex(zend_ce_exception, 0, "I2C write failed: %s", bm_i2c_hal_err_name(err));
        RETURN_THROWS();
    }
}

PHP_METHOD(I2cDevice, readReg)
{
    zend_long reg, len = 1;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(reg)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(len)
    ZEND_PARSE_PARAMETERS_END();
    if (len <= 0) {
        zend_argument_value_error(2, "must be greater than 0");
        RETURN_THROWS();
    }
    i2c_dev_t *d = this_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }

    zend_string *buf = zend_string_alloc(len, 0);
    bm_err_t err = i2c_dev_read_reg(d, (uint8_t) reg, (uint8_t *) ZSTR_VAL(buf), len);
    if (err != BM_OK) {
        zend_string_release(buf);
        zend_throw_exception_ex(zend_ce_exception, 0,
            "I2C readReg 0x%02X failed: %s", (unsigned) reg, bm_i2c_hal_err_name(err));
        RETURN_THROWS();
    }
    ZSTR_VAL(buf)[len] = '\0';
    RETURN_STR(buf);
}

PHP_METHOD(I2cDevice, writeReg)
{
    zend_long reg;
    zval *value;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(reg)
        Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();
    i2c_dev_t *d = this_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }

    bm_err_t err;
    if (Z_TYPE_P(value) == IS_STRING) {
        err = i2c_dev_write_reg(d, (uint8_t) reg, (const uint8_t *) Z_STRVAL_P(value), Z_STRLEN_P(value));
    } else {
        err = i2c_dev_write_reg1(d, (uint8_t) reg, (uint8_t) zval_get_long(value));
    }
    if (err != BM_OK) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "I2C writeReg 0x%02X failed: %s", (unsigned) reg, bm_i2c_hal_err_name(err));
        RETURN_THROWS();
    }
}

/* poll/sample/drain need a driver with a poll block. The HAL poller fills a ring at `hz`; sample() and
 * drain() read the ring without bus traffic. */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_poll, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, hz, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, depth, IS_LONG, 0, "64")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, event, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

/* Runs on the poller task after each sample: queue the device's event. */
static void i2c_emit_notify(void *arg)
{
    i2c_dev_t *d = arg;
    bm_events_emit(d->emit_tag, d->emit_obj);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_sample, 0, 0, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_device_drain, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static i2c_dev_t *this_poller_dev(zval *zthis)
{
    i2c_dev_t *d = i2c_device_this(zthis);
    if (!d) {
        return NULL;
    }
    if (!d->driver || !d->driver->poll.sample || !d->driver->poll.sample_size) {
        zend_throw_exception(zend_ce_exception, "this device has no poller (driver has no poll block)", 0);
        return NULL;
    }
    return d;
}

PHP_METHOD(I2cDevice, poll)
{
    zend_long hz, depth = 64;
    zend_string *event = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_LONG(hz)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(depth)
        Z_PARAM_STR_OR_NULL(event)
    ZEND_PARSE_PARAMETERS_END();
    if (hz <= 0) {
        zend_argument_value_error(1, "must be greater than 0");
        RETURN_THROWS();
    }
    i2c_dev_t *d = this_poller_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    d->poller = bm_i2c_hal_poll_start(d, (uint32_t) hz, (size_t) depth);
    if (!d->poller) {
        zend_throw_exception(zend_ce_exception, "cannot start poller (too many, or out of memory)", 0);
        RETURN_THROWS();
    }
    if (event) {
        /* Emit an event per sample with this device as $e->device; hold a ref so the object outlives
         * any PHP variable. */
        uint16_t tag;
        if (!bm_events_source_tag(event, &tag)) {
            zend_throw_exception_ex(zend_ce_exception, 0,
                "poll(): unknown event class '%s' or too many sources", ZSTR_VAL(event));
            RETURN_THROWS();
        }
        if (d->emit_obj != Z_OBJ_P(ZEND_THIS)) {
            /* Stop the notify first (afterwards the poller no longer reads emit_obj), then release the
             * old object only after the events already queued with it. */
            bm_i2c_hal_poll_on_sample(d->poller, NULL, NULL);
            zend_object *prev = d->emit_obj;
            d->emit_obj = Z_OBJ_P(ZEND_THIS);
            GC_ADDREF(Z_OBJ_P(ZEND_THIS));
            if (prev) {
                bm_events_release_later(prev);
            }
        }
        d->emit_tag = tag;
        bm_i2c_hal_poll_on_sample(d->poller, i2c_emit_notify, d);
    }
}

PHP_METHOD(I2cDevice, sample)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = this_poller_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    if (!d->poller) {
        zend_throw_exception(zend_ce_exception, "not polling; call poll() first", 0);
        RETURN_THROWS();
    }
    size_t sz = d->driver->poll.sample_size;
    zend_string *s = zend_string_alloc(sz, 0);
    if (bm_i2c_hal_poll_latest(d->poller, ZSTR_VAL(s))) {
        ZSTR_VAL(s)[sz] = '\0';
        RETURN_STR(s);
    }
    zend_string_release(s);
    RETURN_NULL();
}

PHP_METHOD(I2cDevice, drain)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = this_poller_dev(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    array_init(return_value);
    if (!d->poller) {
        return;
    }
    size_t sz = d->driver->poll.sample_size;
    uint8_t *buf = emalloc(POLL_DRAIN_CAP * sz);
    size_t n = bm_i2c_hal_poll_drain(d->poller, buf, POLL_DRAIN_CAP);
    for (size_t i = 0; i < n; i++) {
        add_next_index_stringl(return_value, (char *) (buf + i * sz), sz);
    }
    efree(buf);
}

static const zend_function_entry i2c_device_methods[] = {
    PHP_ME(I2cDevice, __construct, arginfo_device_ctor,     ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, probe,       arginfo_device_probe,    ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, read,        arginfo_device_read,     ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, write,       arginfo_device_write,    ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, readReg,     arginfo_device_readreg,  ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, writeReg,    arginfo_device_writereg, ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, poll,        arginfo_device_poll,     ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, sample,      arginfo_device_sample,   ZEND_ACC_PUBLIC)
    PHP_ME(I2cDevice, drain,       arginfo_device_drain,    ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void i2c_device_class_register(void)
{
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\I2c", "Device", i2c_device_methods);
    i2c_device_ce = zend_register_internal_class(&ce);
    i2c_device_ce->create_object = i2c_device_create;

    memcpy(&i2c_device_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    i2c_device_handlers.offset = XtOffsetOf(i2c_device_object, std);
    i2c_device_handlers.free_obj = i2c_device_free;
}

void i2c_capabilities_register(void)
{
    /* Marker interfaces registered up front so a type hint like `Baremetal\Sensor\Imu` resolves even when
     * no driver providing it is built. Other capability FQNs are created on demand. */
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Sensor", "Imu", NULL);
    zend_register_internal_interface(&ce);
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Input", "Touch", NULL);
    zend_register_internal_interface(&ce);
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Sensor", "Magnetometer", NULL);
    zend_register_internal_interface(&ce);
}

/* The interface for a capability FQN: the registered one, else a new marker interface. NULL only if
 * the name is taken by a non-interface. */
static zend_class_entry *i2c_capability_iface(const char *fqn)
{
    size_t len = strlen(fqn);
    zend_string *lc = zend_string_alloc(len, 0);
    zend_str_tolower_copy(ZSTR_VAL(lc), fqn, len);
    zend_class_entry *ce = zend_hash_find_ptr(CG(class_table), lc);
    zend_string_release(lc);
    if (ce) {
        return (ce->ce_flags & ZEND_ACC_INTERFACE) ? ce : NULL;
    }
    zend_class_entry nce;
    INIT_CLASS_ENTRY_EX(nce, fqn, len, NULL);
    return zend_register_internal_interface(&nce);
}

void i2c_events_register(void)
{
    /* SamplesReady: the event a polled sensor emits; $e->device is the emitting sensor. */
    zend_class_entry *base = bm_events_base_ce();
    if (!base) {
        return;
    }
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "Baremetal\\Sensor\\Imu", "SamplesReady", NULL);
    zend_register_internal_class_ex(&ce, base);
}

void i2c_driver_classes_register(void)
{
    for (size_t i = 0; i < i2c_drivers_count; i++) {
        const i2c_driver_desc_t *desc = i2c_drivers[i];
        if (!desc || !desc->php_class) {
            continue;
        }
        char fqn[96];
        int n = snprintf(fqn, sizeof(fqn), "Baremetal\\I2c\\Driver\\%s", desc->php_class);
        zend_class_entry ce;
        INIT_CLASS_ENTRY_EX(ce, fqn, n, desc->methods);
        zend_class_entry *dce = zend_register_internal_class_ex(&ce, i2c_device_ce);
        dce->create_object = i2c_device_create;

        if (desc->capability) {
            zend_class_entry *iface = i2c_capability_iface(desc->capability);
            if (iface) {
                zend_class_implements(dce, 1, iface);
            }
        }
    }
}

void i2c_driver_ctor(INTERNAL_FUNCTION_PARAMETERS, const i2c_driver_desc_t *desc)
{
    zval *zbus;
    zend_long addr = -1, hz = 0;
    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_OBJECT_OF_CLASS(zbus, i2c_bus_ce)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(addr)
        Z_PARAM_LONG(hz)
    ZEND_PARSE_PARAMETERS_END();

    i2c_bus_object *bo = i2c_bus_object_from(Z_OBJ_P(zbus));
    if (!bo->bus) {
        zend_throw_exception(zend_ce_exception, "I2c\\Bus is not initialised", 0);
        return;
    }
    if (addr < 0) {
        addr = desc->n_default_addrs ? desc->default_addrs[0] : 0;
    }
    if (hz <= 0) {
        hz = desc->default_hz ? desc->default_hz : I2C_DEFAULT_HZ;
    }

    bm_err_t err = BM_OK;
    i2c_dev_t *dev = i2c_dev_get(bo->bus, (uint16_t) addr, (uint32_t) hz, desc, &err);
    if (!dev && err == BM_ERR_ADDR_IN_USE) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "address 0x%02X on this bus is already used by another driver", (unsigned) addr);
        return;
    }
    if (!dev) {
        zend_throw_exception_ex(zend_ce_exception, 0,
            "cannot init %s at 0x%02X: %s", desc->name, (unsigned) addr, bm_i2c_hal_err_name(err));
        return;
    }
    i2c_device_object_from(Z_OBJ_P(ZEND_THIS))->dev = dev;
}
