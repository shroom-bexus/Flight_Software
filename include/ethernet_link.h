// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_ETHERNET_LINK_H
#define FLIGHT_SOFTWARE_ETHERNET_LINK_H

#include <Arduino.h>


/**
 * @brief Initialize the Ethernet interface and TCP server.
 *
 * @return true if the Ethernet stack was initialized.
 */
bool ethernet_link_init();


/**
 * @brief Handle Ethernet connections and incoming data.
 *
 * Call continuously from loop().
 */
void ethernet_link_update();


/**
 * @brief Check whether a ground station TCP client is connected.
 */
bool ethernet_link_connected();


/**
 * @brief Read one complete newline-terminated message.
 *
 * @param buffer Destination buffer.
 * @param buffer_size Size of destination buffer.
 *
 * @return true if a complete message was available.
 */
bool ethernet_link_read_line(
    char* buffer,
    size_t buffer_size
);


/**
 * @brief Send one newline-terminated message to the ground station.
 *
 * @return true if the message was sent to a connected client.
 */
bool ethernet_link_send_line(
    const char* message
);


/**
 * @brief Send an important message using reserved downlink capacity.
 *
 * ACK, NACK, and WARN messages use this function so ordinary telemetry
 * cannot delay command responses.
 */
bool ethernet_link_send_priority_line(
    const char* message
);


/**
 * @brief Set the estimated maximum wire downlink rate in kbit/s.
 *
 * A value of 0 disables rate limiting.
 *
 * @return true if the requested limit is valid and was applied.
 */
bool ethernet_link_set_downlink_limit(float limit_kbit_s);


/** @brief Return the active estimated wire downlink limit in kbit/s. */
float ethernet_link_get_downlink_limit();


#endif // FLIGHT_SOFTWARE_ETHERNET_LINK_H
