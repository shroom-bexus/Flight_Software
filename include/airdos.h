// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_AIRDOS_H
#define FLIGHT_SOFTWARE_AIRDOS_H

#include <Arduino.h>


/**
 * @brief Initialize the AIRDOS UART interface.
 */
void airdos_init();


/**
 * @brief Process incoming AIRDOS UART data.
 *
 * Call continuously from loop().
 *
 * @return true if a complete new UART line was received.
 */
bool airdos_update();


/**
 * @brief Return the most recently received AIRDOS message.
 *
 * @return Null-terminated UART message without CR/LF.
 */
const char* airdos_get_data();


/**
 * @brief Return the number of discarded messages caused by buffer overflow.
 */
uint32_t airdos_get_overflow_count();

/**
 * @brief Check whether at least one complete AIRDOS message was received.
 */
bool airdos_has_received_data();


/**
 * @brief Return the time of the most recently received complete message.
 *
 * @return millis() timestamp of the last message.
 */
uint32_t airdos_get_last_message_ms();


#endif // FLIGHT_SOFTWARE_AIRDOS_H