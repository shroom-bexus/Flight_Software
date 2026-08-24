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

//#include <Arduino.h>


/**
 * @brief Initializes the WSEN-PADS pressure sensor.
 *
 * Initializes the I2C interface, verifies the device ID and configures
 * the sensor for continuous pressure and temperature measurements.
 *
 * @return true if initialization was successful.
 * @return false if the sensor could not be initialized.
 */
bool wsen_pads_init();


/**
 * @brief Reads a new pressure and temperature measurement.
 *
 * Checks the data-ready flags and reads the pressure and temperature
 * registers if new data is available. The measured values are stored
 * internally.
 *
 * @return true if a new measurement was read successfully.
 * @return false if no new data is available or communication failed.
 */
bool wsen_pads_update();


/**
 * @brief Returns the last successfully measured pressure.
 *
 * This function does not perform a new I2C transaction.
 *
 * @return Pressure in Pa.
 */
float wsen_pads_get_pressure();


/**
 * @brief Returns the last successfully measured temperature.
 *
 * This function does not perform a new I2C transaction.
 *
 * @return Temperature in degrees Kelvin.
 */
float wsen_pads_get_temperature();


/**
 * @brief Checks whether at least one valid measurement is available.
 *
 * @return true if valid sensor data has been received.
 * @return false if no valid measurement has been received yet.
 */
bool wsen_pads_data_valid();


#endif //FLIGHT_SOFTWARE_WSEN_PADS_H
