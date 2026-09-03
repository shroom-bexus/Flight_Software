// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_ETHERNET_LINK_H
#define FLIGHT_SOFTWARE_ETHERNET_LINK_H

#include <Arduino.h>


/**
 * @brief Initialize the Ethernet interface and UDP endpoint.
 *
 * @return true if the Ethernet stack was initialized.
 */
bool ethernet_link_init();


/**
 * @brief Receive commands and periodically transmit queued telemetry.
 *
 * Call continuously from loop().
 */
void ethernet_link_update();


/**
 * @brief Check whether the ground station contacted us recently.
 */
bool ethernet_link_connected();


/**
 * @brief Read one complete command received in a UDP datagram.
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
 * @brief Queue one telemetry line for the next UDP batch.
 *
 * @return true if the line was added to the current batch.
 */
bool ethernet_link_send_line(
    const char* message
);


/**
 * @brief Send an important message immediately in one UDP datagram.
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
