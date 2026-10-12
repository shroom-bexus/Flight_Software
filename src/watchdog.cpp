// SHROOM Flight Software
#include "watchdog.h"

#include <Watchdog_t4.h>
#include <util/atomic.h>
#include "config.h"

namespace
{
WDT_T4<WDT1> watchdog;
bool watchdog_reset = false;
static_assert(WATCHDOG_TIMEOUT_SECONDS >= 1 && WATCHDOG_TIMEOUT_SECONDS <= 128,
              "WDT1 timeout must be between 1 and 128 seconds");
}

void watchdog_init()
{
    // Read the previous timeout status before configuring the peripheral.
    watchdog_reset = watchdog.expired();
    WDT_timings_t settings;
    settings.timeout = WATCHDOG_TIMEOUT_SECONDS;
    settings.callback = nullptr; // Hardware resets even if interrupts are blocked.
    settings.pin = 0;            // Do not commandeer any payload GPIO.
    watchdog.begin(settings);
    watchdog_feed();
}

void watchdog_feed()
{
    // The two service writes must not be separated by an interrupt. Preserve
    // the caller's interrupt state instead of unconditionally enabling IRQs.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        watchdog.feed();
    }
}

void watchdog_print_status()
{
    Serial.print("Watchdog: WDT1, timeout ");
    Serial.print(WATCHDOG_TIMEOUT_SECONDS);
    Serial.println(" s");
    Serial.print("Previous watchdog timeout: ");
    Serial.println(watchdog_reset ? "YES" : "NO");
}
