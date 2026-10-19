---
eyebrow: 'Package · Extension'
lede:    'An I2C master for PHP on the board: open a bus, scan it, read and write any chip, or use a driver class that samples in the background. Neutral extension; conductor adds the I2C HAL for your platform.'
see_also:
  - { href: './usage/bus.md', meta: 'PHP API' }
  - { href: './drivers/writing-a-driver.md', meta: 'Drivers' }
  - { href: 'package:php-baremetal/esp32-i2c-hal', meta: 'HAL', label: 'esp32-i2c-hal' }
  - { href: 'package:php-baremetal/ext-events', meta: 'Extension', label: 'ext-events' }
  - { href: 'package:php-baremetal/imu-qmi8658#docs/overview.md', meta: 'Driver', label: 'imu-qmi8658' }
prev: { label: 'No previous page', href: '#' }
next: { label: 'Bus', href: './usage/bus.md' }
---

# I2C bus and devices

ext-i2c lets the PHP script on your board talk to I2C chips. `Baremetal\I2c\Bus` opens a master bus on an
SDA/SCL pin pair and scans it. `Baremetal\I2c\Device` addresses one chip with raw reads and writes or register
access. Chips with a driver package get their own class under `Baremetal\I2c\Driver\`, which extends `Device`,
adds typed methods (`accel()`, `now()`, `heading()`) and can sample the chip in the background, optionally
raising an event per sample.

## Requirements

| Requires | Why |
| --- | --- |
| `php-baremetal/i2c-hal` (virtual) | The platform side: wire handles, bus locking, background poller. On ESP32 it is `php-baremetal/esp32-i2c-hal` (platform `esp32`, over `esp_driver_i2c`), which also brings `php-baremetal/esp32-executor`. |
| `php-baremetal/ext-events` | Event delivery for `poll(..., event: ...)` and the `SamplesReady` class. Its HAL (`esp32-events-hal`) is added the same way. |
| `php-baremetal/bm-common` | The shared `bm_err.h` error codes. |

You never require the HAL yourself. Conductor reads the platform from `extra.baremetal.sdk` (`esp32@^1.0` gives
`esp32`) and adds `php-baremetal/esp32-i2c-hal` and `php-baremetal/esp32-events-hal` while Composer resolves. The
extension's PHP module is `i2c`.

## Install

<!-- @steps -->
- **Install conductor once per machine**

  `composer global require php-baremetal/conductor` keeps the plugin active during the first solve of a new
  project.

- **Create the project**

  `phpbm init my-sensors` writes `composer.json` with conductor (`^1.0`, allowed as a plugin), the package
  registry, and the SDK, board, runtime and storage you pick.

- **Require the extension and any drivers**

  `composer require php-baremetal/ext-i2c php-baremetal/imu-qmi8658`

- **Build and flash**

  `phpbm flash` builds the firmware and flashes it; `phpbm monitor` opens the serial console.
<!-- @endsteps -->

The resulting `composer.json`, for a Waveshare ESP32-S3-Touch-LCD-1.85B (QMI8658 IMU and PCF85063 RTC on
SDA 11 / SCL 10):

<!-- @code-block language="json" label="composer.json" -->
```json
{
  "name": "acme/my-sensors",
  "type": "project",
  "repositories": [
    { "type": "composer", "url": "https://repository.php-baremetal.com", "canonical": true }
  ],
  "require": {
    "php-baremetal/conductor": "^1.0",
    "php-baremetal/ext-i2c": "^1.0",
    "php-baremetal/imu-qmi8658": "^1.0"
  },
  "config": {
    "allow-plugins": { "php-baremetal/conductor": true },
    "platform": { "php": "8.5.11" }
  },
  "extra": {
    "baremetal": {
      "sdk": "esp32@^1.0",
      "board": "waveshare-esp32-s3-touch-lcd-1.85b",
      "runtime": "init-loop",
      "storage": "embedded"
    }
  }
}
```
<!-- @endcode-block -->

## A first script

`project-src/index.php` for the `init-loop` runtime: the top level runs once, then `loop()` runs repeatedly.

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;

$bus = new Bus(sda: 11, scl: 10);
$imu = new Qmi8658($bus);            // 0x6B, 400 kHz

foreach ($bus->scan() as $address => $info) {
    printf("0x%02X %s\n", $address, $info['driver'] ?? '(no driver)');
}

function loop(): void
{
    global $imu;

    [$ax, $ay, $az] = $imu->accel(); // g
    printf("accel [% .2f % .2f % .2f] g\n", $ax, $ay, $az);
    usleep(500000);
}
```
<!-- @endcode-block -->

## PHP API at a glance

| Name | Kind | Meaning |
| --- | --- | --- |
| `Baremetal\I2c\Bus` | class | A master bus on an SDA/SCL pair; `device()`, `scan()`. Constants `SYNC`, `CORE1`. |
| `Baremetal\I2c\Device` | class | One chip on a bus: `probe()`, `read()`, `write()`, `readReg()`, `writeReg()`, `poll()`, `sample()`, `drain()`. |
| `Baremetal\I2c\Driver\*` | classes | One per installed driver package, each extends `Device`. |
| `Baremetal\Sensor\Imu`, `Baremetal\Sensor\Magnetometer`, `Baremetal\Input\Touch` | interfaces | Capability markers a driver class implements. Always defined. |
| `Baremetal\Sensor\Imu\SamplesReady` | class | Event a polled device can raise; extends `Baremetal\Event`. |

The extension defines no functions. Failures throw `\Exception` with a readable message; invalid arguments throw
`\ValueError`. See [Errors and limits](./reference/errors-and-limits.md).

## Drivers

ext-i2c declares the registry `i2c_drivers` (element type `i2c_driver_desc_t`, header `i2c_driver_desc.h`).
Driver packages (`bm-lib`) register a descriptor into it with `extra.baremetal.register`, and at module startup
ext-i2c registers one PHP class per descriptor. Available today:

| Package | Class | Capability |
| --- | --- | --- |
| [imu-qmi8658](package:php-baremetal/imu-qmi8658) | `Baremetal\I2c\Driver\Qmi8658` | `Baremetal\Sensor\Imu` |
| [mag-mmc5603](package:php-baremetal/mag-mmc5603) | `Baremetal\I2c\Driver\Mmc5603` | `Baremetal\Sensor\Magnetometer` |
| [rtc-pcf85063](package:php-baremetal/rtc-pcf85063) | `Baremetal\I2c\Driver\Pcf85063` | none |

See [The i2c_drivers registry](./drivers/registry.md) and, to add a chip, [Writing a driver](./drivers/writing-a-driver.md).

## Notes

- Buses and devices live for the whole run. `new Bus()` with the same pins returns the same bus, and a device
  object is a handle: dropping it never touches the wire. This also holds across requests in the `web-server`
  runtime.
- Each bus is reset when it opens; a transaction that times out resets the bus and is retried once.
- A bus is owned either by the PHP task under a lock (`Bus::SYNC`, default) or by the executor task
  (`Bus::CORE1`). `Bus::CORE1` is refused when the `power-save` option is on. See
  [Bus ownership and power-save](./reference/ownership-and-power.md).
- Internal pull-ups are enabled on SDA and SCL; long wires or many devices still need external pull-ups.
