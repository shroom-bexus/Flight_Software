// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

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


#endif // FLIGHT_SOFTWARE_ETHERNET_LINK_H