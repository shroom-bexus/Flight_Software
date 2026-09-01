// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#ifndef FLIGHT_SOFTWARE_WSEN_PADS_H
#define FLIGHT_SOFTWARE_WSEN_PADS_H
#include <cstdint>


/**
 * @brief Initialize the WSEN-PADS pressure sensor.
 *
 * The I2C bus must already be initialized before calling this function.
 *
 * @return true if the sensor was initialized successfully.
 */
bool wsen_pads_init();


/**
 * @brief Read a new pressure and temperature measurement.
 *
 * The new values are stored internally and can be accessed using the
 * getter functions.
 *
 * @return true if a complete new measurement was received.
 */
bool wsen_pads_update();


/**
 * @brief Return the last valid pressure measurement.
 *
 * @return Pressure in Pascal.
 */
float wsen_pads_get_pressure();


/**
 * @brief Return the last valid temperature measurement.
 *
 * @return Temperature in Kelvin.
 */
float wsen_pads_get_temperature();


/**
 * @brief Check whether the most recent update was successful.
 */
bool wsen_pads_data_valid();

/**
 * @brief Check whether the sensor initialized successfully.
 */
bool wsen_pads_is_initialized();


/**
 * @brief Return the accumulated number of communication errors.
 */
uint32_t wsen_pads_get_error_count();


#endif // FLIGHT_SOFTWARE_WSEN_PADS_H