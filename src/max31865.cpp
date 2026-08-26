
// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "max31865.h"
#include "config.h"
#include <Adafruit_MAX31865.h>

/**
 * One Adafruit_MAX31865 object is created for each temperature channel.
 *
 * All MAX31865 devices can share the same SPI bus. Each device is selected
 * individually through its chip-select pin.
 *
 * SPI bus and chip-select pins are defined in config.h so that no hardware
 * configuration is hard-coded inside this driver.
 */
static Adafruit_MAX31865 maxSensors[MAX31865_SENSOR_COUNT] =
{
    Adafruit_MAX31865(MAX31865_CS_1, &MAX31865_SPI_BUS)//,
    // Adafruit_MAX31865(MAX31865_CS_2, &MAX31865_SPI_BUS)
    // ...
    // TODO: Update number of RTDs
};


/**
 * Actual reference resistor value for each MAX31865 measurement channel.
 *
 * Each MAX31865 has its own physical reference resistor. Therefore, the
 * resistor values are stored individually instead of using one common
 * value for every sensor.
 *
 * The corresponding values are defined in config.h.
 */
static constexpr float referenceResistors[MAX31865_SENSOR_COUNT] =
{
    MAX31865_RREF_1//,
    // MAX31865_REF_2
    // ...
    // TODO: Update Number of reference Resistors
};


/**
 * Stores the most recently measured temperature of every sensor.
 *
 * The array is static and therefore only accessible from within this file.
 * Other program modules cannot modify the temperature values directly.
 *
 * update_temp() writes the values.
 * get_temp() reads the values.
 */
static float temperatures[MAX31865_SENSOR_COUNT];


bool max31865_init()
{
    bool success = true;

    /**
     * Initialize every configured MAX31865 device.
     *
     * All temperature sensors currently use a 2-wire PT1000 connection.
     *
     * Initialization continues even if one sensor fails. This allows all
     * configured sensors to be checked before the function returns.
     */
    for (auto & maxSensor : maxSensors)
    {
        if (!maxSensor.begin(MAX31865_2WIRE))
        {
            success = false;
        }
    }

    return success;
}


void max31865_update()
{
    /**
     * Read every configured MAX31865 once.
     *
     * The Adafruit library reads the RTD value from the MAX31865 and converts
     * it into a temperature.
     *
     * MAX31865_RNOMINAL is the nominal resistance of the PT1000 at 0 °C.
     *
     * referenceResistors[i] contains the individually measured reference
     * resistor belonging to the respective MAX31865 channel.
     *
     * The resulting temperature is stored internally and remains unchanged
     * until update_temp() is called again.
     */
    for (uint8_t i = 0; i < MAX31865_SENSOR_COUNT; i++)
    {
        temperatures[i] = maxSensors[i].temperature(
            MAX31865_RNOMINAL,
            referenceResistors[i]
        );
    }
}


float max31865_get_temperature(TempSensor sensor)
{
    /**
     * Convert the strongly typed TempSensor enum into an array index.
     *
     * No communication with the MAX31865 takes place here. The function
     * simply returns the last temperature stored by update_temp().
     */
    const auto index = static_cast<uint8_t>(sensor);

    return temperatures[index]+ 273.15f;
}
