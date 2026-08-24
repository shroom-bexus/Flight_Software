//   ___ _  _ ___  ___   ___  __  __
//  / __| || | _ \/ _ \ / _ \|  \/  |
//  \__ \ __ |   / (_) | (_) | |\/| |
//  |___/_||_|_|_\\___/ \___/|_|  |_|
// -----------------------------------
// BEXUS - Student Balloon Experiment

#ifndef FLIGHT_SOFTWARE_MAX31865_H
#define FLIGHT_SOFTWARE_MAX31865_H

#include <Arduino.h>


// ============================================================================
// Temperature sensor identification
// ============================================================================

/**
 * @brief Identifiers for the individual temperature sensors.
 *
 * Each enum value corresponds to one MAX31865 / PT1000 measurement channel.
 * The enum is used by get_temp() so that other parts of the program do not
 * need to know anything about chip-select pins or SPI configuration.
 *
 * Additional sensors can be added here later.
 */
enum class TempSensor : uint8_t
{
    TEMP_1 = 0

    // Later:
    // TEMP_2,
    // TEMP_3,
    // TEMP_4,
    // TEMP_5,
    // TEMP_6
};


// ============================================================================
// Public interface
// ============================================================================

/**
 * @brief Initializes all configured MAX31865 devices.
 *
 * Every MAX31865 is configured for a 2-wire PT1000 connection.
 * This function must be called once during startup before update_temp().
 *
 * @return true  All sensors initialized successfully.
 * @return false At least one sensor failed to initialize.
 */
bool max31865_init();


/**
 * @brief Reads all configured temperature sensors.
 *
 * This function performs the actual SPI communication with the MAX31865
 * devices and stores the measured temperatures internally.
 *
 * The function should be called periodically according to the desired
 * temperature measurement interval.
 *
 * get_temp() only returns these stored values and does not trigger a new
 * measurement.
 */
void max31865_update();


/**
 * @brief Returns the most recently measured temperature of one sensor.
 *
 * No SPI communication takes place inside this function. It only accesses
 * the value stored during the most recent update_temp() call.
 *
 * @param sensor Temperature sensor to read.
 *
 * @return Last measured temperature in degrees K.
 */
float max31865_get_temperature(TempSensor sensor);


#endif // FLIGHT_SOFTWARE_MAX31865_H




