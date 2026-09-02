// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "wsen_hids.h"

#include <Wire.h>
#include <cmath>

#include "config.h"


namespace
{
    // ============================================================================
    // Sensor configuration
    // ============================================================================

    // Command for a high-precision single-shot measurement.
    constexpr uint8_t HIDS_MEASURE_CMD = 0xFD;

    // Measurement response:
    // Temperature MSB + LSB + CRC
    // Humidity    MSB + LSB + CRC
    constexpr uint8_t HIDS_DATA_LENGTH = 6;


    // ============================================================================
    // Stored measurement data
    // ============================================================================

    float temperature_k = NAN;
    float humidity_percent = NAN;

    bool initialized = false;
    bool last_measurement_valid = false;
    uint32_t error_count = 0;


    // ============================================================================
    // CRC
    // ============================================================================

    /**
 * @brief Calculate the CRC used by the WSEN-HIDS measurement data.
 */
    uint8_t calculate_crc(const uint8_t* data, size_t length)
    {
        uint8_t crc = 0xFF;

        for (size_t i = 0; i < length; ++i)
        {
            crc ^= data[i];

            for (uint8_t bit = 0; bit < 8; ++bit)
            {
                if (crc & 0x80)
                {
                    crc = (crc << 1) ^ 0x31;
                }
                else
                {
                    crc <<= 1;
                }
            }
        }

        return crc;
    }
} // namespace


// ============================================================================
// Initialization
// ============================================================================

bool wsen_hids_init()
{
    temperature_k = NAN;
    humidity_percent = NAN;

    initialized = false;
    last_measurement_valid = false;
    error_count = 0;

    Wire.beginTransmission(WSEN_HIDS_I2C_ADDRESS);

    if (Wire.endTransmission() != 0)
    {
        ++error_count;
        return false;
    }

    initialized = true;

    return true;
}


// ============================================================================
// Measurement
// ============================================================================

bool wsen_hids_update()
{
    // The validity flag always describes the most recent update.
    // Previous valid values remain stored if this measurement fails.
    last_measurement_valid = false;

    // Start a high-precision single-shot measurement.
    Wire.beginTransmission(WSEN_HIDS_I2C_ADDRESS);
    Wire.write(HIDS_MEASURE_CMD);

    if (Wire.endTransmission() != 0)
    {
        ++error_count;
        return false;
    }

    // The sensor requires time to complete the measurement.
    delay(10);

    uint8_t data[HIDS_DATA_LENGTH];

    if (Wire.requestFrom(
        WSEN_HIDS_I2C_ADDRESS,
        HIDS_DATA_LENGTH) != HIDS_DATA_LENGTH)
    {
        ++error_count;
        return false;
    }

    for (uint8_t& byte : data)
    {
        byte = Wire.read();
    }

    // Temperature and humidity each have their own CRC byte.
    if (calculate_crc(&data[0], 2) != data[2] ||
        calculate_crc(&data[3], 2) != data[5])
    {
        ++error_count;
        return false;
    }

    // Combine MSB and LSB.
    const uint16_t raw_temperature =
        (static_cast<uint16_t>(data[0]) << 8) | data[1];

    const uint16_t raw_humidity =
        (static_cast<uint16_t>(data[3]) << 8) | data[4];

    // Convert raw temperature to Kelvin.
    temperature_k =
        -45.0f +
        175.0f * static_cast<float>(raw_temperature) / 65535.0f +
        273.15f;

    // Convert raw humidity to relative humidity in percent.
    humidity_percent =
        -6.0f +
        125.0f * static_cast<float>(raw_humidity) / 65535.0f;

    // Numerical conversion can produce values slightly outside
    // the physical 0...100 % range.
    humidity_percent = constrain(
        humidity_percent,
        0.0f,
        100.0f
    );

    last_measurement_valid = true;

    return true;
}


// ============================================================================
// Getter functions
// ============================================================================

float wsen_hids_get_temperature()
{
    return temperature_k;
}


float wsen_hids_get_humidity()
{
    return humidity_percent;
}


bool wsen_hids_data_valid()
{
    return last_measurement_valid;
}


uint32_t wsen_hids_get_error_count()
{
    return error_count;
}

bool wsen_hids_is_initialized()
{
    return initialized;
}