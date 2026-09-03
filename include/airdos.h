// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_AIRDOS_H
#define FLIGHT_SOFTWARE_AIRDOS_H

#include <Arduino.h>


/**
 * @brief Initialize all configured AIRDOS UART interfaces.
 */
void airdos_init();


/**
 * @brief Process incoming UART data for one configured AIRDOS channel.
 *
 * Call continuously from loop().
 *
 * @param channel_index Zero-based configured AIRDOS channel index.
 * @return true if a complete new UART line was received for this channel.
 */
bool airdos_update(uint8_t channel_index);


/**
 * @brief Return the physical AIRDOS sensor number for one configured channel.
 */
uint8_t airdos_get_sensor_id(uint8_t channel_index);


/**
 * @brief Return the most recently received AIRDOS message for one channel.
 *
 * @return Null-terminated UART message without CR/LF, or an empty string for
 *         an invalid channel index.
 */
const char* airdos_get_data(uint8_t channel_index);


/**
 * @brief Return the number of discarded messages caused by line overflow.
 */
uint32_t airdos_get_overflow_count(uint8_t channel_index);


/**
 * @brief Check whether at least one complete AIRDOS message was received.
 */
bool airdos_has_received_data(uint8_t channel_index);


/**
 * @brief Return the time of the most recently received complete message.
 *
 * @return millis() timestamp of the last message, or 0 for an invalid channel.
 */
uint32_t airdos_get_last_message_ms(uint8_t channel_index);


#endif // FLIGHT_SOFTWARE_AIRDOS_H
