# Changelog

## 1.1.1

- Added: the irq block of the driver descriptor is served: bm_i2c_device_irq() starts a driver's interrupt service (its INT line serviced on the executor). Requires php-baremetal/i2c-hal ^1.2.
- Added: bm_i2c_device_dev(), the registry device of a Device object, and bm_i2c_dev_state(), the per-instance state block (state_size bytes, zeroed) of a registry device, in bm_i2c_driver.h.
- Changed: ext-i2c no longer registers an empty Baremetal\Input\Touch marker interface up front; ext-input defines it with its methods (a second registration of the name replaced it). Without ext-input the name exists only once a driver declaring it is registered.

## 1.1.0

- Added: ide/ext-i2c.stub, the PHP declarations of the package for IDEs (conductor copies it into vendor/php-baremetal/ide-stubs/).
- Added: Device::poll() returns the device's Baremetal\I2c\Poller (a Baremetal\Source; the same object on every call): stop() pauses sampling, unread samples stay for drain(); start(); setHz() and hz(). Needs php-baremetal/i2c-hal 1.1.
- Added: Baremetal\I2c\SamplesReady with $count (unread samples in the device's ring at delivery), filled by poll() when the event class extends it; Baremetal\Sensor\Imu\SamplesReady now extends it. poll() takes every: N (one event every N stored samples).
- Changed: Baremetal\I2c\SamplesReady declares public readonly int $count, so a user class that extends it (or Baremetal\Sensor\Imu\SamplesReady) and redeclares $count no longer compiles.
- Added: poll() takes coalesce (default true): a sample event is not queued while one of the same event class is still waiting; Events::stats() counts it as coalesced. coalesce: false queues every sample. Requires php-baremetal/ext-events ^1.1.
- Added: Bus::onboard(), from the board's i2c block.
- Added: extra.baremetal.board-blocks: ["i2c:i2c"], so phpbm offers this package as a board feature.
- Docs: errors and limits say that a read can complete with wrong bytes while a line is shorted or bouncing.

## 1.0.2

- Fixed: `Bus::device()`, `new Device()` and the driver constructors accepted any address and clock, and
  truncated them (an address of `0x150` reached the chip at `0x50`). They now throw `\ValueError` for an
  address outside `0x08` to `0x77` and for a clock outside 1 Hz to 1 MHz. A driver constructor still takes
  `$hz = 0` as its default clock.
- Fixed: a driver class name longer than 96 characters (with its namespace) read past a fixed buffer.
- Changed: messages name the real class and option: `must be Bus::SYNC or Bus::CORE1`, and
  `Bus::CORE1 is not available with the power-save option; use Bus::SYNC`.
- Fixed: `clone` on a `Bus` or a device read outside the object and could crash the firmware. It now throws
  `Error`.
- Fixed: `poll()` with an event class started a source while serving an HTTP request (`web-server`), where
  nothing delivers its events. It now throws, like `every()` (with ext-events 1.0.2), before the sampler starts.

## 1.0.1

- Changed: the author's contact email.
- Docs: projects use `"sdk": "esp32@^2.0"`.

## 1.0.0

Initial release.
