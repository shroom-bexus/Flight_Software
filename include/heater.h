// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_HEATER_H
#define FLIGHT_SOFTWARE_HEATER_H

#include <Arduino.h>


// Heater identification

enum class Heater : uint8_t
{
    HEATER_1 = 0,
    HEATER_2,
    HEATER_3,
    HEATER_4
};


// Public interface

/**
 * @brief Initialize all heater outputs.
 *
 * All heater channels are forced OFF during initialization.
 */
void heater_init();


/**
 * @brief Set the power of one heater.
 *
 * The requested power is limited to both 0...100 % and the individual
 * maximum configured for the selected heater.
 *
 * @param heater Heater channel.
 * @param power_percent Requested heater power in percent.
 */
void heater_set_power(
    Heater heater,
    float power_percent
);


/**
 * @brief Return the currently commanded heater power.
 *
 * @return Heater power in percent.
 */
float heater_get_power(Heater heater);


/**
 * @brief Switch one heater completely off.
 */
void heater_off(Heater heater);


/**
 * @brief Switch all heaters completely off.
 */
void heater_all_off();


/**
 * @brief Check whether a heater channel is enabled in config.h.
 */
bool heater_is_enabled(Heater heater);


/**
 * @brief Set the same power for all enabled heater channels.
 *
 * @param power_percent Requested heater power in percent.
 */
void heater_set_all_power(float power_percent);


#endif // FLIGHT_SOFTWARE_HEATER_H