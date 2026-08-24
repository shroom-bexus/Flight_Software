//   ___ _  _ ___  ___   ___  __  __
//  / __| || | _ \/ _ \ / _ \|  \/  |
//  \__ \ __ |   / (_) | (_) | |\/| |
//  |___/_||_|_|_\\___/ \___/|_|  |_|
// -----------------------------------
// BEXUS - Student Balloon Experiment

/**
 * @file main.cpp
 * @brief Main entry point for the SHROOM flight software.
 */

#include <Arduino.h>

#include "config.h"
#include "max31865.h"
#include "wsen_pads.h"


void setup()
{
    Serial.begin(115200);

    delay(500);

    Serial.println();
    Serial.println("====================================");
    Serial.println("       SHROOM Flight Software");
    Serial.println("====================================");

#if FLIGHT_PRIMARY
    Serial.println("Target: PRIMARY");
#elif FLIGHT_SECONDARY
    Serial.println("Target: SECONDARY");
#endif

    Serial.println();

    // ------------------------------------------------------------------------
    // Sensor initialization
    // ------------------------------------------------------------------------

    max31865_init();
    wsen_pads_init();

    Serial.println("Initialization complete.");
    Serial.println();
}


void loop()
{
    // ------------------------------------------------------------------------
    // Sensor update
    // ------------------------------------------------------------------------

    max31865_update();
    wsen_pads_update();

    // ------------------------------------------------------------------------
    // Debug output
    // ------------------------------------------------------------------------

    Serial.print("MAX31865 1 temperature: ");
    Serial.println(max31865_get_temperature(TempSensor::TEMP_1));

    Serial.print("WSEN-PADS temperature: ");
    Serial.println(wsen_pads_get_temperature());

    Serial.print("WSEN-PADS pressure: ");
    Serial.println(wsen_pads_get_pressure());

    Serial.println();

    delay(500);
}