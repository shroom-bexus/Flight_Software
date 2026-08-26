// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#ifndef FLIGHT_SOFTWARE_WSEN_HIDS_H
#define FLIGHT_SOFTWARE_WSEN_HIDS_H

#include <Arduino.h>


/**
 * @brief Initialize the WSEN-HIDS sensor.
 *
 * The I2C bus must already be initialized before calling this function.
 *
 * @return true if the sensor responded successfully.
 */
bool wsen_hids_init();


/**
 * @brief Perform a new temperature and humidity measurement.
 *
 * The new values are stored internally and can be accessed using the
 * getter functions.
 *
 * @return true if a valid measurement was received.
 */
bool wsen_hids_update();


/**
 * @brief Return the latest temperature measurement.
 *
 * No new sensor measurement is performed.
 *
 * @return Temperature in Kelvin.
 */
float wsen_hids_get_temperature();


/**
 * @brief Return the latest relative humidity measurement.
 *
 * No new sensor measurement is performed.
 *
 * @return Relative humidity in percent.
 */
float wsen_hids_get_humidity();


/**
 * @brief Check whether at least one valid measurement is available.
 *
 * @return true if valid sensor data has been received.
 */
bool wsen_hids_data_valid();


/**
 * @brief Return the number of communication or CRC errors.
 *
 * @return Number of errors since initialization.
 */
uint32_t wsen_hids_get_error_count();


#endif // FLIGHT_SOFTWARE_WSEN_HIDS_H