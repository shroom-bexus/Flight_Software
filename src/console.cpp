// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "console.h"

#include <cstdarg>
#include <cstdio>

#include "config.h"

#if ENABLE_ETHERNET
#include "ethernet_link.h"
#endif


namespace
{

    const char* level_to_string(ConsoleLevel level)
    {
        switch (level)
        {
        case ConsoleLevel::INFO:
            return "INFO";

        case ConsoleLevel::WARN:
            return "WARN";

        case ConsoleLevel::ERROR:
            return "ERROR";

        default:
            return "INFO";
        }
    }


    void send_ethernet_message(
        const char* message,
        ConsoleLevel level
    )
    {
#if ENABLE_ETHERNET

        if (!ethernet_link_connected())
        {
            return;
        }

        char buffer[256];

        snprintf(
            buffer,
            sizeof(buffer),
            "LOG,%s,%s",
            level_to_string(level),
            message
        );

        ethernet_link_send_line(buffer);

#else

        (void)message;
        (void)level;

#endif
    }

} // namespace


void console_println(
    const char* message,
    ConsoleLevel level
)
{
#if ENABLE_SERIAL_CONSOLE

    Serial.println(message);

#endif

#if ENABLE_ETHERNET_CONSOLE

    send_ethernet_message(
        message,
        level
    );

#endif
}

void console_printf(
    ConsoleLevel level,
    const char* format,
    ...
)
{
    char message[192];

    va_list args;
    va_start(args, format);

    vsnprintf(
        message,
        sizeof(message),
        format,
        args
    );

    va_end(args);

    console_println(
        message,
        level
    );
}