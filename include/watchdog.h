// SHROOM Flight Software
#ifndef FLIGHT_SOFTWARE_WATCHDOG_H
#define FLIGHT_SOFTWARE_WATCHDOG_H

// Arm WDT1; call once near the beginning of setup().
void watchdog_init();
// Call only from foreground progress checkpoints, never an ISR or yield().
void watchdog_feed();
// Print the latched boot cause and configured timeout to USB serial.
void watchdog_print_status();

#endif
