
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
 * @brief Central configuration file for the SHROOM experiment firmware.
 *
 * Contains compile-time configuration parameters used throughout the project,
 * including sensor configuration, communication interfaces, timing intervals,
 * heater settings, and control parameters.
 *
 * This file should contain configuration values only.
 * Hardware drivers and application logic must not be implemented here.
 */

#ifndef FLIGHT_SOFTWARE_CONFIG_H
#define FLIGHT_SOFTWARE_CONFIG_H


// ============================================================================
// Build target
// ============================================================================

// FLIGHT_PRIMARY and FLIGHT_SECONDARY are defined in platformio.ini.

#ifndef FLIGHT_PRIMARY
#define FLIGHT_PRIMARY 0
#endif

#ifndef FLIGHT_SECONDARY
#define FLIGHT_SECONDARY 0
#endif

// Exactly one target must be selected.
#if (FLIGHT_PRIMARY + FLIGHT_SECONDARY) != 1
#error "Exactly one SHROOM build target must be selected."
#endif


// ============================================================================
// System features
// ============================================================================

#if FLIGHT_PRIMARY

#define ENABLE_ETHERNET        1
#define ENABLE_TELEMETRY       1
#define ENABLE_SD_LOGGING      1

// Sensors connected to the primary
#define ENABLE_MAX31865        1
#define ENABLE_WSEN_PADS       1
#define ENABLE_WSEN_HIDS       1
#define ENABLE_WSEN_ISDS       1
#define ENABLE_AIRDOS          0


#elif FLIGHT_SECONDARY

#define ENABLE_ETHERNET        0
#define ENABLE_TELEMETRY       0
#define ENABLE_SD_LOGGING      1

// Sensors connected to the secondary
#define ENABLE_MAX31865        0
#define ENABLE_WSEN_PADS       0
#define ENABLE_WSEN_HIDS       0
#define ENABLE_WSEN_ISDS       0
#define ENABLE_AIRDOS          1

#endif


// ============================================================================
// Data logging
// ============================================================================

// Select which available sensors are written to the SD card
#define LOG_MAX31865       1
#define LOG_WSEN_PADS      1
#define LOG_WSEN_HIDS      1
#define LOG_WSEN_ISDS      0
#define LOG_AIRDOS         1

// Force pending SD data to be synchronized every second
#define SD_FLUSH_PERIOD_MS 1000


// ============================================================================
// Sensor sampling intervals
// ============================================================================

// MAX31865 / PT1000
#define MAX31865_SAMPLE_PERIOD_MS 1000

// WSEN-PADS pressure sensor
#define WSEN_PADS_SAMPLE_PERIOD_MS 2000

// WSEN-HIDS humidity sensor
#define WSEN_HIDS_SAMPLE_PERIOD_MS 4000

// WSEN-ISDS IMU
#define WSEN_ISDS_SAMPLE_PERIOD_MS 10

// ============================================================================
// MAX31865 temperature sensors
// ============================================================================

// Number of MAX31865 devices used in the experiment
#define MAX31865_SENSOR_COUNT 1
// TODO: Update number of sensors.

// PT1000 nominal resistance at 0 °C
#define MAX31865_RNOMINAL 1000.0f

// Reference resistors
#define MAX31865_RREF_1 4300.0f
// #define MAX31865_RREF_2 4300.0f
// #define MAX31865_RREF_3 4300.0f
// #define MAX31865_RREF_4 4300.0f
// #define MAX31865_RREF_5 4300.0f
// #define MAX31865_RREF_6 4300.0f
// TODO: Replace with individually measured resistor values.

// SPI configuration
#define MAX31865_SPI_BUS SPI

// Chip-select pins
#define MAX31865_CS_1 16
// #define MAX31865_CS_2 ...
// #define MAX31865_CS_3 ...
// #define MAX31865_CS_4 ...
// #define MAX31865_CS_5 ...
// #define MAX31865_CS_6 ...
// TODO: Replace with the actual chip-select pins.


// ============================================================================
// WSEN-PADS pressure sensor
// ============================================================================

#define WSEN_PADS_ADDRESS    0x5D
#define WSEN_PADS_ODR_HZ     10


// ============================================================================
// WSEN-HIDS - Humidity and temperature sensor
// ============================================================================

// Fixed I2C address
constexpr uint8_t WSEN_HIDS_I2C_ADDRESS = 0x44;

#endif // FLIGHT_SOFTWARE_CONFIG_H