// SHROOM Flight Software
// Central build and hardware configuration

#ifndef FLIGHT_SOFTWARE_CONFIG_H
#define FLIGHT_SOFTWARE_CONFIG_H

#include <Arduino.h>

#include "max31865.h"

// Build target (set by platformio.ini)
#ifndef FLIGHT_PRIMARY
#define FLIGHT_PRIMARY 0
#endif

#ifndef FLIGHT_SECONDARY
#define FLIGHT_SECONDARY 0
#endif

#if (FLIGHT_PRIMARY + FLIGHT_SECONDARY) != 1
#error "Exactly one flight software build target must be selected."
#endif

// Feature selection
#if FLIGHT_PRIMARY
#define ENABLE_ETHERNET        true
#define ENABLE_SD_LOGGING      true
#define ENABLE_HEATERS         true
#define ENABLE_THERMAL_CONTROL true
#define ENABLE_MAX31865        true
#define ENABLE_WSEN_PADS       true
#define ENABLE_WSEN_HIDS       true
#define ENABLE_WSEN_ISDS       false
#define ENABLE_AIRDOS          true
#else
#define ENABLE_ETHERNET        false
#define ENABLE_SD_LOGGING      true
#define ENABLE_HEATERS         false
#define ENABLE_THERMAL_CONTROL false
#define ENABLE_MAX31865        false
#define ENABLE_WSEN_PADS       false
#define ENABLE_WSEN_HIDS       false
#define ENABLE_WSEN_ISDS       false
#define ENABLE_AIRDOS          true
#endif

// Timing
constexpr uint32_t SD_FLUSH_PERIOD_MS = 1000;
constexpr uint32_t MAX31865_SAMPLE_PERIOD_MS = 1000;
constexpr uint32_t WSEN_PADS_SAMPLE_PERIOD_MS = 2000;
constexpr uint32_t WSEN_HIDS_SAMPLE_PERIOD_MS = 4000;
constexpr uint32_t WSEN_ISDS_SAMPLE_PERIOD_MS = 10;
constexpr uint32_t HEALTH_TELEMETRY_PERIOD_MS = 5000;

// Shared I2C bus
constexpr uint32_t I2C_CLOCK_HZ = 400000;

// MAX31865 / PT1000 channels
constexpr uint8_t MAX31865_CHANNEL_COUNT = 9;
// All nine flight channels are always monitored. Missing/disconnected RTDs
// are reported through health telemetry as FAULT instead of being hidden.
constexpr bool MAX31865_ENABLED[MAX31865_CHANNEL_COUNT] =
{
    true, true, true, true, true, true, true, true, true
};
constexpr uint8_t MAX31865_CS_PINS[MAX31865_CHANNEL_COUNT] =
{
    16, 17, 20, 21, 30, 31, 34, 35, 36
};
constexpr float MAX31865_RREF[MAX31865_CHANNEL_COUNT] =
{
    4300.0f, 4300.0f, 4300.0f,
    4300.0f, 4300.0f, 4300.0f,
    4300.0f, 4300.0f, 4300.0f
};
// T_calibrated = T_measured * scale + offset.
constexpr float MAX31865_SCALE[MAX31865_CHANNEL_COUNT] =
{
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f
};
constexpr float MAX31865_OFFSET_C[MAX31865_CHANNEL_COUNT] =
{
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f
};
constexpr float MAX31865_RNOMINAL = 1000.0f;
#define MAX31865_SPI_BUS SPI

// Heaters
constexpr uint8_t HEATER_CHANNEL_COUNT = 4;
constexpr bool HEATER_ENABLED[HEATER_CHANNEL_COUNT] =
{
    true, true, true, true
};
constexpr uint8_t HEATER_PINS[HEATER_CHANNEL_COUNT] =
{
    2, 3, 4, 5
};
constexpr float HEATER_MAX_POWER_PERCENT[HEATER_CHANNEL_COUNT] =
{
    100.0f, 100.0f, 100.0f, 100.0f
};
constexpr uint32_t HEATER_PWM_FREQUENCY_HZ = 100;

// Thermal control
constexpr TempSensor THERMAL_CONTROL_SENSOR = TempSensor::TEMP_1;
constexpr float THERMAL_TARGET_K = 298.15f;
constexpr float THERMAL_DEFAULT_KP = 0.0f; // % / K
constexpr float THERMAL_DEFAULT_KI = 0.0f; // % / (K s)
constexpr float THERMAL_DEFAULT_KD = 0.0f; // % s / K
constexpr float THERMAL_MAX_PID_GAIN = 1000.0f;
constexpr float THERMAL_MAX_OUTPUT_PERCENT = 100.0f;
constexpr float THERMAL_MAX_TEMPERATURE_K = 313.15f;

