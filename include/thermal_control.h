// SHROOM Flight Software
// Thermal controller

#ifndef FLIGHT_SOFTWARE_THERMAL_CONTROL_H
#define FLIGHT_SOFTWARE_THERMAL_CONTROL_H

#include <stdint.h>

enum class ThermalMode : unsigned char { PID = 0, BANG_BANG = 1 };
enum class ThermalFusionMode : unsigned char
{
    MEAN = 0,
    MEDIAN = 1,
    MINIMUM = 2,
    MAXIMUM = 3
};

ThermalMode thermal_control_get_mode();
const char* thermal_control_get_mode_name();
bool thermal_control_set_mode(ThermalMode mode);

ThermalFusionMode thermal_control_get_fusion_mode();
const char* thermal_control_get_fusion_mode_name();
bool thermal_control_set_fusion_mode(ThermalFusionMode mode);

// Half-width in Kelvin: ON at target - h, OFF at target + h.
float thermal_control_get_hysteresis();
bool thermal_control_set_hysteresis(float hysteresis_k);
float thermal_control_get_bang_bang_power();
bool thermal_control_set_bang_bang_power(float power_percent);

/**
 * @brief Initialize the thermal controller.
 *
 * Resets the PID state and switches all heaters off.
 */
void thermal_control_init();


/**
 * @brief Update the selected thermal controller.
 *
 * Uses the latest temperature measurement and applies the calculated
 * power equally to all heater channels.
 *
 * Call once after a new MAX31865 measurement.
 */
void thermal_control_update();


/**
 * @brief Return the current commanded heater power.
 *
 * @return Heater output in percent.
 */
float thermal_control_get_output();


/**
 * @brief Return the configured target temperature.
 *
 * @return Target temperature in Kelvin.
 */
float thermal_control_get_target();


/**
 * @brief Return the current control temperature.
 *
 * @return Temperature in Kelvin, or NAN if no valid measurement is available.
 */
float thermal_control_get_temperature();

/**
 * @brief Return the configured proportional gain.
 */
float thermal_control_get_kp();

/**
 * @brief Return the configured integral gain.
 */
float thermal_control_get_ki();

/**
 * @brief Return the configured derivative gain.
 */
float thermal_control_get_kd();

/**
 * @brief Change the thermal control target.
 *
 * @param target_k New target temperature in Kelvin.
 *
 * @return true if the target was accepted.
 */
bool thermal_control_set_target(float target_k);

/**
 * @brief Change and store all PID gains.
 *
 * The new gains take effect immediately and remain available after a reset.
 *
 * @return true if all gains were accepted.
 */
bool thermal_control_set_pid(float kp, float ki, float kd);

/**
 * @brief Enable or disable the thermal controller.
 *
 * Disabling the controller immediately switches all heaters off.
 */
void thermal_control_set_enabled(bool enabled);


/**
 * @brief Check whether the thermal controller is enabled.
 */
bool thermal_control_is_enabled();

/**
 * @brief Store all current manual heater outputs.
 */
void thermal_control_save_heater_state();

/**
 * @brief Store one current manual heater output without overwriting the others.
 *
 * @param heater_index Zero-based heater index.
 */
void thermal_control_save_heater_power(uint8_t heater_index);

/** @brief Enable or disable the heating-plate temperature limiter. */
void thermal_control_set_plate_limit_enabled(bool enabled);

/** @brief Return whether the heating-plate temperature limiter is enabled. */
bool thermal_control_plate_limit_is_enabled();

/** @brief Change the persistent heating-plate temperature limit in Kelvin. */
bool thermal_control_set_plate_limit(float limit_k);

/** @brief Return the configured heating-plate temperature limit in Kelvin. */
float thermal_control_get_plate_limit();

/** @brief Return the current heating-plate sensor temperature or NAN. */
float thermal_control_get_plate_temperature();

/** @brief Return whether the plate limiter is currently inhibiting heating. */
bool thermal_control_plate_limit_tripped();

/** @brief Apply the plate limiter immediately to current physical outputs. */
void thermal_control_enforce_plate_limit();

#endif // FLIGHT_SOFTWARE_THERMAL_CONTROL_H

