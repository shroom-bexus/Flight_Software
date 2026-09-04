// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_LOGGER_H
#define FLIGHT_SOFTWARE_LOGGER_H

#include <Arduino.h>


/**
 * @brief Initialize both storage devices and open all enabled log files.
 *
 * @return true if at least one complete storage copy is available.
 */
bool logger_init();


/**
 * @brief Periodically flush pending data to the SD card.
 *
 * Call continuously from loop().
 */
void logger_update();


/**
 * @brief Log one MAX31865 temperature measurement.
 *
 * @param sensor_id Sensor identifier, starting at 1.
 * @param temperature_k Temperature in Kelvin.
 */
void logger_log_max31865(
    uint8_t sensor_id,
    float temperature_k
);


/**
 * @brief Log one WSEN-PADS measurement.
 *
 * @param temperature_k Temperature in Kelvin.
 * @param pressure_pa Pressure in Pascal.
 */
void logger_log_wsen_pads(
    float temperature_k,
    float pressure_pa
);


/**
 * @brief Log one WSEN-HIDS measurement.
 *
 * @param temperature_k Temperature in Kelvin.
 * @param humidity_percent Relative humidity in percent.
 */
void logger_log_wsen_hids(
    float temperature_k,
    float humidity_percent
);


/**
 * @brief Log one WSEN-ISDS measurement.
 */
void logger_log_wsen_isds(
    float accel_x,
    float accel_y,
    float accel_z,
    float gyro_x,
    float gyro_y,
    float gyro_z
);


/**
 * @brief Log one raw AIRDOS UART message.
 *
 * @param sensor_index AIRDOS sensor channel, starting at 0.
 * @param data Received UART message.
 */
void logger_log_airdos(
    uint8_t sensor_index,
    const char* data
);


/**
 * @brief Check whether all enabled storage devices are ready.
 */
bool logger_is_ready();


/** @brief Check the built-in Teensy SD card independently. */
bool logger_internal_sd_is_ready();


/** @brief Check the soldered XTSD backup independently. */
bool logger_backup_sd_is_ready();


/**
 * @brief Return the combined initialization and write error count.
 */
uint32_t logger_get_error_count();


/** @brief Return errors belonging to the built-in SD card. */
uint32_t logger_get_internal_sd_error_count();


/** @brief Return errors belonging to the XTSD backup. */
uint32_t logger_get_backup_sd_error_count();


/** @brief Return the latest XTSD/SdFat error code and error data. */
uint8_t logger_get_backup_sd_error_code();
uint8_t logger_get_backup_sd_error_data();

#endif // FLIGHT_SOFTWARE_LOGGER_H
