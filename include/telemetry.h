// SHROOM Flight Software
// Ground-station telemetry formatting

#ifndef FLIGHT_SOFTWARE_TELEMETRY_H
#define FLIGHT_SOFTWARE_TELEMETRY_H

/** @brief Send the current thermal-controller and heater state. */
void telemetry_send_thermal();

/** @brief Send the latest WSEN-PADS measurement. */
void telemetry_send_pads();

/** @brief Send the latest WSEN-HIDS measurement. */
void telemetry_send_hids();

/** @brief Periodically send health information for all enabled modules. */
void telemetry_update();

#endif // FLIGHT_SOFTWARE_TELEMETRY_H
