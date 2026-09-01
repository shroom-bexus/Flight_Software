// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "airdos.h"

#include "config.h"


namespace
{

// ============================================================================
// UART buffer
// ============================================================================

constexpr size_t AIRDOS_LINE_BUFFER_SIZE = 256;

char line_buffer[AIRDOS_LINE_BUFFER_SIZE];

size_t line_length = 0;
bool line_overflow = false;

uint32_t overflow_count = 0;

bool message_received = false;
uint32_t last_message_ms = 0;


// Additional UART receive buffer.
//
// This is useful because SD card operations can temporarily block the CPU.
// Incoming AIRDOS data can continue accumulating here during that time.
uint8_t uart_rx_buffer[1024];

} // namespace


// ============================================================================
// Initialization
// ============================================================================

void airdos_init()
{
    line_length = 0;
    line_overflow = false;
    overflow_count = 0;

    message_received = false;
    last_message_ms = 0;

    AIRDOS_1_SERIAL.addMemoryForRead(
        uart_rx_buffer,
        sizeof(uart_rx_buffer)
    );

    AIRDOS_1_SERIAL.begin(AIRDOS_BAUD_RATE);
}


// ============================================================================
// UART processing
// ============================================================================

bool airdos_update()
{
    while (AIRDOS_1_SERIAL.available())
    {
        const char c =
            static_cast<char>(AIRDOS_1_SERIAL.read());


        // Ignore carriage return.
        if (c == '\r')
        {
            continue;
        }


        // Newline marks the end of one AIRDOS message.
        if (c == '\n')
        {
            if (line_overflow)
            {
                line_length = 0;
                line_overflow = false;

                return false;
            }

            if (line_length == 0)
            {
                continue;
            }

            line_buffer[line_length] = '\0';
            line_length = 0;

            // A complete AIRDOS message was received.
            message_received = true;
            last_message_ms = millis();

            return true;
        }


        // Leave one byte available for the null terminator.
        if (line_length < AIRDOS_LINE_BUFFER_SIZE - 1)
        {
            line_buffer[line_length++] = c;
        }
        else
        {
            line_overflow = true;
            ++overflow_count;
        }
    }

    return false;
}


// ============================================================================
// Getter functions
// ============================================================================

const char* airdos_get_data()
{
    return line_buffer;
}


uint32_t airdos_get_overflow_count()
{
    return overflow_count;
}

