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
