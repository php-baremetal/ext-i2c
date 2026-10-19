---
eyebrow: 'ext-i2c · Drivers'
lede:    'ext-i2c owns the i2c_drivers registry. Driver packages register a descriptor into it, and each descriptor becomes a PHP class under Baremetal\I2c\Driver.'
see_also:
  - { href: './writing-a-driver.md', meta: 'Drivers' }
  - { href: 'package:php-baremetal/imu-qmi8658#docs/overview.md', meta: 'Driver', label: 'imu-qmi8658' }
  - { href: 'package:php-baremetal/mag-mmc5603', meta: 'Driver', label: 'mag-mmc5603' }
  - { href: 'package:php-baremetal/rtc-pcf85063', meta: 'Driver', label: 'rtc-pcf85063' }
prev: { label: 'Errors and limits', href: '../reference/errors-and-limits.md' }
next: { label: 'Writing a driver', href: './writing-a-driver.md' }
---

# The i2c_drivers registry

ext-i2c declares one registry in its `composer.json`:

<!-- @code-block language="json" label="ext-i2c composer.json, extra.baremetal" -->
```json
{
  "module": "i2c",
  "priority": 100,
  "registries": {
    "i2c_drivers": { "type": "i2c_driver_desc_t", "header": "i2c_driver_desc.h" }
  }
}
```
<!-- @endcode-block -->

A driver package registers into it by naming its descriptor symbol:

<!-- @code-block language="json" label="imu-qmi8658 composer.json, extra.baremetal" -->
```json
{ "register": { "i2c_drivers": ["qmi8658_driver_desc"] } }
```
<!-- @endcode-block -->

## What happens at build and startup

<!-- @steps -->
- **The build collects the entries**

  The SDK driver generates a table from every package in the project that registers into `i2c_drivers`,
  ordered by package and then symbol, NULL-terminated, with a count. With no driver installed the table is
  empty and ext-i2c still builds.

- **ext-i2c registers the classes**

  At module startup, for each descriptor ext-i2c registers `Baremetal\I2c\Driver\<php_class>` as a subclass of
  `Baremetal\I2c\Device`, with the descriptor's method table. If the descriptor names a `capability`, the class
  implements that interface.

- **PHP uses the class**

  `new Qmi8658($bus)` attaches the chip at the descriptor's default address and clock, allocates the driver
  state and runs the driver's `init`. `Bus::scan()` reports the descriptor's `name` for that address.
<!-- @endsteps -->

So a project gets a driver class just by requiring the driver package: nothing to configure, and no class to
enable at runtime. Classes of drivers that are not installed do not exist.

## Drivers available today

| Package | Class | Default address, clock | Capability | Sampler |
| --- | --- | --- | --- | --- |
| [imu-qmi8658](package:php-baremetal/imu-qmi8658) | `Qmi8658` | `0x6B`, 400 kHz | `Baremetal\Sensor\Imu` | 12-byte samples |
| [mag-mmc5603](package:php-baremetal/mag-mmc5603) | `Mmc5603` | `0x30`, 400 kHz | `Baremetal\Sensor\Magnetometer` | 9-byte samples |
| [rtc-pcf85063](package:php-baremetal/rtc-pcf85063) | `Pcf85063` | `0x51`, 400 kHz | none | none |

Their methods:

| Class | Methods |
| --- | --- |
| `Qmi8658` | `accel(): array` (g), `gyro(): array` (dps), `temp(): float` (°C), `decode(string $raw): array` (`accel`, `gyro`) |
| `Mmc5603` | `mag(): array` (µT), `heading(): float` (degrees), `temp(): float` (°C), `decode(string $raw): array` (`mag`, `heading`) |
| `Pcf85063` | `now(): array` (`year`, `month`, `day`, `hour`, `minute`, `second`, `valid`), `set(int $year, int $month, int $day, int $hour, int $minute, int $second): void` |

Every constructor is `(Bus $bus, int $address = <default>, int $hz = <default>)`, and every class also has the
`Device` methods.

## Rules the build enforces

- Only ext-i2c may declare `i2c_drivers`; a second package declaring it is a build error.
- A symbol registered twice, an invalid C name, or a registry that no installed package declares is a build
  error that names the package.
- The header `i2c_driver_desc.h` comes from ext-i2c's `include/`, so a driver needs no include path of its own.
