---
eyebrow: 'ext-i2c · Drivers'
lede:    'A driver is a bm-lib package with one C file: a descriptor of type i2c_driver_desc_t, registered into i2c_drivers. ext-i2c turns it into a PHP class.'
see_also:
  - { href: './registry.md', meta: 'Drivers' }
  - { href: '../reference/ownership-and-power.md', meta: 'Reference' }
  - { href: 'package:php-baremetal/imu-qmi8658', meta: 'Driver', label: 'imu-qmi8658 (a complete driver)' }
  - { href: 'package:php-baremetal/bm-common', meta: 'Library', label: 'bm-common (bm_err.h)' }
prev: { label: 'The i2c_drivers registry', href: './registry.md' }
next: { label: 'Scan the bus', href: '../recipes/scan-the-bus.md' }
---

# Writing a driver

A driver gives one chip a PHP class. It is a `bm-lib` package that requires ext-i2c and registers a descriptor
symbol into the `i2c_drivers` registry. The C code includes only `bm_i2c_driver.h` (plus Zend headers for its
methods): no ESP-IDF, no FreeRTOS, so the same source builds on any platform that has an i2c HAL.

## The package

<!-- @code-block language="json" label="composer.json" -->
```json
{
  "name": "acme/xyz-driver",
  "description": "XYZ sensor driver for ext-i2c (Baremetal\\I2c\\Driver\\Xyz).",
  "version": "1.0.0",
  "type": "bm-lib",
  "license": "MIT",
  "require": {
    "php": ">=8.3",
    "php-baremetal/ext-i2c": "^1.0"
  },
  "extra": {
    "baremetal": {
      "register": { "i2c_drivers": ["xyz_driver_desc"] }
    }
  }
}
```
<!-- @endcode-block -->

Sources default to `src/*.c`; no `CMakeLists.txt`. The symbol in `register` must be a non-static `const
i2c_driver_desc_t` defined in those sources.

## The C file

A complete driver with a constructor, one method and a sampler. The register addresses are placeholders for
your chip's.

<!-- @code-block language="c" label="src/xyz.c" -->
```c
#include "bm_i2c_driver.h"
#include "zend_exceptions.h"

#define XYZ_ID_REG    0x0F   /* identity register of the chip */
#define XYZ_ID_VALUE  0x33   /* value it must return */
#define XYZ_DATA_REG  0x28   /* 2 bytes, little-endian */

extern const i2c_driver_desc_t xyz_driver_desc;

static bm_err_t xyz_init(i2c_dev_t *d)
{
    uint8_t id = 0;
    bm_err_t e = i2c_dev_read_reg(d, XYZ_ID_REG, &id, 1);
    if (e != BM_OK) {
        return e;
    }
    return id == XYZ_ID_VALUE ? BM_OK : BM_ERR_NOT_FOUND;
}

/* Runs on the executor task: transport calls only, no PHP. */
static bm_err_t xyz_sample(i2c_dev_t *d, void *out)
{
    return i2c_dev_read_reg(d, XYZ_DATA_REG, out, 2);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_xyz_ctor, 0, 0, 1)
    ZEND_ARG_OBJ_INFO(0, bus, Baremetal\\I2c\\Bus, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address, IS_LONG, 0, "0x18")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, hz, IS_LONG, 0, "400000")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_xyz_value, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Xyz, __construct)
{
    i2c_driver_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, &xyz_driver_desc);
}

PHP_METHOD(Xyz, value)
{
    ZEND_PARSE_PARAMETERS_NONE();
    i2c_dev_t *d = i2c_device_this(ZEND_THIS);
    if (!d) {
        RETURN_THROWS();
    }
    uint8_t b[2];
    if (i2c_dev_read_reg(d, XYZ_DATA_REG, b, 2) != BM_OK) {
        zend_throw_exception(zend_ce_exception, "XYZ read failed", 0);
        RETURN_THROWS();
    }
    RETURN_LONG((int16_t) (b[0] | (b[1] << 8)));
}

static const zend_function_entry xyz_methods[] = {
    PHP_ME(Xyz, __construct, arginfo_xyz_ctor,  ZEND_ACC_PUBLIC)
    PHP_ME(Xyz, value,       arginfo_xyz_value, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

const i2c_driver_desc_t xyz_driver_desc = {
    .name = "xyz",
    .php_class = "Xyz",
    .default_addrs = { 0x18 },
    .n_default_addrs = 1,
    .default_hz = 400000,
    .init = xyz_init,
    .methods = xyz_methods,
    .poll = { .sample = xyz_sample, .sample_size = 2 },
};
```
<!-- @endcode-block -->

PHP then has `Baremetal\I2c\Driver\Xyz`, with `value()` plus every `Device` method, and `poll()` works because
the descriptor has a sampler.

