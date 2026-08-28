// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology
// thermal_control.h

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
 * @brief Check whether the thermal controller currently has a valid state.
 *
 * @return true if the controller has processed a valid temperature measurement.
 */
bool thermal_control_is_active();


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

#endif // FLIGHT_SOFTWARE_THERMAL_CONTROL_H