---
eyebrow: 'ext-i2c · Reference'
lede:    'Who drives the wire: the PHP task under a lock (Bus::SYNC) or the executor task (Bus::CORE1). The choice matters for interrupts, background samplers and the power-save option.'
see_also:
  - { href: '../usage/bus.md', meta: 'PHP API' }
  - { href: '../usage/polling-and-events.md', meta: 'PHP API' }
  - { href: 'package:php-baremetal/esp32-i2c-hal', meta: 'HAL', label: 'esp32-i2c-hal' }
  - { href: 'package:php-baremetal/esp32-executor', meta: 'Library', label: 'esp32-executor' }
prev: { label: 'Background sampling and events', href: '../usage/polling-and-events.md' }
next: { label: 'Errors and limits', href: './errors-and-limits.md' }
---

# Bus ownership and power-save

The firmware runs PHP on its own task on core 1. Next to it, `esp32-executor` provides one **executor** task on
core 0 (8 KB stack in internal RAM) that runs device samplers and work handed over by extensions. A bus's
`owner`, fixed when the bus is first created, decides which of the two drives the wire.

## Bus::SYNC

The default. The caller runs each transaction itself while holding a per-bus mutex. From PHP the caller is the
PHP task; a background sampler of a device on the same bus runs on the executor and takes the same mutex, so the
two never overlap on the wire. The I2C interrupt is bound to the core that opened the bus, here core 1.

## Bus::CORE1

The executor task owns the wire. The bus is opened on the executor, and every transaction from PHP (including
`scan()` and a driver's constructor) is queued to it while the PHP task waits for the result. Samplers already
run there, so on this bus they need no lock round-trip with PHP. If the executor queue stays full for one
second, the call fails with `ESP_ERR_TIMEOUT`.

<!-- @callout variant="note" title="About the name" -->
`Bus::CORE1` names the ownership mode, not the core: the executor task is pinned to core 0, and the PHP task
stays on core 1.
<!-- @endcallout -->

Pick `CORE1` when a bus carries background samplers and you want PHP's own transactions serialised with them on
one task; keep `SYNC` otherwise. Both behave the same from PHP.

## Power-save

The SDK option `power-save` (`"options": { "power-save": "on" }` in `extra.baremetal`) lets the chip enter light
sleep automatically whenever every task is waiting.

- `new Bus(..., owner: Bus::CORE1)` throws while power-save is on:
  `Bus::CORE1 is not available with the power-save option; use Bus::SYNC`. Use `Bus::SYNC`.
- `poll()` is accepted on a `SYNC` bus, but the sampler wakes the chip once per period, so the chip never
  sleeps longer than `1000 / $hz` ms. ext-i2c has no call that stops a sampler, so keep rates low and start
  one only when the project needs it.
- Buses are not closed around light sleep; the descriptor's sleep hooks are not called by ext-i2c (see
  [Writing a driver](../drivers/writing-a-driver.md)).

## Rules that follow from the firmware

- Native code never calls PHP from the executor: a sampler's notification only posts to the event queue, and
  listeners run on the PHP task.
- I2C transfers do not touch the flash, so they are allowed on the PHP task (whose stack is in PSRAM).
- A driver's `init` and PHP methods run on the PHP task; only their transfers are routed (lock or executor).
  A driver's `poll.sample` runs on the executor, so it must not call PHP or the Zend allocator.
