```text
          ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
          ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
          ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
          ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
          ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
          ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝

 Stratospheric High-Altitude Radiation Observation of Organismic Mycology
```

# SHROOM Flight Software

Flight software for the SHROOM stratospheric balloon experiment.

The software runs on two Teensy 4.1 microcontrollers and handles sensor
acquisition, data logging, thermal control, telemetry, and experiment control.

## Hardware

Main controllers:

- Teensy 4.1 Primary
- Teensy 4.1 Secondary
## Building

Use PlatformIO from this directory:
grep -n -E '^(<<<<<<<|=======|>>>>>>>)' README.md
```sh
pio run -e primary
pio run -e secondary
```

`include/config.h` selects features for each board and holds pin assignments,
sampling periods, calibration, heater limits, and downlink priorities.

## Where to change the code

| Module | Responsibility |
| --- | --- |
| `src/main.cpp` | Startup order and cooperative sampling scheduler |
| `src/max31865.cpp`, `src/wsen_*.cpp` | Sensor access, validation, and recovery |
| `src/airdos.cpp` | Read complete raw UART lines for each configured channel |
| `src/heater.cpp` | Per-channel PWM output and power limits |
| `src/thermal_control.cpp` | PID, temperature cutoff, and EEPROM settings |
| `src/logger.cpp` | Mirror CSV logs to internal SD and XTSD; storage failures are independent |
| `src/telemetry.cpp` | Format measurements and periodic health messages |
| `src/ethernet_link.cpp` | UDP, system/AIRDOS queues, rate limiting, and automatic AIRDOS selection |
| `src/commands.cpp` | Parse commands, validate arguments, and reply; suppress duplicate command IDs |
| `src/storage_download.cpp` | USB log download protocol; pauses normal flight activity until reset |
| `tools/download_logs.py` | PC client for USB log downloads |

The scheduler samples MAX31865 before updating thermal control. Valid samples
are logged and passed to telemetry. AIRDOS logging receives every complete
UART line independently of downlink selection. System telemetry has priority
over AIRDOS telemetry; command replies are immediate and charged to the same
rate schedule.

To add a command, add a handler and an entry in `command_table`. Scalar settings
can reuse `handle_scalar_setting`; the setter remains responsible for range
validation. To add a log, declare its `MirroredLogFile` and add its filename and
CSV header to `log_files`. Opening, flushing, and closing then include it
automatically. Keep per-sensor CSV formatting in the corresponding log function.
For USB access to a new filename, also update `known_log_files` in
`storage_download.cpp` and the PC downloader as needed.

EEPROM currently preserves thermal enable, target, manual heater outputs, and
PID gains. The downlink limit starts from `config.h` after reset and is refreshed
by the ground station. Changing persistence or protocol formats requires a
separate compatibility change.

## Secondary AIRDOS logging and forwarding

Both firmwares must be updated together. The secondary reads AIRDOS 1-7;
AIRDOS 8-9 remain on the primary. Each secondary message is written to
`airdos.csv` on its internal SDIO card and its SPI XTSD backup. The existing
mirrored logger disables a failed destination independently, so a missing SD
does not prevent the other copy or UART forwarding.

The secondary then forwards the complete line and sensor ID over `Serial1`
at **2,000,000 baud, 8N1**. The primary stores a received copy on both of its
SD devices and sends the existing `AIRDOS,time_ms,sensor,data` format to the
GS. The current Groundstation parser and logger already support IDs 1-9;
no GS update is required. All nine sensors share the existing downlink
priority table, queues, and bandwidth limiter. Downlink suppression never
suppresses secondary logging or the inter-Teensy link.

### PCB v2.2 connections

The MCU sheet already crosses the `TX_T` and `RX_T` nets:
secondary pin **1 (TX1)** goes to primary pin **0 (RX1)**; primary pin 1 goes
to secondary pin 0. The boards share GND and use 3.3 V UART logic. This
implementation uses only the secondary-to-primary direction. I2C remains
available for the existing sensors; UART avoids polling for this stream.

