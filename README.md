# ext-i2c

An I2C master for the PHP that runs on your board. `Baremetal\I2c\Bus` opens a bus on two pins and scans it,
`Baremetal\I2c\Device` reads and writes one chip (raw bytes or registers), and device-driver packages add typed
classes under `Baremetal\I2c\Driver\` (for example `Qmi8658`) that can also sample in the background and raise
events.

ext-i2c is platform-neutral: it requires the virtual `php-baremetal/i2c-hal`, and conductor adds the HAL for
your SDK platform (`php-baremetal/esp32-i2c-hal` on ESP32). It also pulls in `ext-events`.

## Install

In a project created with `phpbm init`:

```bash
composer require php-baremetal/ext-i2c
```

Add a device driver the same way; a driver requires ext-i2c, so it is enough on its own:

```bash
composer require php-baremetal/imu-qmi8658
```

## Example

```php
<?php
use Baremetal\I2c\Bus;

$bus = new Bus(sda: 11, scl: 10);

foreach ($bus->scan() as $address => $info) {
    printf("0x%02X %s\n", $address, $info['driver'] ?? '(no driver)');
}

$chip = $bus->device(0x51);           // raw device, 100 kHz
$seconds = ord($chip->readReg(0x04)); // one register byte
```

## Documentation

- [Overview](docs/overview.md): requirements, install, a first project, limits.
- [Bus](docs/usage/bus.md) and [Device](docs/usage/device.md): the PHP API.
- [Background sampling and events](docs/usage/polling-and-events.md): `poll()`, `drain()`, `SamplesReady`.
- [Bus ownership and power-save](docs/reference/ownership-and-power.md)
- [Errors and limits](docs/reference/errors-and-limits.md)
- [The i2c_drivers registry](docs/drivers/registry.md) and [Writing a driver](docs/drivers/writing-a-driver.md)

## License

MIT.
