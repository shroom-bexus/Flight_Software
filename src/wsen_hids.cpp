
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
#include <math.h>

#include "config.h"


namespace
{

// High precision single-shot measurement
constexpr uint8_t HIDS_MEASURE_CMD = 0xFD;


// Cached measurement values
float temperature = NAN;
float humidity = NAN;

bool data_valid = false;
uint32_t error_count = 0;


// ============================================================================
// CRC
// ============================================================================

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
    data_valid = false;
    error_count = 0;

    temperature = NAN;
    humidity = NAN;


    if (!WSEN_HIDS_ENABLED)
    {
        return false;
    }


    // Check if the sensor responds on the I2C bus
    Wire.beginTransmission(WSEN_HIDS_I2C_ADDRESS);

    if (Wire.endTransmission() != 0)
    {
        ++error_count;
        return false;
    }


    return true;
}


// ============================================================================
// Update
// ============================================================================

bool wsen_hids_update()
{
    if (!WSEN_HIDS_ENABLED)
    {
        return false;
    }


    // Start high precision measurement
    Wire.beginTransmission(WSEN_HIDS_I2C_ADDRESS);
    Wire.write(HIDS_MEASURE_CMD);

    if (Wire.endTransmission() != 0)
    {
        ++error_count;
        return false;
    }


    // Wait for measurement to finish
    delay(10);


    // Temperature MSB + LSB + CRC
    // Humidity    MSB + LSB + CRC
    constexpr uint8_t HIDS_DATA_LENGTH = 6;

    uint8_t data[HIDS_DATA_LENGTH];

    if (Wire.requestFrom(WSEN_HIDS_I2C_ADDRESS, HIDS_DATA_LENGTH) != HIDS_DATA_LENGTH)
    {
        ++error_count;
        return false;
    }


    for (uint8_t i = 0; i < 6; ++i)
    {
        data[i] = Wire.read();
    }


    // Check CRC of temperature and humidity
    if (calculate_crc(&data[0], 2) != data[2] ||
        calculate_crc(&data[3], 2) != data[5])
    {
        ++error_count;
        return false;
    }


    // Combine MSB and LSB
    const uint16_t raw_temperature =
        (static_cast<uint16_t>(data[0]) << 8) | data[1];

    const uint16_t raw_humidity =
        (static_cast<uint16_t>(data[3]) << 8) | data[4];


    // Convert temperature to °C
    temperature =
        -45.0f +
        175.0f * static_cast<float>(raw_temperature) / 65535.0f;


    // Convert relative humidity to %
    humidity =
        -6.0f +
        125.0f * static_cast<float>(raw_humidity) / 65535.0f;


    // Limit humidity to physical range
    humidity = constrain(humidity, 0.0f, 100.0f);


    data_valid = true;

    return true;
}


// ============================================================================
// Getter functions
// ============================================================================

float wsen_hids_get_temperature()
{
    return temperature;
}


float wsen_hids_get_humidity()
{
    return humidity;
}


bool wsen_hids_is_valid()
{
    return data_valid;
}


uint32_t wsen_hids_get_error_count()
{
    return error_count;
}