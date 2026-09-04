// SHROOM Flight Software

#include "wsen_isds.h"

#include <Arduino.h>
#include <Wire.h>
#include <cmath>

#include "config.h"


namespace
{
    // Register map
    constexpr uint8_t REG_DEVICE_ID = 0x0F;
    constexpr uint8_t REG_CTRL1_XL = 0x10;
    constexpr uint8_t REG_CTRL2_G = 0x11;
    constexpr uint8_t REG_CTRL3_C = 0x12;
    constexpr uint8_t REG_STATUS = 0x1E;
    constexpr uint8_t REG_G_X_OUT_L = 0x22;

    constexpr uint8_t DEVICE_ID = 0x6A;

    // STATUS register
    constexpr uint8_t STATUS_XLDA = 0x01;
    constexpr uint8_t STATUS_GDA = 0x02;

    // CTRL3_C register
    constexpr uint8_t CTRL3_BDU = 0x40;
    constexpr uint8_t CTRL3_IF_INC = 0x04;
    constexpr uint8_t CTRL3_SW_RESET = 0x01;

    // Stored measurement data
    float accel_x_g = NAN;
    float accel_y_g = NAN;
    float accel_z_g = NAN;

    float gyro_x_dps = NAN;
    float gyro_y_dps = NAN;
    float gyro_z_dps = NAN;

    float accel_sensitivity_g = 0.0f;
    float gyro_sensitivity_dps = 0.0f;

    bool initialized = false;
    bool last_measurement_valid = false;
    bool event_detected = false;

    uint32_t error_count = 0;
    uint32_t last_init_attempt_ms = 0;


    bool write_register(uint8_t reg, uint8_t value)
    {
        Wire.beginTransmission(WSEN_ISDS_I2C_ADDRESS);
        Wire.write(reg);
        Wire.write(value);

        return Wire.endTransmission() == 0;
    }


    bool read_register(uint8_t reg, uint8_t& value)
    {
        Wire.beginTransmission(WSEN_ISDS_I2C_ADDRESS);
        Wire.write(reg);

        if (Wire.endTransmission(false) != 0)
        {
            return false;
        }

        if (Wire.requestFrom(
            static_cast<uint8_t>(WSEN_ISDS_I2C_ADDRESS),
            static_cast<uint8_t>(1)) != 1)
        {
            return false;
        }

        value = Wire.read();
        return true;
    }


