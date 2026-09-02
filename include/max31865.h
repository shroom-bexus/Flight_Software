// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_MAX31865_H
#define FLIGHT_SOFTWARE_MAX31865_H

#include <Arduino.h>


// Temperature sensor identification

enum class TempSensor : uint8_t
{
    TEMP_1 = 0,
    TEMP_2,
    TEMP_3,
    TEMP_4,
    TEMP_5,
    TEMP_6,
    TEMP_7,
    TEMP_8,
    TEMP_9
};


// Public interface

/**
 * @brief Initialize all enabled MAX31865 channels.
 *
 * Disabled channels are ignored.
 *
 * @return true if all enabled channels initialized successfully.
 */
bool max31865_init();


/**
 * @brief Update all enabled temperature channels.
 *
 * Each channel is handled independently. A fault on one sensor does not
 * prevent the remaining sensors from being updated.
 *
 * @return true if all enabled channels were read without faults.
 */
bool max31865_update();


/**
 * @brief Return the last valid temperature of one sensor.
 *
 * @return Temperature in Kelvin, or NAN if no valid measurement exists.
 */
float max31865_get_temperature(TempSensor sensor);


/**
 * @brief Check whether a channel is enabled in config.h.
 */
bool max31865_is_enabled(TempSensor sensor);


/**
 * @brief Check whether a channel initialized successfully.
 */
bool max31865_is_initialized(TempSensor sensor);


/**
 * @brief Check whether the most recent measurement was valid.
 */
bool max31865_data_valid(TempSensor sensor);


/**
 * @brief Return the most recent MAX31865 fault register.
 *
 * @return Raw MAX31865 fault byte. Zero means no detected fault.
 */
uint8_t max31865_get_fault(TempSensor sensor);


/**
 * @brief Return the accumulated number of measurement faults.
 */
uint32_t max31865_get_error_count(TempSensor sensor);


#endif // FLIGHT_SOFTWARE_MAX31865_H