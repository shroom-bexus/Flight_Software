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