    bool read_registers(uint8_t start_reg, uint8_t* buffer, uint8_t length)
    {
        Wire.beginTransmission(WSEN_ISDS_I2C_ADDRESS);
        Wire.write(start_reg);

        if (Wire.endTransmission(false) != 0)
        {
            return false;
        }

        if (Wire.requestFrom(
            static_cast<uint8_t>(WSEN_ISDS_I2C_ADDRESS),
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


    bool get_odr_bits(uint16_t odr_hz, uint8_t& bits)
    {
        switch (odr_hz)
        {
        case 26:
            bits = 0b0010;
            return true;
        case 52:
            bits = 0b0011;
            return true;
        case 104:
            bits = 0b0100;
            return true;
        case 208:
            bits = 0b0101;
            return true;
        case 416:
            bits = 0b0110;
            return true;
        case 833:
            bits = 0b0111;
            return true;
        default:
            return false;
        }
    }


    bool get_accel_config(uint8_t range_g, uint8_t& bits, float& sensitivity_g)
    {
        switch (range_g)
        {
        case 2:
            bits = 0x00;
            sensitivity_g = 0.000061f;
            return true;
        case 4:
            bits = 0x08;
            sensitivity_g = 0.000122f;
            return true;
        case 8:
            bits = 0x0C;
            sensitivity_g = 0.000244f;
            return true;
        case 16:
            bits = 0x04;
            sensitivity_g = 0.000488f;
            return true;
        default:
            return false;
        }
    }


    bool get_gyro_config(
        uint16_t range_dps,
        uint8_t& bits,
        float& sensitivity_dps
    )
    {
        switch (range_dps)
        {
        case 125:
            bits = 0x02;
            sensitivity_dps = 0.004375f;
            return true;
        case 250:
            bits = 0x00;
            sensitivity_dps = 0.00875f;
            return true;
        case 500:
            bits = 0x04;
            sensitivity_dps = 0.0175f;
            return true;
        case 1000:
            bits = 0x08;
            sensitivity_dps = 0.035f;
            return true;
        case 2000:
            bits = 0x0C;
            sensitivity_dps = 0.070f;
            return true;
        default:
            return false;
        }
    }


    int16_t combine_le(uint8_t low, uint8_t high)
    {
        return static_cast<int16_t>(
            (static_cast<uint16_t>(high) << 8) |
            static_cast<uint16_t>(low)
        );
    }


    void mark_communication_failure()
    {
        ++error_count;
        initialized = false;
        last_measurement_valid = false;
        event_detected = false;
    }
} // namespace


bool wsen_isds_init()
{
    last_init_attempt_ms = millis();
    initialized = false;
    last_measurement_valid = false;
    event_detected = false;

    uint8_t device_id = 0;
    if (!read_register(REG_DEVICE_ID, device_id))
    {
        ++error_count;
        return false;
    }

    if (device_id != DEVICE_ID)
    {
        ++error_count;
        return false;
    }

    // Reset the sensor and wait for SW_RESET to clear.
    if (!write_register(REG_CTRL3_C, CTRL3_SW_RESET))
    {
        ++error_count;
        return false;
    }

    bool reset_complete = false;

    for (uint8_t attempt = 0; attempt < 20; ++attempt)
    {
        delay(1);

        uint8_t ctrl3 = 0;
        if (!read_register(REG_CTRL3_C, ctrl3))
        {
            ++error_count;
            return false;
        }

        if ((ctrl3 & CTRL3_SW_RESET) == 0)
        {
            reset_complete = true;
            break;
        }
    }

    if (!reset_complete)
    {
        ++error_count;
        return false;
    }

    uint8_t odr_bits = 0;
    uint8_t accel_bits = 0;
    uint8_t gyro_bits = 0;

    if (!get_odr_bits(WSEN_ISDS_ODR_HZ, odr_bits) ||
        !get_accel_config(
            WSEN_ISDS_ACCEL_RANGE_G,
            accel_bits,
            accel_sensitivity_g
        ) ||
        !get_gyro_config(
            WSEN_ISDS_GYRO_RANGE_DPS,
            gyro_bits,
            gyro_sensitivity_dps
        ))
    {
        ++error_count;
        return false;
    }

    // Block-data-update prevents mixed MSB/LSB samples. Automatic register
    // increment allows one 12-byte burst read for gyro + acceleration.
    if (!write_register(REG_CTRL3_C, CTRL3_BDU | CTRL3_IF_INC))
    {
        ++error_count;
        return false;
    }

    const uint8_t ctrl1_xl =
        static_cast<uint8_t>((odr_bits << 4) | accel_bits);

    const uint8_t ctrl2_g =
        static_cast<uint8_t>((odr_bits << 4) | gyro_bits);

    if (!write_register(REG_CTRL1_XL, ctrl1_xl) ||
        !write_register(REG_CTRL2_G, ctrl2_g))
    {
        ++error_count;
        return false;
    }

    initialized = true;
    return true;
}


bool wsen_isds_update()
{
    last_measurement_valid = false;
    event_detected = false;

    if (!initialized)
    {
        if (millis() - last_init_attempt_ms < WSEN_ISDS_RETRY_PERIOD_MS)
        {
            return false;
        }

        if (!wsen_isds_init())
        {
            return false;
        }
    }

    uint8_t status = 0;
    if (!read_register(REG_STATUS, status))
    {
        mark_communication_failure();
        return false;
    }

    // "No new sample yet" is not a communication error.
    if ((status & (STATUS_XLDA | STATUS_GDA)) !=
        (STATUS_XLDA | STATUS_GDA))
    {
        return false;
    }

    // Registers 0x22...0x2D contain gyro XYZ followed by acceleration XYZ.
    uint8_t data[12];

    if (!read_registers(REG_G_X_OUT_L, data, sizeof(data)))
    {
        mark_communication_failure();
        return false;
    }

    const int16_t raw_gyro_x = combine_le(data[0], data[1]);
    const int16_t raw_gyro_y = combine_le(data[2], data[3]);
    const int16_t raw_gyro_z = combine_le(data[4], data[5]);

    const int16_t raw_accel_x = combine_le(data[6], data[7]);
    const int16_t raw_accel_y = combine_le(data[8], data[9]);
    const int16_t raw_accel_z = combine_le(data[10], data[11]);

    gyro_x_dps = raw_gyro_x * gyro_sensitivity_dps;
    gyro_y_dps = raw_gyro_y * gyro_sensitivity_dps;
    gyro_z_dps = raw_gyro_z * gyro_sensitivity_dps;

    accel_x_g = raw_accel_x * accel_sensitivity_g;
    accel_y_g = raw_accel_y * accel_sensitivity_g;
    accel_z_g = raw_accel_z * accel_sensitivity_g;

    const float accel_magnitude_g = sqrtf(
        accel_x_g * accel_x_g +
        accel_y_g * accel_y_g +
        accel_z_g * accel_z_g
    );

    const float gyro_magnitude_dps = sqrtf(
        gyro_x_dps * gyro_x_dps +
        gyro_y_dps * gyro_y_dps +
        gyro_z_dps * gyro_z_dps
    );

    event_detected =
        fabsf(accel_magnitude_g - 1.0f) >=
            WSEN_ISDS_ACCEL_EVENT_THRESHOLD_G ||
        gyro_magnitude_dps >=
            WSEN_ISDS_GYRO_EVENT_THRESHOLD_DPS;

    last_measurement_valid = true;
    return true;
}


bool wsen_isds_event_detected()
{
    return last_measurement_valid && event_detected;
}


float wsen_isds_get_accel_x()
{
    return accel_x_g;
}

float wsen_isds_get_accel_y()
{
    return accel_y_g;
}

float wsen_isds_get_accel_z()
{
    return accel_z_g;
}

float wsen_isds_get_gyro_x()
{
    return gyro_x_dps;
}

float wsen_isds_get_gyro_y()
{
    return gyro_y_dps;
}

float wsen_isds_get_gyro_z()
{
    return gyro_z_dps;
}


bool wsen_isds_data_valid()
{
    return last_measurement_valid;
}


bool wsen_isds_is_initialized()
{
    return initialized;
}


uint32_t wsen_isds_get_error_count()
{
    return error_count;
}
