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
`$event` set, every stored sample also raises an event that the `event-driven` runtime delivers to your
listeners.

Only driver classes whose driver declares a sampler support this (today `Qmi8658` and `Mmc5603`). On a raw
`Device` or on `Pcf85063`, these methods throw `this device has no poller (driver has no poll block)`.

## poll()

<!-- @method name="Device::poll(int $hz, int $depth = 64, ?string $event = null): void" returns="void" visibility="public" -->

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
Event class to raise after each stored sample, usually `Baremetal\Sensor\Imu\SamplesReady::class`. The event's
`device` property is this device. `null` samples silently.
<!-- @endparam -->
<!-- @endparams -->

Calling `poll()` again on the same device changes its rate instead of starting a second sampler. Throws
`\Exception` with `cannot start poller (too many, or out of memory)` when the executor already runs eight
samplers (a firmware-wide limit of `esp32-executor`), and with
`poll(): unknown event class '...' or too many sources` when `$event` is not a loaded class or the event
source table (16 classes) is full.

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

`Baremetal\Sensor\Imu\SamplesReady` extends `Baremetal\Event`; ext-i2c defines it at startup. Listen for it with
`Baremetal\Events::listen()` in the `event-driven` runtime. The sampler posts one event per stored sample; if
the event queue is full the event is dropped, but the sample stays in the ring, so a handler that calls
`drain()` catches up.

Driver classes implement a capability interface, so a handler can accept any chip of a kind:

| Interface | Implemented by |
| --- | --- |
| `Baremetal\Sensor\Imu` | `Qmi8658` |
| `Baremetal\Sensor\Magnetometer` | `Mmc5603` |
| `Baremetal\Input\Touch` | no packaged driver yet |

The three interfaces are always defined, so type hints resolve even when no driver for them is installed. A
driver may also name a new interface; ext-i2c creates it on demand.

<!-- @callout variant="note" title="Where the work runs" -->
The sampler runs on the executor task, never on the PHP task. Its notification only posts to the event queue;
your listener always runs on the PHP task.
<!-- @endcallout -->
