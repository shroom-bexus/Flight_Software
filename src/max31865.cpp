// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "max31865.h"

#include <Adafruit_MAX31865.h>

#include "config.h"


namespace
{

// ============================================================================
// Channel configuration
// ============================================================================

constexpr bool channel_enabled[] =
{
    MAX31865_TEMP_1_ENABLED,
    MAX31865_TEMP_2_ENABLED,
    MAX31865_TEMP_3_ENABLED,
    MAX31865_TEMP_4_ENABLED,
    MAX31865_TEMP_5_ENABLED,
    MAX31865_TEMP_6_ENABLED,
    MAX31865_TEMP_7_ENABLED,
    MAX31865_TEMP_8_ENABLED,
    MAX31865_TEMP_9_ENABLED
};


Adafruit_MAX31865 max_sensors[] =
{
    Adafruit_MAX31865(MAX31865_CS_1, &MAX31865_SPI_BUS),
    Adafruit_MAX31865(MAX31865_CS_2, &MAX31865_SPI_BUS),
    Adafruit_MAX31865(MAX31865_CS_3, &MAX31865_SPI_BUS),
    Adafruit_MAX31865(MAX31865_CS_4, &MAX31865_SPI_BUS),
    Adafruit_MAX31865(MAX31865_CS_5, &MAX31865_SPI_BUS),
    Adafruit_MAX31865(MAX31865_CS_6, &MAX31865_SPI_BUS),
    Adafruit_MAX31865(MAX31865_CS_7, &MAX31865_SPI_BUS),
    Adafruit_MAX31865(MAX31865_CS_8, &MAX31865_SPI_BUS),
    Adafruit_MAX31865(MAX31865_CS_9, &MAX31865_SPI_BUS)
};


constexpr float reference_resistors[] =
{
    MAX31865_RREF_1,
    MAX31865_RREF_2,
    MAX31865_RREF_3,
    MAX31865_RREF_4,
    MAX31865_RREF_5,
    MAX31865_RREF_6,
    MAX31865_RREF_7,
    MAX31865_RREF_8,
    MAX31865_RREF_9
};


constexpr float calibration_scale[] =
{
    MAX31865_SCALE_1,
    MAX31865_SCALE_2,
    MAX31865_SCALE_3,
    MAX31865_SCALE_4,
    MAX31865_SCALE_5,
    MAX31865_SCALE_6,
    MAX31865_SCALE_7,
    MAX31865_SCALE_8,
    MAX31865_SCALE_9
};


constexpr float calibration_offset_c[] =
{
    MAX31865_OFFSET_C_1,
    MAX31865_OFFSET_C_2,
    MAX31865_OFFSET_C_3,
    MAX31865_OFFSET_C_4,
    MAX31865_OFFSET_C_5,
    MAX31865_OFFSET_C_6,
    MAX31865_OFFSET_C_7,
    MAX31865_OFFSET_C_8,
    MAX31865_OFFSET_C_9
};


// Compile-time checks prevent configuration arrays from becoming inconsistent.
static_assert(
    sizeof(channel_enabled) / sizeof(channel_enabled[0]) ==
    MAX31865_CHANNEL_COUNT
);

static_assert(
    sizeof(max_sensors) / sizeof(max_sensors[0]) ==
    MAX31865_CHANNEL_COUNT
);

static_assert(
    sizeof(reference_resistors) / sizeof(reference_resistors[0]) ==
    MAX31865_CHANNEL_COUNT
);

static_assert(
    sizeof(calibration_scale) / sizeof(calibration_scale[0]) ==
    MAX31865_CHANNEL_COUNT
);

static_assert(
    sizeof(calibration_offset_c) / sizeof(calibration_offset_c[0]) ==
    MAX31865_CHANNEL_COUNT
);


// ============================================================================
// Runtime state
// ============================================================================

struct SensorState
{
    float temperature_k = NAN;

    uint32_t error_count = 0;
    uint8_t fault = 0;

    bool valid = false;
};


SensorState sensor_state[MAX31865_CHANNEL_COUNT];


// ============================================================================
// Helper functions
// ============================================================================

bool index_valid(uint8_t index)
{
    return index < MAX31865_CHANNEL_COUNT;
}

} // namespace


// ============================================================================
// Initialization
// ============================================================================

bool max31865_init()
{
    bool success = true;

    for (uint8_t i = 0; i < MAX31865_CHANNEL_COUNT; ++i)
    {
        sensor_state[i] = {};

        if (!channel_enabled[i])
        {
            continue;
        }

        if (!max_sensors[i].begin(MAX31865_2WIRE))
        {
            ++sensor_state[i].error_count;
            success = false;
        }
    }

    return success;
}


// ============================================================================
// Measurement
// ============================================================================

bool max31865_update()
{
    bool success = true;

    for (uint8_t i = 0; i < MAX31865_CHANNEL_COUNT; ++i)
    {
        if (!channel_enabled[i])
        {
            continue;
        }

        SensorState &state = sensor_state[i];

        // valid always describes the current measurement cycle.
        state.valid = false;
        state.fault = 0;


        // Adafruit_MAX31865 returns temperature in degrees Celsius.
        const float measured_c =
            max_sensors[i].temperature(
                MAX31865_RNOMINAL,
                reference_resistors[i]
            );


        // Read the hardware fault register after the measurement.
        state.fault = max_sensors[i].readFault();

        if (state.fault != 0)
        {
            ++state.error_count;
            success = false;

            // Clear the latched fault before the next measurement.
            max_sensors[i].clearFault();

            continue;
        }


        // Apply the individual linear calibration before converting to Kelvin.
        const float calibrated_c =
            measured_c * calibration_scale[i] +
            calibration_offset_c[i];


        // Only overwrite the stored value after a successful measurement.
        state.temperature_k =
            calibrated_c + 273.15f;

        state.valid = true;
    }

    return success;
}


// ============================================================================
// Getter functions
// ============================================================================

float max31865_get_temperature(TempSensor sensor)
{
    const uint8_t index =
        static_cast<uint8_t>(sensor);

    if (!index_valid(index))
    {
        return NAN;
    }

    return sensor_state[index].temperature_k;
}


bool max31865_is_enabled(TempSensor sensor)
{
    const uint8_t index =
        static_cast<uint8_t>(sensor);

    return index_valid(index) &&
           channel_enabled[index];
}


bool max31865_data_valid(TempSensor sensor)
{
    const uint8_t index =
        static_cast<uint8_t>(sensor);

    return index_valid(index) &&
           channel_enabled[index] &&
           sensor_state[index].valid;
}


uint8_t max31865_get_fault(TempSensor sensor)
{
    const uint8_t index =
        static_cast<uint8_t>(sensor);

    if (!index_valid(index))
    {
        return 0;
    }

    return sensor_state[index].fault;
}


uint32_t max31865_get_error_count(TempSensor sensor)
{
    const uint8_t index =
        static_cast<uint8_t>(sensor);

    if (!index_valid(index))
    {
        return 0;
    }

    return sensor_state[index].error_count;
}