// WSEN sensors
constexpr uint8_t WSEN_PADS_ADDRESS = 0x5D;
constexpr uint16_t WSEN_PADS_ODR_HZ = 10;
constexpr uint8_t WSEN_HIDS_I2C_ADDRESS = 0x44;

// AIRDOS03
constexpr uint32_t AIRDOS_BAUD_RATE = 115200;
constexpr uint32_t AIRDOS_TIMEOUT_MS = 15000;

#if FLIGHT_PRIMARY
// Primary PCB routing: AIRDOS 8 -> Serial2, AIRDOS 9 -> Serial7.
constexpr uint8_t AIRDOS_CHANNEL_COUNT = 2;
constexpr uint8_t AIRDOS_SENSOR_IDS[AIRDOS_CHANNEL_COUNT] = {8, 9};
#define AIRDOS_8_SERIAL Serial2
#define AIRDOS_9_SERIAL Serial7
#else
// Secondary AIRDOS 1-7 are implemented separately later. Keep the existing
// single-channel behavior until that integration is done.
constexpr uint8_t AIRDOS_CHANNEL_COUNT = 1;
constexpr uint8_t AIRDOS_SENSOR_IDS[AIRDOS_CHANNEL_COUNT] = {0};
#define AIRDOS_LEGACY_SERIAL Serial7
#endif

// AIRDOS raw-data downlink priority
//
// Each row is one sample group. Within a row, the first sensor has the
// highest downlink priority and the third sensor has the lowest priority.
// Change only this table if the physical sample/AIRDOS assignment changes.
// Every AIRDOS sensor ID 1-9 should occur exactly once.
//
// Automatic downlink levels:
//   level 3 -> all three entries from every row = 9 AIRDOS
//   level 2 -> first two entries from every row  = 6 AIRDOS
//   level 1 -> first entry from every row         = 3 AIRDOS
//   level 0 -> no AIRDOS raw-data downlink
//
// Local SD logging is not affected by these levels.
constexpr uint8_t AIRDOS_SAMPLE_GROUP_COUNT = 3;
constexpr uint8_t AIRDOS_SENSORS_PER_GROUP = 3;
constexpr uint8_t AIRDOS_DOWNLINK_MAX_LEVEL = AIRDOS_SENSORS_PER_GROUP;
constexpr uint8_t AIRDOS_DOWNLINK_PRIORITY
    [AIRDOS_SAMPLE_GROUP_COUNT][AIRDOS_SENSORS_PER_GROUP] =
{
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9}
};

// Queue-pressure hysteresis for automatic 9 -> 6 -> 3 -> 0 selection.
constexpr uint8_t AIRDOS_DOWNLINK_QUEUE_HIGH_PERCENT = 50;
constexpr uint8_t AIRDOS_DOWNLINK_QUEUE_LOW_PERCENT = 10;
constexpr uint32_t AIRDOS_DOWNLINK_REDUCE_HOLD_MS = 1500;
constexpr uint32_t AIRDOS_DOWNLINK_RESTORE_HOLD_MS = 30000;

// Ethernet
constexpr uint8_t ETHERNET_LOCAL_IP[] = {172, 16, 18, 131};
constexpr uint8_t ETHERNET_SUBNET[] = {255, 255, 255, 0}; // TODO: confirm with SSC
constexpr uint8_t ETHERNET_GATEWAY[] = {0, 0, 0, 0};
constexpr uint8_t ETHERNET_DNS[] = {0, 0, 0, 0};
constexpr uint16_t ETHERNET_UDP_PORT = 5000;
constexpr size_t ETHERNET_RX_BUFFER_SIZE = 256;
constexpr size_t ETHERNET_UDP_PAYLOAD_MAX = 1200;

// Telemetry is queued line-by-line. Small system lines may share a compact
// UDP packet, while each AIRDOS raw line stays in its own packet. This avoids
// the old one-second mega-batches without wasting excessive Ethernet overhead.
// System telemetry always has priority over AIRDOS raw data.
constexpr size_t ETHERNET_TELEMETRY_LINE_MAX = 384;
constexpr size_t ETHERNET_SYSTEM_PACKET_PAYLOAD_MAX = 256;
constexpr size_t ETHERNET_SYSTEM_QUEUE_DEPTH = 32;
constexpr size_t ETHERNET_AIRDOS_QUEUE_DEPTH = 48;

constexpr uint32_t ETHERNET_GROUND_STATION_TIMEOUT_MS = 60000;
// 0 disables the limiter until the ground station applies its saved setting.
constexpr float ETHERNET_DEFAULT_DOWNLINK_LIMIT_KBIT_S = 0.0f;
constexpr float ETHERNET_MIN_DOWNLINK_LIMIT_KBIT_S = 2.0f;
constexpr float ETHERNET_MAX_DOWNLINK_LIMIT_KBIT_S = 10000.0f;

#endif // FLIGHT_SOFTWARE_CONFIG_H