The sensor net names `TX1` through `TX9` mean the sensor's transmit signal.
They connect to the following Teensy hardware receive pins:

| AIRDOS ID | Teensy | Arduino port | Teensy RX pin | Teensy TX pin |
| --- | --- | --- | --- | --- |
| 1 | Secondary | Serial2 | 7 | 8 |
| 2 | Secondary | Serial3 | 15 | 14 |
| 3 | Secondary | Serial4 | 16 | 17 |
| 4 | Secondary | Serial5 | 21 | 20 |
| 5 | Secondary | Serial6 | 25 | 24 |
| 6 | Secondary | Serial7 | 28 | 29 |
| 7 | Secondary | Serial8 | 34 | 35 |
| 8 | Primary | Serial2 | 7 | 8 |
| 9 | Primary | Serial7 | 28 | 29 |

This mapping is taken from `ShroomPCBv2.2.pdf`, MCU sheet, and checked against
[PJRC's Teensy serial implementation](https://github.com/PaulStoffregen/cores/tree/master/teensy4).
All AIRDOS ports use 115200 baud. Secondary XTSD remains on SPI pins
MOSI=11, MISO=12, SCK=13, CS=10.

### Buffers, time and failure behavior

- Each AIRDOS has an additional 4 KiB receive buffer. Each channel processes
  at most eight complete lines per scheduler visit so busy inputs share time.
- The secondary UART link has a 32 KiB transmit buffer; the primary has a
  32 KiB receive buffer. The sender checks space for a **whole** frame and
  never waits for buffer space. A full transmit buffer drops the new frame,
  increments a counter and reports that counter over USB when available.
  Local logging has already been attempted independently.
- Link frames are `\n!A,<ID>,<raw line>\n`. Commas in the payload are preserved.
  AIRDOS lines must start with `$` and fit the existing 255-character limit.
  Overlong or malformed frames are discarded until newline; the next intact
  frame can be received after either Teensy resets. No ACK, retransmission
  or checksum is implemented. Electrically corrupted but syntactically valid
  data and hardware receive-buffer loss are not reliably detected.
- `\n!O,<ID>,<source parser overflow count>\n` reports each source's overlong
  line counter every five seconds. Remote GS AIRDOS health uses the primary's
  last data reception time (FAULT after 15 seconds without data). The overflow
  field is the latest received source count, initially zero; it is not a
  count of all transport losses. Link parser errors are exposed by
  `teensy_link_get_error_count()` on the primary.
- Secondary CSV timestamps belong to the secondary; primary CSV and downlink
  timestamps describe primary reception. This change does not synchronize
  the two RTCs or their `millis()` counters.
- Live data is **not replayed** after a link or GS outage. The local SD copies
  are the recovery source. Queues are finite; prolonged SD stalls or sustained
  overload can still lose input. No loss-free throughput claim has been
  established without hardware testing. The existing one-second flush period
  also means the newest buffered data can be lost on power failure.
- USB log-download mode deliberately pauses acquisition and forwarding on the
  selected Teensy until reset, as before.

### Validation

Build both targets with `pio run -e primary -e secondary`. Run the standalone
host tests with `bash test/host/run.sh` (requires g++). They exercise the actual
AIRDOS parser and link module with fake serial ports: all seven input ports,
fragmentation, maximum line length, malformed/overlong input, resynchronizing
after a reset, overflow counters, and atomic rejection when TX space is full.
They do not simulate SD timing, UART electrical behavior or radio throughput.

Before flight, test all nine sources together, compare the two secondary
`airdos.csv` copies, check ID 1-7 reception on the primary and GS, repeat with
one secondary storage device unavailable, interrupt the inter-Teensy link,
reset either board, and verify recovery and the configured downlink limit.

## Selectable thermal regulator

Use the updated Groundstation terminal commands:

```text
thermal off
hysteresis 0.5
bbpower 30
thermal bangbang
thermal on
```

This example selects bang-bang with thresholds at target ±0.5 K and 30% ON
power. Choose ON power for your heater supply; the fresh-setting default is
100%, subject to existing heater limits. PID remains the initial regulator.
Use `thermal pid` to switch back. `pid <Kp> <Ki> <Kd>` only changes gains;
it does not select PID. `thermal on/off` enables/disables the selected regulator.

Bang-bang turns ON at or below target minus hysteresis, OFF at or above target
plus hysteresis, and retains its state inside that band. Hysteresis is a
half-width (>0..10 K); ON power accepts 0..100%. Both regulators use the same
sensor, target, output limits and overtemperature cutoff. The cutoff always
wins, even if the upper hysteresis threshold is above it. There is no automatic
fallback. Missing/invalid/non-finite sensor readings switch heating off.

Switching an enabled regulator clears its history and immediately turns heating
off until the next valid sample. Bang-bang starts OFF inside the band, including
after a reboot or sensor recovery. Changes to its target, hysteresis or power
also restart it OFF. Selecting a regulator while thermal control is OFF does
not enable it or change saved manual outputs.

Regulator selection, hysteresis and ON power survive resets. EEPROM version-2
PID gains, target, enabled state and manual settings are preserved on upgrade.
PID gains remain available when switching back from bang-bang.

The flight computer sends `THERMAL_CONFIG,time_ms,mode,hysteresis_K,on_power_percent`
every health interval (5 s). The GS displays the reported configuration and logs
it in `thermal_config.csv`; the dashboard labels the selected regulator.
Existing THERMAL/PID messages remain unchanged. Update both repositories for
command and display support. New settings may take up to a health interval to
appear, plus any downlink queue delay.

Validation: host-side regression tests exercise the real controller source with
simulated EEPROM, sensor and heater interfaces. Hardware timing, PWM and thermal
response still require a Teensy bench test before use.

## Independent storage health

Every five seconds the Primary sends separate messages for its own storage
and the Secondary's storage, using Primary reception time for the health batch:

```text
HEALTH,<time_ms>,SD_INTERNAL,<state>,<errors>
HEALTH,<time_ms>,SD_BACKUP,<state>,<errors>
HEALTH,<time_ms>,SD_SECONDARY_INTERNAL,<state>,<errors>
HEALTH,<time_ms>,SD_SECONDARY_BACKUP,<state>,<errors>
```

These replace the aggregate `SD` health entry. Existing per-card initialization,
open/header, write-length and flush/sync error checks feed the individual counters.
A failed device stops logging; the other device continues independently.
There is no automatic storage reinitialization. Counters reset on board reboot.

The Secondary transmits `!S,<storage>,<state>,<errors>` over the same Serial1 link
as AIRDOS. Storage is 0 (internal) or 1 (backup); state is 0 (disabled), 1 (OK)
or 2 (fault). Frames are bounded and skipped if the UART queue is full, then
retried at the next health interval. The Primary validates each frame and
reports WAITING before the first report, or STALE when status is absent for
more than three health periods (15 s). STALE is a communication/observability
warning, not a detected SD write error; its counter is the last reported count.

The GS presents temperatures and accepts `target` in °C. Flight control,
telemetry, raw commands and CSV temperatures continue to use Kelvin.


### Primary RTC telemetry

The primary sends `RTC,time_ms,valid,timestamp_utc` every 5 seconds with health
telemetry, through the existing bandwidth-limited system queue. For example:
`RTC,12345,1,2026-09-21T12:34:56Z`. An unsynchronized clock sends
`RTC,12345,0,`. UTC uses the same TimeLib/hardware RTC source as SD timestamps;
validity confirms synchronization, not accuracy against an external clock.
The terminal and browser dashboard show the last received sample in UTC and
its reception age (not transport latency), with STALE after 15 seconds or
while disconnected. No PC-clock substitution or RTC-setting command is used.
Update both firmware and GS; older firmware leaves the display waiting.
