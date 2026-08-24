//   ___ _  _ ___  ___   ___  __  __
//  / __| || | _ \/ _ \ / _ \|  \/  |
//  \__ \ __ |   / (_) | (_) | |\/| |
//  |___/_||_|_|_\\___/ \___/|_|  |_|
// -----------------------------------
// BEXUS - Student Balloon Experiment

#include "wsen_pads.h"


#include <Wire.h>
#include "config.h"

// -----------------------------------------------------------------------------
// WSEN-PADS registers
// -----------------------------------------------------------------------------

static constexpr uint8_t REG_DEVICE_ID = 0x0F;
static constexpr uint8_t REG_CTRL_1    = 0x10;
static constexpr uint8_t REG_CTRL_2    = 0x11;
static constexpr uint8_t REG_STATUS    = 0x27;

static constexpr uint8_t REG_PRESS_XL  = 0x28;

static constexpr uint8_t DEVICE_ID     = 0xB3;

// STATUS register
static constexpr uint8_t STATUS_P_DA = 0x01;
static constexpr uint8_t STATUS_T_DA = 0x02;

// CTRL_REG1
static constexpr uint8_t CTRL1_BDU = 0x02;

// CTRL_REG2
static constexpr uint8_t CTRL2_LOW_NOISE = 0x02;
static constexpr uint8_t CTRL2_SWRESET   = 0x04;
static constexpr uint8_t CTRL2_AUTO_INC  = 0x10;


// -----------------------------------------------------------------------------
// Stored measurement data
// -----------------------------------------------------------------------------

static float pressure_Pa    = NAN;
static float temperature_K = NAN;

static bool dataValid = false;


// -----------------------------------------------------------------------------
// Internal I2C functions
// -----------------------------------------------------------------------------

static bool write_register(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(WSEN_PADS_ADDRESS);

    Wire.write(reg);
    Wire.write(value);

    return Wire.endTransmission() == 0;
}


static bool read_register(uint8_t reg, uint8_t &value)
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


static bool read_registers(uint8_t startReg, uint8_t *buffer, uint8_t length)
{
    Wire.beginTransmission(WSEN_PADS_ADDRESS);
    Wire.write(startReg);

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

    for (uint8_t i = 0; i < length; i++)
    {
        buffer[i] = Wire.read();
    }

    return true;
}


// -----------------------------------------------------------------------------
// ODR configuration
// -----------------------------------------------------------------------------

static bool get_odr_bits(uint16_t odrHz, uint8_t &bits)
{
    switch (odrHz)
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


// -----------------------------------------------------------------------------
// Public functions
// -----------------------------------------------------------------------------

bool wsen_pads_init()
{
    Wire.begin();
    Wire.setClock(400000);

    delay(10);

    // -------------------------------------------------------------------------
    // Verify sensor identity
    // -------------------------------------------------------------------------

    uint8_t deviceID;

    if (!read_register(REG_DEVICE_ID, deviceID))
    {
        Serial.println("WSEN-PADS: I2C communication failed.");
        return false;
    }

    if (deviceID != DEVICE_ID)
    {
        Serial.print("WSEN-PADS: Wrong device ID: 0x");
        Serial.println(deviceID, HEX);
        return false;
    }

    // -------------------------------------------------------------------------
    // Software reset
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    // Configure CTRL_REG2
    //
    // Auto increment:
    // Required so pressure + temperature can be read in one transaction.
    //
    // Low noise:
    // Available for ODR < 100 Hz.
    // -------------------------------------------------------------------------

    ctrl2 = CTRL2_AUTO_INC;

    if (WSEN_PADS_ODR_HZ < 100)
    {
        ctrl2 |= CTRL2_LOW_NOISE;
    }

    if (!write_register(REG_CTRL_2, ctrl2))
    {
        return false;
    }

    // -------------------------------------------------------------------------
    // Configure output data rate
    // -------------------------------------------------------------------------

    uint8_t odrBits;

    if (!get_odr_bits(WSEN_PADS_ODR_HZ, odrBits))
    {
        return false;
    }

    uint8_t ctrl1 = CTRL1_BDU | (odrBits << 4);

    if (!write_register(REG_CTRL_1, ctrl1))
    {
        return false;
    }

    dataValid = false;

    return true;
}


bool wsen_pads_update()
{
    // -------------------------------------------------------------------------
    // Check whether both pressure and temperature data are available
    // -------------------------------------------------------------------------

    uint8_t status;

    if (!read_register(REG_STATUS, status))
    {
        return false;
    }

    if ((status & (STATUS_P_DA | STATUS_T_DA))
        != (STATUS_P_DA | STATUS_T_DA))
    {
        return false;
    }

    // -------------------------------------------------------------------------
    // Read pressure + temperature in one transaction
    //
    // 0x28  Pressure XL
    // 0x29  Pressure L
    // 0x2A  Pressure H
    // 0x2B  Temperature L
    // 0x2C  Temperature H
    // -------------------------------------------------------------------------

    uint8_t data[5];

    if (!read_registers(REG_PRESS_XL, data, sizeof(data)))
    {
        return false;
    }

    // -------------------------------------------------------------------------
    // Pressure: signed 24-bit two's complement
    // -------------------------------------------------------------------------

    int32_t rawPressure =
        (static_cast<int32_t>(data[2]) << 16) |
        (static_cast<int32_t>(data[1]) << 8)  |
         static_cast<int32_t>(data[0]);

    // Sign extension from 24 bit to 32 bit
    if (rawPressure & 0x00800000)
    {
        rawPressure |= 0xFF000000;
    }

    // -------------------------------------------------------------------------
    // Temperature: signed 16-bit two's complement
    // -------------------------------------------------------------------------

    int16_t rawTemperature =
        static_cast<int16_t>(
            (static_cast<uint16_t>(data[4]) << 8) |
             static_cast<uint16_t>(data[3])
        );

    // -------------------------------------------------------------------------
    // Convert to physical units
    // -------------------------------------------------------------------------

    float newPressure =
        static_cast<float>(rawPressure) / 40.96f;

    float newTemperature =
        static_cast<float>(rawTemperature) / 100.0 + 273.15f;

    // Only replace stored values after a successful complete read
    pressure_Pa = newPressure;
    temperature_K = newTemperature;

    dataValid = true;

    return true;
}


float wsen_pads_get_pressure()
{
    return pressure_Pa;
}


float wsen_pads_get_temperature()
{
    return temperature_K;
}


bool wsen_pads_data_valid()
{
    return dataValid;
}