# Changelog

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
