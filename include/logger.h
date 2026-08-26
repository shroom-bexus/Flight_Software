
// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology//

#ifndef FLIGHT_SOFTWARE_LOGGER_H
#define FLIGHT_SOFTWARE_LOGGER_H

#include <Arduino.h>


/**
 * @brief Initialize the SD card and open all enabled log files.
 *
 * @return true if initialization was successful.
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
 * @param sensorIndex Sensor number, starting at 0.
 * @param temperature_K Temperature in Kelvin.
 */
void logger_log_max31865(
    uint8_t sensorIndex,
    float temperature_K
);


/**
 * @brief Log one WSEN-PADS measurement.
 *
 * @param temperature_K Temperature in Kelvin.
 * @param pressure_Pa Pressure in Pascal.
 */
void logger_log_wsen_pads(
    float temperature_K,
    float pressure_Pa
);


/**
 * @brief Log one WSEN-HIDS measurement.
 *
 * @param temperature_K Temperature in Kelvin.
 * @param humidity_percent Relative humidity in percent.
 */
void logger_log_wsen_hids(
    float temperature_K,
    float humidity_percent
);


/**
 * @brief Log one WSEN-ISDS measurement.
 */
void logger_log_wsen_isds(
    float accelX,
    float accelY,
    float accelZ,
    float gyroX,
    float gyroY,
    float gyroZ
);


/**
 * @brief Log one raw AIRDOS UART message.
 *
 * AIRDOS support can be connected once the AIRDOS driver is implemented.
 */
void logger_log_airdos(
    uint8_t sensorIndex,
    const char* data
);

#endif //FLIGHT_SOFTWARE_LOGGER_H
