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
 * @brief Return the latest pressure measurement.
 *
 * No new sensor measurement is performed.
 *
 * @return Pressure in Pascal.
 */
float wsen_pads_get_pressure();


/**
 * @brief Return the latest temperature measurement.
 *
 * No new sensor measurement is performed.
 *
 * @return Temperature in Kelvin.
 */
float wsen_pads_get_temperature();


/**
 * @brief Check whether at least one valid measurement is available.
 *
 * @return true if valid sensor data has been received.
 */
bool wsen_pads_data_valid();


#endif // FLIGHT_SOFTWARE_WSEN_PADS_H