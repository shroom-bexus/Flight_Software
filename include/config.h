// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

/**
 * @file config.h
 * @brief Central configuration of the SHROOM flight software.
 *
 * This file contains configuration values only.
 * Hardware drivers and application logic belong in their respective modules.
 */

#ifndef FLIGHT_SOFTWARE_CONFIG_H
#define FLIGHT_SOFTWARE_CONFIG_H

#include <Arduino.h>
#include "max31865.h"

// ============================================================================
// Build target
// ============================================================================

// Defined through platformio.ini.
#ifndef FLIGHT_PRIMARY
#define FLIGHT_PRIMARY 0
#endif

#ifndef FLIGHT_SECONDARY
#define FLIGHT_SECONDARY 0
#endif

// Exactly one target must be selected.
#if (FLIGHT_PRIMARY + FLIGHT_SECONDARY) != 1
#error "Exactly one flight software build target must be selected."
#endif


// ============================================================================
// System features
// ============================================================================
//
// These values remain macros because they are used by #if directives.
//

#if FLIGHT_PRIMARY

#define ENABLE_ETHERNET         true
#define ENABLE_TELEMETRY        true
#define ENABLE_SD_LOGGING       true
#define ENABLE_HEATERS          true
#define ENABLE_THERMAL_CONTROL  false

#define ENABLE_MAX31865         true
#define ENABLE_WSEN_PADS        true
#define ENABLE_WSEN_HIDS        true
#define ENABLE_WSEN_ISDS        false
#define ENABLE_AIRDOS           true

#elif FLIGHT_SECONDARY

#define ENABLE_ETHERNET         false
#define ENABLE_TELEMETRY        false
#define ENABLE_SD_LOGGING       true
#define ENABLE_HEATERS          false
#define ENABLE_THERMAL_CONTROL  false

#define ENABLE_MAX31865         false
#define ENABLE_WSEN_PADS        false
#define ENABLE_WSEN_HIDS        false
#define ENABLE_WSEN_ISDS        false
#define ENABLE_AIRDOS           true

#endif


// ============================================================================
// Console
// ============================================================================

#define ENABLE_SERIAL_CONSOLE   true
#define ENABLE_ETHERNET_CONSOLE true


// ============================================================================
// Data logging
// ============================================================================

#define LOG_MAX31865  true
#define LOG_WSEN_PADS true
#define LOG_WSEN_HIDS true
#define LOG_WSEN_ISDS false
#define LOG_AIRDOS    true

// Pending SD data is committed periodically.
// Data written since the previous flush can be lost during sudden power loss.
constexpr uint32_t SD_FLUSH_PERIOD_MS = 1000;


// ============================================================================
// I2C
// ============================================================================

// Shared I2C bus speed for the onboard sensors.
constexpr uint32_t I2C_CLOCK_HZ = 400000;


// ============================================================================
// Sensor sampling intervals
// ============================================================================

constexpr uint32_t MAX31865_SAMPLE_PERIOD_MS = 1000;
constexpr uint32_t WSEN_PADS_SAMPLE_PERIOD_MS = 2000;
constexpr uint32_t WSEN_HIDS_SAMPLE_PERIOD_MS = 4000;
constexpr uint32_t WSEN_ISDS_SAMPLE_PERIOD_MS = 10;


// ============================================================================
// MAX31865 temperature sensors
// ============================================================================

// Number of available MAX31865 measurement channels.
constexpr uint8_t MAX31865_CHANNEL_COUNT = 9;


// ---------------------------------------------------------------------------
// Enabled channels
// ---------------------------------------------------------------------------
//
// Set a channel to false if no RTD is connected.
// Disabled channels are completely skipped during initialization and sampling.

#define MAX31865_TEMP_1_ENABLED true
#define MAX31865_TEMP_2_ENABLED false
#define MAX31865_TEMP_3_ENABLED false
#define MAX31865_TEMP_4_ENABLED false
#define MAX31865_TEMP_5_ENABLED false
#define MAX31865_TEMP_6_ENABLED false
#define MAX31865_TEMP_7_ENABLED false
#define MAX31865_TEMP_8_ENABLED false
#define MAX31865_TEMP_9_ENABLED false


// ---------------------------------------------------------------------------
// PT1000 configuration
// ---------------------------------------------------------------------------

// Nominal PT1000 resistance at 0 °C.
constexpr float MAX31865_RNOMINAL = 1000.0f;


// ---------------------------------------------------------------------------
// Reference resistors
// ---------------------------------------------------------------------------
//
// Replace these values with the individually measured resistance
// of each MAX31865 reference resistor.

constexpr float MAX31865_RREF_1 = 4300.0f;
constexpr float MAX31865_RREF_2 = 4300.0f;
constexpr float MAX31865_RREF_3 = 4300.0f;
constexpr float MAX31865_RREF_4 = 4300.0f;
constexpr float MAX31865_RREF_5 = 4300.0f;
constexpr float MAX31865_RREF_6 = 4300.0f;
constexpr float MAX31865_RREF_7 = 4300.0f;
constexpr float MAX31865_RREF_8 = 4300.0f;
constexpr float MAX31865_RREF_9 = 4300.0f;


// ---------------------------------------------------------------------------
// Temperature calibration
// ---------------------------------------------------------------------------
//
// Linear calibration:
// T_calibrated = T_measured * scale + offset
//
// Leave scale = 1.0 and offset = 0.0 until calibration data is available.

constexpr float MAX31865_SCALE_1 = 1.0f;
constexpr float MAX31865_SCALE_2 = 1.0f;
constexpr float MAX31865_SCALE_3 = 1.0f;
constexpr float MAX31865_SCALE_4 = 1.0f;
constexpr float MAX31865_SCALE_5 = 1.0f;
constexpr float MAX31865_SCALE_6 = 1.0f;
constexpr float MAX31865_SCALE_7 = 1.0f;
constexpr float MAX31865_SCALE_8 = 1.0f;
constexpr float MAX31865_SCALE_9 = 1.0f;

