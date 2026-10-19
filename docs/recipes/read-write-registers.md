---
eyebrow: 'ext-i2c · Recipe'
lede:    'Talk to a chip that has no driver: read and write its registers with a raw Device.'
see_also:
  - { href: '../usage/device.md', meta: 'PHP API' }
  - { href: '../drivers/writing-a-driver.md', meta: 'Drivers' }
prev: { label: 'Scan the bus', href: './scan-the-bus.md' }
next: { label: 'Stream IMU samples as events', href: './imu-events.md' }
---

# Read and write registers

Any chip works through a raw `Device`, no driver needed. This recipe sets and reads the PCF85063 real-time
clock at `0x51` on a Waveshare ESP32-S3-Touch-LCD-1.85B (SDA 11 / SCL 10) using its registers directly. Use the
same project `composer.json` as [Scan the bus](./scan-the-bus.md).

## The registers

| Register | Content |
| --- | --- |
| `0x00` | Control 1: `0x00` keeps the clock running. |
| `0x04` to `0x0A` | Seconds, minutes, hours, day, weekday, month, year, in BCD. Bit 7 of seconds set means the time is not valid (power was lost). |

## The script

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
use Baremetal\I2c\Bus;

$rtc = (new Bus(sda: 11, scl: 10))->device(0x51);   // 100 kHz raw device

if (!$rtc->probe()) {
    exit("no RTC at 0x51\n");
}

$bcd = fn (int $v): int => ($v >> 4) * 10 + ($v & 0x0F);
$toBcd = fn (int $v): int => (intdiv($v, 10) << 4) | ($v % 10);

$rtc->writeReg(0x00, 0x00);                          // one byte: clock running

$raw = unpack('C7', $rtc->readReg(0x04, 7));         // seven registers in one read
if ($raw[1] & 0x80) {
    // seconds, minutes, hours, day, weekday, month, year (2026-10-19 09:00:00)
    $rtc->writeReg(0x04, pack('C7', 0, 0, $toBcd(9), $toBcd(19), 0, $toBcd(10), $toBcd(26)));
    $raw = unpack('C7', $rtc->readReg(0x04, 7));
}

printf("20%02d-%02d-%02d %02d:%02d:%02d\n",
    $bcd($raw[7]), $bcd($raw[6] & 0x1F), $bcd($raw[4] & 0x3F),
    $bcd($raw[3] & 0x3F), $bcd($raw[2] & 0x7F), $bcd($raw[1] & 0x7F));
```
<!-- @endcode-block -->

- `readReg($reg, $n)` is one combined write-read; `unpack('C7', …)` turns the bytes into a 1-based array.
- `writeReg($reg, int)` writes one byte; `writeReg($reg, string)` writes every byte of the string after the
  register address, here all seven time registers at once.

<!-- @callout variant="tip" title="One owner per address" -->
A raw device and a driver cannot share an address. With rtc-pcf85063 installed, use `new Pcf85063($bus)` and
its `now()` / `set()` instead of `$bus->device(0x51)`: mixing them throws
`address 0x51 on this bus is already used by another driver`.
<!-- @endcallout -->
