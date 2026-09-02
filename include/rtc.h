// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_RTC_H
#define FLIGHT_SOFTWARE_RTC_H

#include <Arduino.h>


/**
 * @brief Initialize TimeLib using the Teensy hardware RTC.
 *
 * The hardware RTC is expected to contain UTC time.
 *
 * @return true if TimeLib successfully synchronized with the RTC.
 */
bool rtc_init();


/**
 * @brief Write the current UTC timestamp into a buffer.
 *
 * Format:
 * YYYY-MM-DDTHH:MM:SSZ
 *
 * Example:
 * 2026-08-26T21:42:15Z
 *
 * @param buffer Destination buffer.
 * @param buffer_size Size of the destination buffer.
 */
void rtc_get_timestamp(
    char* buffer,
    size_t buffer_size
);


#endif // FLIGHT_SOFTWARE_RTC_H