constexpr float MAX31865_OFFSET_C_1 = 0.0f;
constexpr float MAX31865_OFFSET_C_2 = 0.0f;
constexpr float MAX31865_OFFSET_C_3 = 0.0f;
constexpr float MAX31865_OFFSET_C_4 = 0.0f;
constexpr float MAX31865_OFFSET_C_5 = 0.0f;
constexpr float MAX31865_OFFSET_C_6 = 0.0f;
constexpr float MAX31865_OFFSET_C_7 = 0.0f;
constexpr float MAX31865_OFFSET_C_8 = 0.0f;
constexpr float MAX31865_OFFSET_C_9 = 0.0f;


// ---------------------------------------------------------------------------
// SPI configuration
// ---------------------------------------------------------------------------

#define MAX31865_SPI_BUS SPI

// ---------------------------------------------------------------------------
// Chip-select pins
// ---------------------------------------------------------------------------

constexpr uint8_t MAX31865_CS_1 = 16;
constexpr uint8_t MAX31865_CS_2 = 17;
constexpr uint8_t MAX31865_CS_3 = 20;
constexpr uint8_t MAX31865_CS_4 = 21;
constexpr uint8_t MAX31865_CS_5 = 30;
constexpr uint8_t MAX31865_CS_6 = 31;
constexpr uint8_t MAX31865_CS_7 = 34;
constexpr uint8_t MAX31865_CS_8 = 35;
constexpr uint8_t MAX31865_CS_9 = 36;


// ============================================================================
// Heaters
// ============================================================================

constexpr uint8_t HEATER_CHANNEL_COUNT = 4;

// ---------------------------------------------------------------------------
// Enabled channels
// ---------------------------------------------------------------------------

#define HEATER_1_ENABLED true
#define HEATER_2_ENABLED true
#define HEATER_3_ENABLED true
#define HEATER_4_ENABLED true

// ---------------------------------------------------------------------------
// GPIO pins
// ---------------------------------------------------------------------------

constexpr uint8_t HEATER_PIN_1 = 2;
constexpr uint8_t HEATER_PIN_2 = 3;
constexpr uint8_t HEATER_PIN_3 = 4;
constexpr uint8_t HEATER_PIN_4 = 5;

// ---------------------------------------------------------------------------
// PWM configuration
// ---------------------------------------------------------------------------

// Heater loads are thermally slow. A low PWM frequency reduces unnecessary
// switching while still providing sufficiently smooth power control.
constexpr uint32_t HEATER_PWM_FREQUENCY_HZ = 100;

// Maximum allowed output power for each heater.
// Can later also be changed through telemetry commands.
constexpr float HEATER_1_MAX_POWER_PERCENT = 100.0f;
constexpr float HEATER_2_MAX_POWER_PERCENT = 100.0f;
constexpr float HEATER_3_MAX_POWER_PERCENT = 100.0f;
constexpr float HEATER_4_MAX_POWER_PERCENT = 100.0f;


// ============================================================================
// Thermal control
// ============================================================================

// MAX31865 sensor used for temperature control.
constexpr TempSensor THERMAL_CONTROL_SENSOR =
    TempSensor::TEMP_1;


// Target temperature.
constexpr float THERMAL_TARGET_K = 298.15f;


// PID parameters.
//
// Units:
// Kp: % / K
// Ki: % / (K s)
// Kd: % s / K

constexpr float THERMAL_KP = 0.0f;
constexpr float THERMAL_KI = 0.0f;
constexpr float THERMAL_KD = 0.0f;


// Maximum allowed heater output.
constexpr float THERMAL_MAX_OUTPUT_PERCENT = 100.0f;


// Software safety cutoff.
constexpr float THERMAL_MAX_TEMPERATURE_K = 313.15f;


// ============================================================================
// WSEN-PADS pressure sensor
// ============================================================================

constexpr uint8_t WSEN_PADS_ADDRESS = 0x5D;
constexpr uint16_t WSEN_PADS_ODR_HZ = 10;


// ============================================================================
// WSEN-HIDS humidity sensor
// ============================================================================

constexpr uint8_t WSEN_HIDS_I2C_ADDRESS = 0x44;


// ============================================================================
// AIRDOS radiation sensors
// ============================================================================

// UART baud rate used by AIRDOS03.
constexpr uint32_t AIRDOS_BAUD_RATE = 115200;

// First AIRDOS sensor.
// RX9/TX9 on the SHROOM PCB is connected to Teensy Serial7.
#define AIRDOS_1_SERIAL Serial7



// ============================================================================
// Ethernet
// ============================================================================

// SHROOM experiment IP assigned by BEXUS/SSC.
constexpr uint8_t ETHERNET_LOCAL_IP[4] =
{
    172,16,18,131
};

// Ground Station IP assigned by BEXUS/SSC.
constexpr uint8_t GROUND_STATION_IP[4] =
{
    172,16,18,130
};

// TODO: Confirm subnet mask with SSC.
constexpr uint8_t ETHERNET_SUBNET[4] =
{
    255,255,255,0
};

constexpr uint8_t ETHERNET_GATEWAY[4] =
{
    0,0,0,0
};

constexpr uint8_t ETHERNET_DNS[4] =
{
    0,0,0,0
};

constexpr uint16_t ETHERNET_TCP_PORT = 5000;

constexpr size_t ETHERNET_RX_BUFFER_SIZE = 256;


#endif // FLIGHT_SOFTWARE_CONFIG_H