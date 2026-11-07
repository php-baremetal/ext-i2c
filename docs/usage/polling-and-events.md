---
eyebrow: 'ext-i2c · PHP API'
lede:    'A driver with a sampler can read its chip in the background at a fixed rate into a ring buffer. PHP reads the ring without bus traffic, and can get an event per sample.'
see_also:
  - { href: '../recipes/imu-events.md', meta: 'Recipe' }
  - { href: '../reference/ownership-and-power.md', meta: 'Reference' }
  - { href: 'package:php-baremetal/ext-events', meta: 'Extension', label: 'ext-events' }
prev: { label: 'Device', href: './device.md' }
next: { label: 'Bus ownership and power-save', href: '../reference/ownership-and-power.md' }
---

# Background sampling and events

`poll()` hands the sampling of one device to the executor task: it reads the chip at the requested rate and
stores each raw sample in a ring buffer. `sample()` and `drain()` read that ring and never touch the bus. With
`$event` set, stored samples also raise an event (every sample, or every `$every` samples) that the
`event-driven` runtime delivers to your listeners.

Only driver classes whose driver declares a sampler support this (today `Qmi8658` and `Mmc5603`). On a raw
`Device` or on `Pcf85063`, these methods throw `this device has no poller (driver has no poll block)`.

## poll()

<!-- @method name="Device::poll(int $hz, int $depth = 64, ?string $event = null, bool $coalesce = true, int $every = 1): Baremetal\I2c\Poller" returns="Baremetal\I2c\Poller" visibility="public" -->

<!-- @params -->
<!-- @param name="$hz" type="int" required -->
Sampling rate. The period is whole milliseconds (`1000 / $hz`, at least 1 ms), so rates above 1000 run at
1000 Hz and other rates round up (300 Hz samples every 3 ms). `\ValueError` when not greater than 0.
<!-- @endparam -->
<!-- @param name="$depth" type="int" default="64" -->
Ring size in samples, at most 256 (larger values are clamped). When the ring is full, the oldest unread samples
are overwritten.
<!-- @endparam -->
<!-- @param name="$event" type="?string" default="null" -->
Event class to raise after every `$every` stored samples (default 1, so after each one), usually `Baremetal\Sensor\Imu\SamplesReady::class`. The event's
`device` property is this device. `null` samples silently.
<!-- @endparam -->
<!-- @param name="$coalesce" type="bool" default="true" -->
Only with `$event`. While an event of that class is still waiting in the queue, the next sample does not queue
another one: `Events::stats()` counts it as `coalesced` and the listener sees one event for several samples
(read them all with `drain()`). The rule is per event class, so two devices polled with the same class share it.
`false` queues one event per sample; a full queue then drops the extra ones (counted as `dropped`).
<!-- @endparam -->
<!-- @param name="$every" type="int" default="1" -->
Raise one event every `$every` stored samples instead of every sample (`every: 10` at 100 Hz is 10 events a
second); it has an effect only with `$event`, but it is checked even without one. Between 1 and the depth of the
device's ring, else `\ValueError`. That ring is fixed by the first `poll()` on the device (0 means 64, at most
256), not by the `$depth` of a later call. A second `poll()` on the same device sets `$every` again.
<!-- @endparam -->
<!-- @endparams -->

Calling `poll()` again on the same device changes its rate instead of starting a second sampler. Throws
`\Exception` with `cannot start poller (too many, or out of memory)` when the executor already runs eight
samplers (a firmware-wide limit of `esp32-executor`), and with
`poll(): unknown event class '...' or too many sources` when `$event` is not a loaded class, cannot be
instantiated (abstract class, interface, trait, enum), or the event source table (32 entries) is full.

## The Poller

`poll()` returns the device's `Baremetal\I2c\Poller`, a `Baremetal\Source`. There is one per device: every
`poll()` call on the device returns the same object (while a handle is alive), and a new `poll()` after the
handle was discarded gives a new one. Dropping the handle does not stop the sampler: it keeps sampling, and
the handle keeps the device object alive.

