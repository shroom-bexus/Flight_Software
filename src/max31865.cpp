// SHROOM Flight Software

#include "max31865.h"

#include <Adafruit_MAX31865.h>

#include "config.h"


namespace
{
    // One driver instance is assigned to every configured chip-select pin.
    Adafruit_MAX31865 max_sensors[] =
    {
        Adafruit_MAX31865(MAX31865_CS_PINS[0], &MAX31865_SPI_BUS),
        Adafruit_MAX31865(MAX31865_CS_PINS[1], &MAX31865_SPI_BUS),
        Adafruit_MAX31865(MAX31865_CS_PINS[2], &MAX31865_SPI_BUS),
        Adafruit_MAX31865(MAX31865_CS_PINS[3], &MAX31865_SPI_BUS),
        Adafruit_MAX31865(MAX31865_CS_PINS[4], &MAX31865_SPI_BUS),
        Adafruit_MAX31865(MAX31865_CS_PINS[5], &MAX31865_SPI_BUS),
        Adafruit_MAX31865(MAX31865_CS_PINS[6], &MAX31865_SPI_BUS),
        Adafruit_MAX31865(MAX31865_CS_PINS[7], &MAX31865_SPI_BUS),
        Adafruit_MAX31865(MAX31865_CS_PINS[8], &MAX31865_SPI_BUS)
    };


    // Compile-time checks prevent configuration arrays from becoming inconsistent.
    static_assert(
        sizeof(max_sensors) / sizeof(max_sensors[0]) ==
        MAX31865_CHANNEL_COUNT
    );


    // Runtime state

    struct SensorState
    {
        // Keep the last valid value while validity describes the latest cycle.
        float temperature_k = NAN;

        uint32_t error_count = 0;
        uint8_t fault = 0;

        // Prevent one persistent hardware fault from incrementing the counter
        // on every 1 s measurement cycle.
        bool fault_active = false;

        bool initialized = false;
        bool valid = false;
    };


    SensorState sensor_state[MAX31865_CHANNEL_COUNT];


    // Helper functions

    bool index_valid(uint8_t index)
    {
        return index < MAX31865_CHANNEL_COUNT;
    }
} // namespace


// Initialization

bool max31865_init()
{
    bool success = true;

    for (uint8_t i = 0; i < MAX31865_CHANNEL_COUNT; ++i)
    {
        sensor_state[i] = {};

        if (!MAX31865_ENABLED[i])
        {
            continue;
        }

        sensor_state[i].initialized =
            max_sensors[i].begin(MAX31865_2WIRE);

        if (!sensor_state[i].initialized)
        {
            if (!sensor_state[i].fault_active)
            {
                ++sensor_state[i].error_count;
                sensor_state[i].fault_active = true;
            }

            success = false;
        }
    }

    return success;
}


// Measurement

bool max31865_update()
{
    bool success = true;

    for (uint8_t i = 0; i < MAX31865_CHANNEL_COUNT; ++i)
    {
        if (!MAX31865_ENABLED[i])
        {
            continue;
        }

        SensorState& state = sensor_state[i];

        // Validity always refers to the current measurement cycle.
        state.valid = false;
        state.fault = 0;


        // Retry initialization if this channel was unavailable earlier.
        if (!state.initialized)
        {
            state.initialized =
                max_sensors[i].begin(MAX31865_2WIRE);

            if (!state.initialized)
            {
                if (!state.fault_active)
                {
                    ++state.error_count;
                    state.fault_active = true;
                }

                success = false;
                continue;
            }
        }


        // Read with the channel-specific measured reference resistance.
        const float measured_c =
            max_sensors[i].temperature(
                MAX31865_RNOMINAL,
                MAX31865_RREF[i]
            );


        state.fault = max_sensors[i].readFault();

        if (state.fault != 0)
        {
            if (!state.fault_active)
            {
                ++state.error_count;
                state.fault_active = true;
            }

            success = false;

            max_sensors[i].clearFault();

            continue;
        }


        // Apply the individual linear calibration before converting to Kelvin.
        const float calibrated_c =
            measured_c * MAX31865_SCALE[i] +
            MAX31865_OFFSET_C[i];

        state.temperature_k =
            calibrated_c + 273.15f;

        state.valid = true;

        // A valid measurement ends the current fault episode.
        state.fault_active = false;
    }

    return success;
}


// Getter functions

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
        MAX31865_ENABLED[index];
}


bool max31865_data_valid(TempSensor sensor)
{
    const uint8_t index =
        static_cast<uint8_t>(sensor);

    return index_valid(index) &&
        MAX31865_ENABLED[index] &&
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

bool max31865_is_initialized(TempSensor sensor)
{
    const uint8_t index =
        static_cast<uint8_t>(sensor);

    return index_valid(index) &&
        MAX31865_ENABLED[index] &&
        sensor_state[index].initialized;
}
