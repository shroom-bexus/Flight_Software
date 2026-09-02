// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "heater.h"

#include "config.h"


namespace
{
    // ============================================================================
    // Channel configuration
    // ============================================================================

    constexpr bool channel_enabled[] =
    {
        HEATER_1_ENABLED,
        HEATER_2_ENABLED,
        HEATER_3_ENABLED,
        HEATER_4_ENABLED
    };


    constexpr uint8_t heater_pins[] =
    {
        HEATER_PIN_1,
        HEATER_PIN_2,
        HEATER_PIN_3,
        HEATER_PIN_4
    };


    constexpr float max_power_percent[] =
    {
        HEATER_1_MAX_POWER_PERCENT,
        HEATER_2_MAX_POWER_PERCENT,
        HEATER_3_MAX_POWER_PERCENT,
        HEATER_4_MAX_POWER_PERCENT
    };


    static_assert(
        sizeof(channel_enabled) / sizeof(channel_enabled[0]) ==
        HEATER_CHANNEL_COUNT
    );

    static_assert(
        sizeof(heater_pins) / sizeof(heater_pins[0]) ==
        HEATER_CHANNEL_COUNT
    );

    static_assert(
        sizeof(max_power_percent) / sizeof(max_power_percent[0]) ==
        HEATER_CHANNEL_COUNT
    );


    // ============================================================================
    // Runtime state
    // ============================================================================

    float current_power_percent[HEATER_CHANNEL_COUNT] = {};


    // ============================================================================
    // Helper functions
    // ============================================================================

    bool index_valid(uint8_t index)
    {
        return index < HEATER_CHANNEL_COUNT;
    }


    void write_power(
        uint8_t index,
        float power_percent
    )
    {
        // Arduino analogWrite uses the default 8-bit range 0...255.
        const uint8_t pwm_value =
            static_cast<uint8_t>(
                power_percent * 255.0f / 100.0f + 0.5f
            );

        analogWrite(
            heater_pins[index],
            pwm_value
        );

        current_power_percent[index] = power_percent;
    }
} // namespace


// ============================================================================
// Initialization
// ============================================================================

void heater_init()
{
    for (uint8_t i = 0; i < HEATER_CHANNEL_COUNT; ++i)
    {
        current_power_percent[i] = 0.0f;

        // Configure the PWM frequency for every heater output.
        analogWriteFrequency(
            heater_pins[i],
            HEATER_PWM_FREQUENCY_HZ
        );

        // Heater outputs must always start in the safe OFF state.
        pinMode(
            heater_pins[i],
            OUTPUT
        );

        digitalWrite(
            heater_pins[i],
            LOW
        );
    }
}


// ============================================================================
// Heater control
// ============================================================================

void heater_set_power(
    Heater heater,
    float power_percent
)
{
    const uint8_t index =
        static_cast<uint8_t>(heater);

    if (!index_valid(index) ||
        !channel_enabled[index])
    {
        return;
    }


    power_percent = constrain(
        power_percent,
        0.0f,
        max_power_percent[index]
    );

    write_power(
        index,
        power_percent
    );
}


void heater_off(Heater heater)
{
    heater_set_power(
        heater,
        0.0f
    );
}


void heater_all_off()
{
    for (uint8_t i = 0; i < HEATER_CHANNEL_COUNT; ++i)
    {
        digitalWrite(
            heater_pins[i],
            LOW
        );

        current_power_percent[i] = 0.0f;
    }
}

void heater_set_all_power(float power_percent)
{
    for (uint8_t i = 0; i < HEATER_CHANNEL_COUNT; ++i)
    {
        heater_set_power(
            static_cast<Heater>(i),
            power_percent
        );
    }
}


// ============================================================================
// Getter functions
// ============================================================================

float heater_get_power(Heater heater)
{
    const uint8_t index =
        static_cast<uint8_t>(heater);

    if (!index_valid(index))
    {
        return 0.0f;
    }

    return current_power_percent[index];
}


bool heater_is_enabled(Heater heater)
{
    const uint8_t index =
        static_cast<uint8_t>(heater);

    return index_valid(index) &&
        channel_enabled[index];
}