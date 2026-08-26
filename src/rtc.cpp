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


namespace
{

    /**
     * @brief Provide the Teensy hardware RTC time to TimeLib.
     */
    time_t get_teensy_time()
    {
        return Teensy3Clock.get();
    }

} // namespace


// ============================================================================
// Initialization
// ============================================================================

bool rtc_init()
{
    // TimeLib uses this function to synchronize with the hardware RTC.
    setSyncProvider(get_teensy_time);

    return timeStatus() == timeSet;
}


// ============================================================================
// Timestamp
// ============================================================================

void rtc_get_timestamp(
    char* buffer,
    size_t buffer_size
)
{
    // Read the clock only once so all timestamp fields belong to the
    // same instant, even if the second changes during formatting.
    const time_t current_time = now();

    snprintf(
        buffer,
        buffer_size,
        "%04d-%02d-%02dT%02d:%02d:%02dZ",
        year(current_time),
        month(current_time),
        day(current_time),
        hour(current_time),
        minute(current_time),
        second(current_time)
    );
}