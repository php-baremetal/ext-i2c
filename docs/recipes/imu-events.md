---
eyebrow: 'ext-i2c · Recipe'
lede:    'The executor samples the IMU in the background and raises SamplesReady; the event-driven runtime calls your handler, which drains and decodes the samples.'
see_also:
  - { href: '../usage/polling-and-events.md', meta: 'PHP API' }
  - { href: '../reference/ownership-and-power.md', meta: 'Reference' }
  - { href: 'package:php-baremetal/imu-qmi8658#docs/overview.md', meta: 'Driver', label: 'imu-qmi8658' }
  - { href: 'package:php-baremetal/ext-events', meta: 'Extension', label: 'ext-events' }
prev: { label: 'Read and write registers', href: './read-write-registers.md' }
next: { label: 'No next page', href: '#' }
---

# Stream IMU samples as events

The QMI8658 on a Waveshare ESP32-S3-Touch-LCD-1.85B (SDA 11 / SCL 10), sampled at 10 Hz by the executor. Each
sample raises `SamplesReady`; the handler reads every sample collected since the last event.

## The project

`runtime` is `event-driven`: the script registers its handlers and returns, then the reactor waits for events.

<!-- @code-block language="json" label="composer.json" -->
```json
{
  "name": "acme/imu-events",
  "type": "project",
  "repositories": [
    { "type": "composer", "url": "https://repository.php-baremetal.com", "canonical": true }
  ],
  "require": {
    "php-baremetal/conductor": "^1.0",
    "php-baremetal/ext-events": "^1.0",
    "php-baremetal/ext-i2c": "^1.0",
    "php-baremetal/imu-qmi8658": "^1.0"
  },
  "config": {
    "allow-plugins": { "php-baremetal/conductor": true },
    "platform": { "php": "8.5.11" }
  },
  "extra": {
    "baremetal": {
      "sdk": "esp32@^2.0",
      "board": "waveshare-esp32-s3-touch-lcd-1.85b",
      "runtime": "event-driven",
      "storage": "embedded"
    }
  }
}
```
<!-- @endcode-block -->

ext-events would come in through ext-i2c anyway; requiring it states that the script uses `Events` directly.

## The script

<!-- @code-block language="php" label="project-src/index.php" -->
```php
<?php
use Baremetal\Events;
use Baremetal\I2c\Bus;
use Baremetal\I2c\Driver\Qmi8658;
use Baremetal\Sensor\Imu\SamplesReady;

$imu = new Qmi8658(new Bus(sda: 11, scl: 10));

$seen = 0;

Events::listen(SamplesReady::class, function (SamplesReady $e) use (&$seen): void {
    foreach ($e->device->drain() as $raw) {
        ['accel' => [$ax, $ay, $az]] = $e->device->decode($raw);
        printf("#%d accel [% .2f % .2f % .2f] g\n", ++$seen, $ax, $ay, $az);
    }
});

$imu->poll(hz: 10, depth: 32, event: SamplesReady::class);

echo "imu-events: sampling at 10 Hz\n";
```
<!-- @endcode-block -->

## How it works

- `poll()` starts a sampler on the executor task: every 100 ms it reads 12 bytes from the chip into a ring of
  32 samples, then posts a `SamplesReady` event carrying the device.
- The reactor delivers the event on the PHP task; `$e->device` is the `Qmi8658` object.
- `drain()` returns the unread samples from the ring, oldest first, without touching the bus. If events were
  dropped because the queue was full, the next `drain()` still returns those samples.
- The handler is typed on `SamplesReady` and only uses `drain()` and `decode()`, so it works for any polled
  driver whose `decode()` returns `accel`.

With the `power-save` option on this still works on the default `Bus::SYNC` bus, but the 10 Hz sampler wakes the
chip ten times a second.
