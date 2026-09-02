// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_COMMANDS_H
#define FLIGHT_SOFTWARE_COMMANDS_H


/**
 * @brief Process one command received from the ground station.
 *
 * Command format:
 *
 * CMD,<command>[,<arguments>]
 */
void commands_handle(const char* message);


#endif // FLIGHT_SOFTWARE_COMMANDS_H