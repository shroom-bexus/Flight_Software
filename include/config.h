//   ___ _  _ ___  ___   ___  __  __
//  / __| || | _ \/ _ \ / _ \|  \/  |
//  \__ \ __ |   / (_) | (_) | |\/| |
//  |___/_||_|_|_\\___/ \___/|_|  |_|
// -----------------------------------
// BEXUS - Student Balloon Experiment

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
 *
 * Configuration includes:
 *   - Sensor count and interface configuration
 *   - GPIO and chip-select pins
 *   - UART, SPI, and I2C configuration
 *   - Sensor sampling intervals
 *   - Heater configuration
 *   - Thermal controller parameters
 *   - Logging and telemetry intervals
 */

#ifndef FLIGHT_PRIMARY_CONFIG_H
#define FLIGHT_PRIMARY_CONFIG_H

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
// WSEN-PADS
// ============================================================================

#define WSEN_PADS_ADDRESS    0x5D
#define WSEN_PADS_ODR_HZ     10

#endif //FLIGHT_PRIMARY_CONFIG_H