- `stop()` pauses sampling: no new samples are taken, no events are raised, and the chip is not read (a sample already being read when `stop()` lands can still arrive). The samples
  already in the ring stay, so `drain()` and `sample()` still return them after `stop()`. Events already in
  the queue are still delivered.
- `start()` resumes. The next sample is taken at once, and the `every:` window counts from 0 again.
  A `poll()` on a stopped device also resumes it.
- `running()` is `false` after `stop()`; `stop()` on a stopped poller and `start()` on a running one do nothing.
- `hz(): int` is the rate asked for; `setHz(int $hz): void` changes it. On a stopped poller it only saves the
  rate, applied by `start()`. `\ValueError` `Poller::setHz(): $hz must be greater than 0` for 0 or less.

```php
$poller = $imu->poll(hz: 100, event: SamplesReady::class);
$poller->stop();        // quiet: no sampling while nobody looks
$poller->setHz(50);
$poller->start();       // sampling again, at 50 Hz
```

A `Poller` stored on its own device object (for example a property of a `Device` subclass) forms a reference cycle that is freed only at shutdown.

`new Baremetal\I2c\Poller()` throws `\Error` `Cannot directly construct Baremetal\I2c\Poller`.

## sample() and drain()

<!-- @method name="Device::sample(): ?string" returns="?string" visibility="public" -->

The most recent raw sample, or `null` if none has been taken yet. Throws `not polling; call poll() first`
before `poll()`.

<!-- @method name="Device::drain(): array" returns="array" visibility="public" -->

Every unread sample, oldest first, as a list of raw strings (at most 256 per call), and marks them read.
Returns `[]` before `poll()`.

Raw samples are the chip's bytes (12 for `Qmi8658`, 9 for `Mmc5603`). Each such driver has a `decode()` method
that turns one into numbers:

<!-- @code-block language="php" label="init-loop: drain and decode" -->
```php
<?php
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));
$imu->poll(hz: 100, depth: 128);

function loop(): void
{
    global $imu;

    foreach ($imu->drain() as $raw) {
        ['accel' => [$ax, $ay, $az], 'gyro' => [$gx, $gy, $gz]] = $imu->decode($raw);
        printf("a [% .2f % .2f % .2f] g  w [% 6.1f % 6.1f % 6.1f] dps\n", $ax, $ay, $az, $gx, $gy, $gz);
    }
    usleep(200000);
}
```
<!-- @endcode-block -->

## SamplesReady and capabilities

ext-i2c defines two event classes at startup. `Baremetal\I2c\SamplesReady` extends `Baremetal\Event` and works for
any polled device; `Baremetal\Sensor\Imu\SamplesReady` extends it and is the one to use for an IMU. Both carry
`public readonly int $count`: the unread samples in the device's ring when the event is delivered (at most
`$depth`; `drain()` takes them and resets it). `poll()` fills `$count` for these classes and their subclasses; any
other `Baremetal\Event` class still works, without `$count`. Listen for it with
`Baremetal\Events::listen()` in the `event-driven` runtime. The sampler posts one event every `$every` stored samples (default 1)
(with `coalesce: true`, the default, none while one is still waiting); if no small event slot is free the event
is dropped, but the sample stays in the ring, so a handler that calls `drain()` catches up.

Driver classes implement a capability interface, so a handler can accept any chip of a kind:

| Interface | Implemented by |
| --- | --- |
| `Baremetal\Sensor\Imu` | `Qmi8658` |
| `Baremetal\Sensor\Magnetometer` | `Mmc5603` |
| `Baremetal\Input\Touch` (from ext-input) | `Cst816s` ([touch-cst816s](package:php-baremetal/touch-cst816s)) |

The IMU and magnetometer interfaces are always defined, so type hints resolve even when no driver for them is installed. A
driver may also name a new interface; ext-i2c creates it on demand.

<!-- @callout variant="note" title="Where the work runs" -->
The sampler runs on the executor task, never on the PHP task. Its notification only posts to the event queue;
your listener always runs on the PHP task.
<!-- @endcallout -->