<!-- @callout variant="warning" title="Always define __construct" -->
The method table must contain a `__construct` that calls `i2c_driver_ctor()` with the descriptor. Without it the
class inherits `Device::__construct`, which attaches a raw device: `init` never runs and the sampler is missing.
<!-- @endcallout -->

## Descriptor fields

`i2c_driver_desc_t` is declared in `i2c_driver_desc.h`. Leave unused fields zeroed (designated initialisers do
that). ext-i2c 1.0 reads these:

| Field | Meaning |
| --- | --- |
| `name` | Registry name, reported by `Bus::scan()` and in constructor errors, e.g. `"qmi8658"`. |
| `php_class` | Short class name; the class is `Baremetal\I2c\Driver\<php_class>`. Must be unique among installed drivers; the full name must fit in 95 characters. |
| `capability` | Interface FQN the class implements, e.g. `"Baremetal\\Sensor\\Imu"`, or `NULL`. A name that does not exist yet is registered as a new marker interface. |
| `default_addrs`, `n_default_addrs` | Default address when the constructor gets none: `default_addrs[0]` when `n_default_addrs` is not 0, else 0. The other entries are not tried. |
| `default_hz` | Default clock when the constructor gets none; 0 means 100 kHz. |
| `state_size` | Bytes allocated (zeroed) per device before `init`. The driver-facing header has no accessor for this block, so drivers in this release keep it 0. |
| `init` | Called once when the device is first created, on the PHP task. Return `BM_OK`, or an error code that the constructor reports (`BM_ERR_NOT_FOUND` for a wrong chip identity). |
| `methods` | The class's method table (`zend_function_entry[]`). |
| `poll.sample` | Reads one sample into `out`. Runs on the executor task: transport functions only, no PHP, no Zend allocator. |
| `poll.sample_size` | Bytes per sample, 1 to 64. Both `sample` and `sample_size` must be set for `poll()` to work. |

Declared but not read by ext-i2c 1.0, so they have no effect yet: `deinit` (devices are never torn down),
`poll.max_hz` (the rate is not capped), and the `fifo`, `block`, `irq`, `power` and `events` / `n_events` blocks.
Filling them is harmless; do not rely on them.

## Transport functions

From `bm_i2c.h`, included by `bm_i2c_driver.h`. Each call is routed by the bus's owner (under the bus lock for
`Bus::SYNC`, on the executor for `Bus::CORE1`), so a driver never locks anything itself. All return `bm_err_t`
(`BM_OK` is 0; codes in `bm_err.h` from bm-common).

| Function | Does |
| --- | --- |
| `i2c_dev_read(d, buf, n)` | Read `n` bytes. |
| `i2c_dev_write(d, buf, n)` | Write `n` bytes. |
| `i2c_dev_read_reg(d, reg, buf, n)` | Write the register address, then read `n` bytes. |
| `i2c_dev_write_reg(d, reg, buf, n)` | Write the register address followed by `n` bytes. |
| `i2c_dev_write_reg1(d, reg, val)` | Write one byte to a register. |
| `i2c_dev_probe(d)` | `BM_OK` if the chip acknowledges its address. |
| `bm_delay_ms(ms)` | Sleep the current task (returns `void`). |

And the PHP glue from `bm_i2c_driver.h`:

| Function | Does |
| --- | --- |
| `i2c_driver_ctor(INTERNAL_FUNCTION_PARAM_PASSTHRU, &desc)` | The shared constructor: `(Bus $bus, int $address = default, int $hz = default)`, attaches the device, runs `init`, throws on failure. |
| `i2c_device_this(ZEND_THIS)` | The `i2c_dev_t *` behind the object; throws and returns `NULL` if the object is not initialised. |

`i2c_dev_t` is opaque: use only these functions on it.

## Try it in a project

Keep the driver next to the project as a path repository; conductor links it into `.phpbm/packages/`.

<!-- @code-block language="json" label="project composer.json" -->
```json
{
  "name": "acme/xyz-demo",
  "type": "project",
  "repositories": [
    { "type": "composer", "url": "https://repository.php-baremetal.com", "canonical": true },
    { "type": "path", "url": "extensions/xyz-driver" }
  ],
  "require": {
    "php-baremetal/conductor": "^1.0",
    "acme/xyz-driver": "^1.0"
  },
  "config": {
    "allow-plugins": { "php-baremetal/conductor": true },
    "platform": { "php": "8.5.11" }
  },
  "extra": {
    "baremetal": {
      "sdk": "esp32@^2.0",
      "board": "espressif-esp32-s3-devkitc-1-n16r8",
      "runtime": "init-loop",
      "storage": "embedded"
    }
  }
}
```
<!-- @endcode-block -->

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Xyz;

$xyz = new Xyz(new Bus(sda: 8, scl: 9));   // the pins your chip is wired to
echo $xyz->value(), "\n";
```
<!-- @endcode-block -->

Then `phpbm flash` and `phpbm monitor`. A wrong chip identity shows up as
`cannot init xyz at 0x18: ESP_ERR_NOT_FOUND`.
