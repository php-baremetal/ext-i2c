---
eyebrow: 'ext-i2c · PHP API'
lede:    'Baremetal\I2c\Bus is a handle onto one I2C master bus. Same pins, same bus: constructing it twice is safe and cheap.'
see_also:
  - { href: './device.md', meta: 'PHP API' }
  - { href: '../reference/ownership-and-power.md', meta: 'Reference' }
  - { href: '../recipes/scan-the-bus.md', meta: 'Recipe' }
prev: { label: 'Overview', href: '../overview.md' }
next: { label: 'Device', href: './device.md' }
---

# Bus

`Baremetal\I2c\Bus` opens an I2C master on an SDA/SCL pin pair. The bus lives for the whole run: a second
`new Bus()` with the same pins (and a matching or automatic port) returns the same underlying bus, so libraries
can each construct their own `Bus` object without coordinating.

## Constructor

<!-- @method name="new Bus(int $sda, int $scl, int $port = -1, int $owner = Bus::SYNC)" visibility="public" -->

<!-- @params -->
<!-- @param name="$sda" type="int" required -->
GPIO number of the data line.
<!-- @endparam -->
<!-- @param name="$scl" type="int" required -->
GPIO number of the clock line.
<!-- @endparam -->
<!-- @param name="$port" type="int" default="-1" -->
Hardware I2C port. `-1` lets the HAL pick the first free port.
<!-- @endparam -->
<!-- @param name="$owner" type="int" default="Bus::SYNC" -->
Who drives the wire: `Bus::SYNC` (the PHP task, under a per-bus lock) or `Bus::CORE1` (the executor task).
Honoured only when the bus is first created; later constructions with the same pins get the existing bus.
<!-- @endparam -->
<!-- @endparams -->

Throws `\ValueError` when `$owner` is neither constant, and `\Exception` when:

- `$owner` is `Bus::CORE1` and the project enables the `power-save` option;
- a pin is already used by an open bus with other pins, or `$port` is taken by a bus on other pins;
- all four bus slots are in use, or the HAL cannot open the port.

The bus is reset right after it opens, which frees a slave left holding SDA by an interrupted transfer.

## Constants

| Constant | Value | Meaning |
| --- | --- | --- |
| `Bus::SYNC` | `0` | The calling PHP task runs each transaction while holding the bus lock. Default. |
| `Bus::CORE1` | `1` | The executor task owns the wire; every transaction is handed to it and the caller waits. |

See [Bus ownership and power-save](../reference/ownership-and-power.md) for when to pick `CORE1`.

## device()

<!-- @method name="Bus::device(int $address, int $hz = 100000): Device" returns="Device" visibility="public" -->

Attaches the chip at 7-bit `$address` (`0x08` to `0x77`) with clock `$hz` (1 to 1000000) and returns a raw
`Device`; values out of range throw `\ValueError`. Same as `new Device($bus, $address, $hz)`.

Devices are kept per bus and address: asking again for an address returns the existing device (the new `$hz`
is ignored). If a driver class already holds that address, it throws
`address 0x.. on this bus is already used by another driver`, and the reverse holds too: a driver cannot be
constructed on an address taken by a raw device.

## scan()

<!-- @method name="Bus::scan(): array" returns="array" visibility="public" -->

Probes every address from `0x08` to `0x77` and returns the ones that answered, as
`[address => ['driver' => ?string]]`. `driver` is the registry name of the driver mounted at that address
(`'qmi8658'`), or `null` for a raw device or nothing constructed yet.

<!-- @code-block language="php" label="scan" -->
```php
<?php
use Baremetal\I2c\Bus;

$bus = new Bus(sda: 11, scl: 10);

foreach ($bus->scan() as $address => $info) {
    printf("0x%02X %s\n", $address, $info['driver'] ?? '(no driver)');
}
```
<!-- @endcode-block -->

Each probe waits at most 50 ms. After eight consecutive timeouts the sweep stops, resets the bus and throws
`I2C scan aborted, bus stuck (repeated timeouts on SDA=.. SCL=..)`: usually missing pull-ups or swapped pins.
The sweep runs under the bus lock (`SYNC`) or on the executor (`CORE1`), like any transaction.
