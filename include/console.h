// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#ifndef FLIGHT_SOFTWARE_CONSOLE_H
#define FLIGHT_SOFTWARE_CONSOLE_H

#include <Arduino.h>


enum class ConsoleLevel
{
    INFO,
    WARN,
    ERROR
};


/**
 * @brief Print a message to the console.
 *
 * The message is sent to USB Serial and, if connected,
 * to the Ground Station via Ethernet.
 */
void console_println(
    const char* message,
    ConsoleLevel level = ConsoleLevel::INFO
);


/**
 * @brief Print a formatted message to the console.
 */
void console_printf(
    ConsoleLevel level,
    const char* format,
    ...
);


#endif // FLIGHT_SOFTWARE_CONSOLE_H