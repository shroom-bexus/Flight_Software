// SHROOM Flight Software
// Thermal controller

#ifndef FLIGHT_SOFTWARE_THERMAL_CONTROL_H
#define FLIGHT_SOFTWARE_THERMAL_CONTROL_H


/**
 * @brief Initialize the thermal controller.
 *
 * Resets the PID state and switches all heaters off.
 */
void thermal_control_init();


/**
 * @brief Update the thermal PID controller.
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
 * @brief Store the current manual heater outputs.
 */
void thermal_control_save_heater_state();

#endif // FLIGHT_SOFTWARE_THERMAL_CONTROL_H
