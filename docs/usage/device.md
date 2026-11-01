---
eyebrow: 'ext-i2c · PHP API'
lede:    'Baremetal\I2c\Device is one chip on a bus: raw reads and writes, register access, and the background sampler that driver classes use.'
see_also:
  - { href: './bus.md', meta: 'PHP API' }
  - { href: './polling-and-events.md', meta: 'PHP API' }
  - { href: '../recipes/read-write-registers.md', meta: 'Recipe' }
prev: { label: 'Bus', href: './bus.md' }
next: { label: 'Background sampling and events', href: './polling-and-events.md' }
---

# Device

A `Device` addresses one 7-bit address on a `Bus`. Get one from `Bus::device()` or construct it. Every
driver class (`Baremetal\I2c\Driver\Qmi8658`, …) extends `Device`, so the raw methods below also work on a
driver object, next to its typed methods.

The object is a handle into a table that lives for the whole run: dropping it never touches the hardware, and
asking for the same address again returns the same device.

## Constructor

<!-- @method name="new Device(Bus $bus, int $address, int $hz = 100000)" visibility="public" -->

<!-- @params -->
<!-- @param name="$bus" type="Bus" required -->
The bus the chip is wired to.
<!-- @endparam -->
<!-- @param name="$address" type="int" required -->
7-bit address, e.g. `0x51`, from `0x08` to `0x77`; anything else throws `\ValueError` (the other values are
reserved by the I2C specification).
<!-- @endparam -->
<!-- @param name="$hz" type="int" default="100000" -->
SCL clock for this device, 1 to 1000000; anything else throws `\ValueError`. Ignored when the device
already exists.
<!-- @endparam -->
<!-- @endparams -->

Throws `\Exception` when a driver already holds the address, when all 32 device slots are in use, or when the
HAL cannot attach the device.

## Methods

<!-- @method name="Device::probe(): bool" returns="bool" visibility="public" -->

`true` when the chip acknowledges its address (50 ms timeout). Never throws for a missing chip.

<!-- @method name="Device::read(int $length): string" returns="string" visibility="public" -->

Reads `$length` bytes from the chip. `\ValueError` when `$length` is not greater than 0.

<!-- @method name="Device::write(string $data): void" returns="void" visibility="public" -->

Writes the bytes of `$data` in one transaction.

<!-- @method name="Device::readReg(int $reg, int $length = 1): string" returns="string" visibility="public" -->

Writes the register address `$reg`, then reads `$length` bytes (a combined write-read). Returns a binary
string: use `ord()` for one byte, `unpack()` for more. `\ValueError` when `$length` is not greater than 0.

<!-- @method name="Device::writeReg(int $reg, string|int $value): void" returns="void" visibility="public" -->

Writes to register `$reg`. An `int` writes one byte (the low 8 bits); a `string` writes all its bytes after the
register address.

<!-- @method name="Device::poll(int $hz, int $depth = 64, ?string $event = null): void" returns="void" visibility="public" -->

<!-- @method name="Device::sample(): ?string" returns="?string" visibility="public" -->

<!-- @method name="Device::drain(): array" returns="array" visibility="public" -->

Background sampling, available on driver classes whose driver declares a sampler. A raw `Device` throws
`this device has no poller (driver has no poll block)`. See
[Background sampling and events](./polling-and-events.md).

## Errors

A failed transfer throws `\Exception` with the operation and the HAL's error name (an `esp_err_t` name on
ESP32), for example `I2C readReg 0x00 failed: ESP_ERR_TIMEOUT`. A transaction that times out (or
finds the bus in an invalid state) has already been retried once after a bus reset. Full list in
[Errors and limits](../reference/errors-and-limits.md).

## Example

<!-- @code-block language="php" label="raw register access" -->
```php
<?php
use Baremetal\I2c\Bus;

$rtc = (new Bus(sda: 11, scl: 10))->device(0x51);

if (!$rtc->probe()) {
    exit("no chip at 0x51\n");
}

$raw = $rtc->readReg(0x04, 3);        // seconds, minutes, hours (BCD)
[, $s, $m, $h] = unpack('C3', $raw);
$bcd = fn (int $v): int => ($v >> 4) * 10 + ($v & 0x0F);
printf("%02d:%02d:%02d\n", $bcd($h & 0x3F), $bcd($m & 0x7F), $bcd($s & 0x7F));
```
<!-- @endcode-block -->
