
// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "rtc.h"

#include <TimeLib.h>


// TimeLib expects a function that returns the current Unix time.
// The Teensy hardware RTC provides this through Teensy3Clock.
static time_t getTeensyTime()
{
    return Teensy3Clock.get();
}


bool rtc_init()
{
    // Synchronize TimeLib with the Teensy's hardware RTC.
    // TimeLib will automatically resynchronize periodically.
    setSyncProvider(getTeensyTime);

    if (timeStatus() != timeSet)
    {
        Serial.println("RTC initialization failed");
        return false;
    }

    Serial.println("RTC initialized");
    return true;
}


void rtc_get_timestamp(char* buffer, size_t bufferSize)
{
    // Read the time only once.
    // This prevents inconsistent values if the second changes
    // while the timestamp is being generated.
    const time_t currentTime = now();

    snprintf(
        buffer,
        bufferSize,
        "%04d-%02d-%02dT%02d:%02d:%02dZ",
        year(currentTime),
        month(currentTime),
        day(currentTime),
        hour(currentTime),
        minute(currentTime),
        second(currentTime)
    );
}