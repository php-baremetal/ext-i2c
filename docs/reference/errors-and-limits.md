---
eyebrow: 'ext-i2c · Reference'
lede:    'Every exception ext-i2c throws, and the fixed sizes and timeouts behind them.'
see_also:
  - { href: '../usage/bus.md', meta: 'PHP API' }
  - { href: '../usage/device.md', meta: 'PHP API' }
prev: { label: 'Bus ownership and power-save', href: './ownership-and-power.md' }
next: { label: 'The i2c_drivers registry', href: '../drivers/registry.md' }
---

# Errors and limits

ext-i2c throws plain `\Exception` for runtime failures and `\ValueError` for invalid arguments; it defines no
exception classes of its own. Messages end with the HAL's error name where one applies (on ESP32 an
`esp_err_t` name such as `ESP_ERR_TIMEOUT`, or one of the two php-baremetal names below).

## Exceptions

| Message | Thrown by | Cause |
| --- | --- | --- |
| `cannot open I2C bus (sda=.. scl=..): ...` | `new Bus()` | Pin or port conflict, no free bus slot, or the HAL could not open the port. |
| `Bus::CORE1 is not available with the power-save option; use Bus::SYNC` | `new Bus()` | `Bus::CORE1` with the `power-save` option on. |
| `address 0x.. on this bus is already used by another driver` | `Bus::device()`, `new Device()`, a driver constructor | The address is held by a device of another kind (raw vs driver, or another driver). |
| `cannot add I2C device 0x..: ...` | `Bus::device()`, `new Device()` | No free device slot, or the HAL could not attach it. |
| `cannot init <driver> at 0x..: ...` | a driver constructor | The chip did not answer, or answered with the wrong identity (`ESP_ERR_NOT_FOUND`). |
| `I2C scan aborted, bus stuck (repeated timeouts on SDA=.. SCL=..): ...` | `Bus::scan()` | Eight consecutive probe timeouts. |
| `I2C read failed: ...`, `I2C write failed: ...` | `read()`, `write()` | The transfer failed after its retry. |
| `I2C readReg 0x.. failed: ...`, `I2C writeReg 0x.. failed: ...` | `readReg()`, `writeReg()` | The transfer failed after its retry. |
| `this device has no poller (driver has no poll block)` | `poll()`, `sample()`, `drain()` | Raw device, or a driver without a sampler. |
| `not polling; call poll() first` | `sample()` | No sampler started for this device. |
| `cannot start poller (too many, or out of memory)` | `poll()` | Eight samplers already running, or no memory for the ring. |
| `poll(): unknown event class '...' or too many sources` | `poll()` | The class is not loaded, or 16 event source classes are already in use. |
| `poll() is not available in the web-server runtime: event sources need the event-driven runtime` | `poll()` with an event | Called in `web-server` (a request or the `init` script), where nothing delivers events. Poll without an event, or use `event-driven`. |
| `Trying to clone an uncloneable object of class ...` | `clone` | A copy would not own the bus or the device: keep one object for each. |
| `I2c\Bus is not initialised`, `I2c\Device is not initialised` | any method | The object was created without its constructor running (for example a subclass that skipped it). |

`\ValueError` is thrown for `$owner` not `Bus::SYNC`/`Bus::CORE1`, for an `$address` outside `0x08` to `0x77`
(`must be a 7-bit address between 0x08 and 0x77`), for a device `$hz` outside 1 to 1000000
(`must be between 1 and 1000000 (Hz)`; a driver constructor takes 0 as its default clock), and for a `$length`
or a sampler `$hz` that is not greater than 0. Driver methods throw their own `\Exception` messages (for example `QMI8658 accel read failed`).

The php-baremetal error names: `address already used by another driver on this bus` and
`the port or a pin is already used by an I2C bus open with other pins`.

## Limits

| Limit | Value | Source |
| --- | --- | --- |
| Open buses | 4 | ext-i2c bus table |
| Devices (raw and driver, all buses) | 32 | ext-i2c device table |
| Scan range and accepted addresses | `0x08` to `0x77` | ext-i2c |
| Device clock | 1 Hz to 1 MHz | ext-i2c |
| Default device clock | 100 kHz (drivers set their own, 400 kHz for the packaged ones) | ext-i2c, driver descriptor |
| Transaction timeout | 1000 ms, then a bus reset and one retry | esp32-i2c-hal |
| Probe timeout | 50 ms, no retry | esp32-i2c-hal |
| Scan abort | after 8 consecutive probe timeouts | esp32-i2c-hal |
| Wait for the executor queue (`CORE1`) | 1000 ms | esp32-i2c-hal |
| Background samplers | 8 in the whole firmware | esp32-executor |
| Sample size | up to 64 bytes | esp32-executor |
| Ring depth | 1 to 256 samples (default 64) | esp32-executor |
| Samples returned by one `drain()` | 256 | ext-i2c |
| Event source classes | 16, shared with every `ext-events` source | ext-events |

Addresses are 7-bit; 10-bit addressing is not supported. Tables are never freed: buses and devices stay open
until the board restarts.
