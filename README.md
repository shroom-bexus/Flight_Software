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
