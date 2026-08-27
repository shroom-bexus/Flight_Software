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
 * @return true if a valid measurement was received.
 */
bool wsen_hids_update();


/**
 * @brief Return the last valid temperature measurement.
 *
 * @return Temperature in Kelvin.
 */
float wsen_hids_get_temperature();


/**
 * @brief Return the last valid relative humidity measurement.
 *
 * @return Relative humidity in percent.
 */
float wsen_hids_get_humidity();


/**
 * @brief Check whether the most recent update was successful.
 */
bool wsen_hids_data_valid();


/**
 * @brief Return the number of communication or CRC errors.
 *
 * @return Number of errors since initialization.
 */
uint32_t wsen_hids_get_error_count();


#endif // FLIGHT_SOFTWARE_WSEN_HIDS_H