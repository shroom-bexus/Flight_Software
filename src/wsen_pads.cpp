// SHROOM Flight Software

#include "wsen_pads.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"


namespace
{
    // Registers

    constexpr uint8_t REG_DEVICE_ID = 0x0F;
    constexpr uint8_t REG_CTRL_1 = 0x10;
    constexpr uint8_t REG_CTRL_2 = 0x11;
    constexpr uint8_t REG_STATUS = 0x27;
    constexpr uint8_t REG_PRESS_XL = 0x28;

    constexpr uint8_t DEVICE_ID = 0xB3;


    // STATUS register
    constexpr uint8_t STATUS_P_DA = 0x01;
    constexpr uint8_t STATUS_T_DA = 0x02;


    // CTRL_REG1
    constexpr uint8_t CTRL1_BDU = 0x02;


    // CTRL_REG2
    constexpr uint8_t CTRL2_LOW_NOISE = 0x02;
    constexpr uint8_t CTRL2_SWRESET = 0x04;
    constexpr uint8_t CTRL2_AUTO_INC = 0x10;


    // Stored measurement data

    float pressure_pa = NAN;
    float temperature_k = NAN;

    bool initialized = false;
    bool last_measurement_valid = false;

    uint32_t error_count = 0;


    // I2C helper functions

    bool write_register(const uint8_t reg, const uint8_t value)
    {
        Wire.beginTransmission(WSEN_PADS_ADDRESS);

        Wire.write(reg);
        Wire.write(value);

        return Wire.endTransmission() == 0;
    }


    bool read_register(const uint8_t reg, uint8_t& value)
    {
        Wire.beginTransmission(WSEN_PADS_ADDRESS);
        Wire.write(reg);

        if (Wire.endTransmission(false) != 0)
        {
            return false;
        }

        if (Wire.requestFrom(
            static_cast<uint8_t>(WSEN_PADS_ADDRESS),
            static_cast<uint8_t>(1)) != 1)
        {
            return false;
        }

        value = Wire.read();

        return true;
    }


    bool read_registers(uint8_t start_reg, uint8_t* buffer, uint8_t length)
    {
        Wire.beginTransmission(WSEN_PADS_ADDRESS);
        Wire.write(start_reg);

        if (Wire.endTransmission(false) != 0)
        {
            return false;
        }

        if (Wire.requestFrom(
            static_cast<uint8_t>(WSEN_PADS_ADDRESS),
            length) != length)
        {
            return false;
        }

        for (uint8_t i = 0; i < length; ++i)
        {
            buffer[i] = Wire.read();
        }

        return true;
    }


    // Output data rate

    bool get_odr_bits(uint16_t odr_hz, uint8_t& bits)
    {
        switch (odr_hz)
        {
        case 1:
            bits = 0b001;
            break;

        case 10:
            bits = 0b010;
            break;

        case 25:
            bits = 0b011;
            break;

        case 50:
            bits = 0b100;
            break;

        case 75:
            bits = 0b101;
            break;

        case 100:
            bits = 0b110;
            break;

        case 200:
            bits = 0b111;
            break;

        default:
            return false;
        }

        return true;
    }
} // namespace


// Initialization

bool wsen_pads_init()
{
    pressure_pa = NAN;
    temperature_k = NAN;

    initialized = false;
    last_measurement_valid = false;
    error_count = 0;

    // Verify that the expected sensor is connected.
    uint8_t device_id;

    if (!read_register(REG_DEVICE_ID, device_id))
    {
        return false;
    }

    if (device_id != DEVICE_ID)
    {
        return false;
    }


    // Perform a software reset.
    uint8_t ctrl2;

    if (!read_register(REG_CTRL_2, ctrl2))
    {
        return false;
    }

    if (!write_register(REG_CTRL_2, ctrl2 | CTRL2_SWRESET))
    {
        return false;
    }

    delay(5);


    // Enable register auto-increment so pressure and temperature can be
    // read in one transaction. Low-noise mode is available below 100 Hz.
    ctrl2 = CTRL2_AUTO_INC;

    if constexpr (WSEN_PADS_ODR_HZ < 100)
    {
        ctrl2 |= CTRL2_LOW_NOISE;
    }

    if (!write_register(REG_CTRL_2, ctrl2))
    {
        return false;
    }


    // Configure output data rate and block-data-update.
    uint8_t odr_bits;

    if (!get_odr_bits(WSEN_PADS_ODR_HZ, odr_bits))
    {
        return false;
    }

    const uint8_t ctrl1 =
        CTRL1_BDU | (odr_bits << 4);

    if (!write_register(REG_CTRL_1, ctrl1))
    {
        ++error_count;
        return false;
    }

    initialized = true;
    return true;
}


// Measurement

bool wsen_pads_update()
{
    // The validity flag always describes the most recent update.
    // The last valid measurement values remain stored after an error.
    last_measurement_valid = false;

    uint8_t status;

    if (!read_register(REG_STATUS, status))
    {
        ++error_count;
        return false;
    }

    if ((status & (STATUS_P_DA | STATUS_T_DA)) !=
        (STATUS_P_DA | STATUS_T_DA))
    {
        return false;
    }


    // Pressure occupies three bytes followed by two temperature bytes.
    uint8_t data[5];

    if (!read_registers(REG_PRESS_XL, data, sizeof(data)))
    {
        ++error_count;
        return false;
    }


    // Pressure is stored as a signed 24-bit two's-complement value.
    int32_t raw_pressure =
        (static_cast<int32_t>(data[2]) << 16) |
        (static_cast<int32_t>(data[1]) << 8) |
        static_cast<int32_t>(data[0]);

    // Extend the 24-bit sign bit to 32 bits.
    if (raw_pressure & 0x00800000)
    {
        raw_pressure |= 0xFF000000;
    }


    // Temperature is stored as a signed 16-bit two's-complement value.
    const int16_t raw_temperature =
        static_cast<int16_t>(
            (static_cast<uint16_t>(data[4]) << 8) |
            static_cast<uint16_t>(data[3])
        );


    // Convert raw sensor values to SI units.
    pressure_pa =
        static_cast<float>(raw_pressure) / 40.96f;

    temperature_k =
        static_cast<float>(raw_temperature) / 100.0f +
        273.15f;

    last_measurement_valid = true;

    return true;
}


// Getter functions

float wsen_pads_get_pressure()
{
    return pressure_pa;
}


float wsen_pads_get_temperature()
{
    return temperature_k;
}


bool wsen_pads_data_valid()
{
    return last_measurement_valid;
}

bool wsen_pads_is_initialized()
{
    return initialized;
}


uint32_t wsen_pads_get_error_count()
{
    return error_count;
}