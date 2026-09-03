// SHROOM Flight Software

#include "airdos.h"

#include <HardwareSerial.h>

#include "config.h"


namespace
{

// UART buffers

constexpr size_t AIRDOS_LINE_BUFFER_SIZE = 256;
constexpr size_t AIRDOS_UART_RX_BUFFER_SIZE = 1024;


#if FLIGHT_PRIMARY
HardwareSerialIMXRT* const airdos_serials[AIRDOS_CHANNEL_COUNT] =
{
    &AIRDOS_8_SERIAL,
    &AIRDOS_9_SERIAL
};
#else
HardwareSerialIMXRT* const airdos_serials[AIRDOS_CHANNEL_COUNT] =
{
    &AIRDOS_LEGACY_SERIAL
};
#endif

static_assert(
    sizeof(airdos_serials) / sizeof(airdos_serials[0]) ==
    AIRDOS_CHANNEL_COUNT
);

static_assert(
    sizeof(AIRDOS_SENSOR_IDS) / sizeof(AIRDOS_SENSOR_IDS[0]) ==
    AIRDOS_CHANNEL_COUNT
);


struct AirdosState
{
    char line_buffer[AIRDOS_LINE_BUFFER_SIZE] = {};
    size_t line_length = 0;
    bool line_overflow = false;

    uint32_t overflow_count = 0;

    bool message_received = false;
    uint32_t last_message_ms = 0;

    // Extra UART receive memory allows data to accumulate while SD writes or
    // other work temporarily keep the main loop busy.
    uint8_t uart_rx_buffer[AIRDOS_UART_RX_BUFFER_SIZE] = {};
};


AirdosState airdos_state[AIRDOS_CHANNEL_COUNT];


bool channel_valid(uint8_t channel_index)
{
    return channel_index < AIRDOS_CHANNEL_COUNT;
}

} // namespace


// Initialization

void airdos_init()
{
    for (uint8_t i = 0; i < AIRDOS_CHANNEL_COUNT; ++i)
    {
        AirdosState& state = airdos_state[i];

        state.line_buffer[0] = '\0';
        state.line_length = 0;
        state.line_overflow = false;
        state.overflow_count = 0;
        state.message_received = false;
        state.last_message_ms = 0;

        airdos_serials[i]->addMemoryForRead(
            state.uart_rx_buffer,
            sizeof(state.uart_rx_buffer)
        );

        airdos_serials[i]->begin(AIRDOS_BAUD_RATE);
    }
}


// UART processing

bool airdos_update(uint8_t channel_index)
{
    if (!channel_valid(channel_index)) return false;

    AirdosState& state = airdos_state[channel_index];
    HardwareSerialIMXRT& serial = *airdos_serials[channel_index];

    while (serial.available())
    {
        const char c = static_cast<char>(serial.read());

        // Ignore carriage return.
        if (c == '\r')
        {
            continue;
        }

        // Newline marks the end of one AIRDOS message.
        if (c == '\n')
        {
            if (state.line_overflow)
            {
                state.line_length = 0;
                state.line_overflow = false;
                state.line_buffer[0] = '\0';

                return false;
            }

            if (state.line_length == 0)
            {
                continue;
            }

            state.line_buffer[state.line_length] = '\0';
            state.line_length = 0;

            state.message_received = true;
            state.last_message_ms = millis();

            return true;
        }

        // Leave one byte available for the null terminator.
        if (state.line_length < AIRDOS_LINE_BUFFER_SIZE - 1)
        {
            state.line_buffer[state.line_length++] = c;
        }
        else
        {
            // Count the discarded line once, not every additional byte.
            if (!state.line_overflow)
            {
                ++state.overflow_count;
            }
            state.line_overflow = true;
        }
    }

    return false;
}


// Getter functions

uint8_t airdos_get_sensor_id(uint8_t channel_index)
{
    if (!channel_valid(channel_index)) return 0;
    return AIRDOS_SENSOR_IDS[channel_index];
}


const char* airdos_get_data(uint8_t channel_index)
{
    if (!channel_valid(channel_index)) return "";
    return airdos_state[channel_index].line_buffer;
}


uint32_t airdos_get_overflow_count(uint8_t channel_index)
{
    if (!channel_valid(channel_index)) return 0;
    return airdos_state[channel_index].overflow_count;
}


bool airdos_has_received_data(uint8_t channel_index)
{
    return channel_valid(channel_index) &&
        airdos_state[channel_index].message_received;
}


uint32_t airdos_get_last_message_ms(uint8_t channel_index)
{
    if (!channel_valid(channel_index)) return 0;
    return airdos_state[channel_index].last_message_ms;
}
