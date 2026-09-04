// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_WSEN_ISDS_H
#define FLIGHT_SOFTWARE_WSEN_ISDS_H

#include <cstdint>

/** @brief Initialize the WSEN-ISDS inertial measurement unit. */
bool wsen_isds_init();

/**
 * @brief Read one new acceleration/gyroscope sample if available.
 *
 * @return true if a complete new 6-axis sample was received.
 */
bool wsen_isds_update();

/**
 * @brief Check whether the latest valid sample exceeds an event threshold.
 *
 * Acceleration uses the deviation of vector magnitude from 1 g. Gyroscope
 * uses the angular-rate vector magnitude.
 */
bool wsen_isds_event_detected();

float wsen_isds_get_accel_x();
float wsen_isds_get_accel_y();
float wsen_isds_get_accel_z();

float wsen_isds_get_gyro_x();
float wsen_isds_get_gyro_y();
float wsen_isds_get_gyro_z();

/** @brief Check whether the most recent sample was valid. */
bool wsen_isds_data_valid();

/** @brief Check whether the sensor is currently initialized. */
bool wsen_isds_is_initialized();

/** @brief Return the accumulated number of communication/configuration errors. */
uint32_t wsen_isds_get_error_count();

#endif // FLIGHT_SOFTWARE_WSEN_ISDS_H
