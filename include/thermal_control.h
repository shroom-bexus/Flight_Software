// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

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


#endif // FLIGHT_SOFTWARE_THERMAL_CONTROL_H