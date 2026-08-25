//
// Created by jacob on 25.08.26.
//

#ifndef FLIGHT_SOFTWARE_WSEN_HIDS_H
#define FLIGHT_SOFTWARE_WSEN_HIDS_H


#include <Arduino.h>


/**
 * @brief Initializes the WSEN-HIDS sensor.
 *
 * Wire.begin() must already have been called.
 *
 * @return true if the sensor is reachable.
 */
bool wsen_hids_init();


/**
 * @brief Performs a new temperature and humidity measurement.
 *
 * @return true if a valid measurement was received.
 */
bool wsen_hids_update();


/**
 * @brief Returns the latest temperature measurement.
 *
 * @return Temperature in degrees Celsius.
 */
float wsen_hids_get_temperature();


/**
 * @brief Returns the latest relative humidity measurement.
 *
 * @return Relative humidity in percent.
 */
float wsen_hids_get_humidity();


/**
 * @brief Returns whether at least one valid measurement exists.
 */
bool wsen_hids_is_valid();


/**
 * @brief Returns the number of communication or CRC errors.
 */
uint32_t wsen_hids_get_error_count();


#endif // FLIGHT_SOFTWARE_WSEN_HIDS_H

