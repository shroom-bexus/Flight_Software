// SHROOM Flight Software
// Ground-station telemetry formatting

#ifndef FLIGHT_SOFTWARE_TELEMETRY_H
#define FLIGHT_SOFTWARE_TELEMETRY_H

#include <stdint.h>

/** @brief Send the current thermal-controller and heater state. */
void telemetry_send_thermal();

/** @brief Send one valid MAX31865 temperature measurement. */
void telemetry_send_max31865(uint8_t sensor_id, float temperature_k);

/** @brief Send the latest WSEN-PADS measurement. */
void telemetry_send_pads();

/** @brief Send the latest WSEN-HIDS measurement. */
void telemetry_send_hids();

/** @brief Send one complete raw AIRDOS UART message. */
void telemetry_send_airdos(uint8_t sensor_id, const char* data);

/** @brief Periodically send health information for all enabled modules. */
void telemetry_update();

#endif // FLIGHT_SOFTWARE_TELEMETRY_H
