---
eyebrow: 'ext-i2c · Recipe'
lede:    'List every chip that answers on the bus, with the driver mounted at each address. The first thing to run on new wiring.'
see_also:
  - { href: '../usage/bus.md', meta: 'PHP API' }
  - { href: '../reference/errors-and-limits.md', meta: 'Reference' }
prev: { label: 'Writing a driver', href: '../drivers/writing-a-driver.md' }
next: { label: 'Read and write registers', href: './read-write-registers.md' }
---

# Scan the bus

A project with ext-i2c only, for a Waveshare ESP32-S3-Touch-LCD-1.85B, whose on-board QMI8658 (`0x6B`) and
PCF85063 (`0x51`) sit on SDA 11 / SCL 10. On another board, change the board id and the pins.

<!-- @code-block language="json" label="composer.json" -->
```json
{
  "name": "acme/i2c-scan",
  "type": "project",
  "repositories": [
    { "type": "composer", "url": "https://repository.php-baremetal.com", "canonical": true }
  ],
  "require": {
    "php-baremetal/conductor": "^1.0",
    "php-baremetal/ext-i2c": "^1.0"
  },
  "config": {
    "allow-plugins": { "php-baremetal/conductor": true },
    "platform": { "php": "8.5.11" }
  },
  "extra": {
    "baremetal": {
      "sdk": "esp32@^2.0",
      "board": "waveshare-esp32-s3-touch-lcd-1.85b",
      "runtime": "init-loop",
      "storage": "embedded",
      "options": { "restart-delay": 3600 }
    }
  }
}
```
<!-- @endcode-block -->

The script runs once and ends; `restart-delay` keeps the board from restarting for an hour, so the output stays
on the console.

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
use Baremetal\I2c\Bus;

const SDA = 11;
const SCL = 10;

$bus = new Bus(sda: SDA, scl: SCL);

printf("I2C scan (sda=%d scl=%d):\n", SDA, SCL);
$found = $bus->scan();
foreach ($found as $address => $info) {
    printf("  0x%02X  %s\n", $address, $info['driver'] ?? '(no driver)');
}
printf("%d device(s)\n", count($found));
```
<!-- @endcode-block -->

Output on a bus with just those two chips:

<!-- @code-block language="text" label="serial console" -->
```text
I2C scan (sda=11 scl=10):
  0x51  (no driver)
  0x6B  (no driver)
2 device(s)
```
<!-- @endcode-block -->

`(no driver)` means no driver object has been constructed for that address yet. Construct one first (for
example `new Qmi8658($bus)` from imu-qmi8658) and the scan shows its name.

## When it fails

- **Empty result**: wrong pins, or the chips are unpowered. Swap SDA and SCL if unsure.
- **`I2C scan aborted, bus stuck`**: eight probes in a row timed out, typically no pull-ups on a bare breakout
  or a chip holding SDA low. The bus has been reset; fix the wiring and restart.
- **`cannot open I2C bus`**: another `Bus` already uses one of the pins with a different pair.
