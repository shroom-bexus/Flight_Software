
// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

/**
 * @file main.cpp
 * @brief Main entry point for the SHROOM flight software.
 */

#include <Arduino.h>
#include "config.h"
#include "max31865.h"
#include "wsen_pads.h"
#include "logo.h"
#include "wsen_hids.h"
#include "elapsedMillis.h"
#include "rtc.h"

// Sampling timers
elapsedMillis max31865Timer;
elapsedMillis padsTimer;
elapsedMillis hidsTimer;
elapsedMillis isdsTimer;

void setup()
{
    // Serial Port initialisation
    Serial.begin(115200);

    delay(500);

    // Serial Output and Target selevt
    Serial.println();
    Serial.print(SHROOM_LOGO);

    #if FLIGHT_PRIMARY
    Serial.println("Target: PRIMARY");
    #elif FLIGHT_SECONDARY
    Serial.println("Target: SECONDARY");
    #endif

    Serial.print("Version: ");
    Serial.println(FLIGHT_VERSION);

    Serial.println();

    // ------------------------------------------------------------------------
    // Sensor initialization
    // ------------------------------------------------------------------------

    max31865_init();
    wsen_pads_init();
    wsen_hids_init();

    Serial.println("Initialization complete.");
    Serial.println();
}


void loop()
{

    // ------------------------------------------------------------------------
    // Sensor update
    // ------------------------------------------------------------------------

    // MAX31865
    if (max31865Timer >= MAX31865_SAMPLE_PERIOD_MS)
    {
        max31865Timer = 0;

        max31865_update();
        Serial.print("MAX31865 1 temperature: ");
        Serial.println(max31865_get_temperature(TempSensor::TEMP_1));
    }

    // WSEN_PADS
    if (padsTimer >= WSEN_PADS_SAMPLE_PERIOD_MS)
    {
        padsTimer = 0;

        wsen_pads_update();
        Serial.print("WSEN-PADS temperature: ");
        Serial.println(wsen_pads_get_temperature());

        Serial.print("WSEN-PADS pressure: ");
        Serial.println(wsen_pads_get_pressure());
    }

    // WSEN_HIDS
    if (hidsTimer >= WSEN_HIDS_SAMPLE_PERIOD_MS)
    {
        hidsTimer = 0;

        wsen_hids_update();
        Serial.print("WSEN-HIDS temperature:");
        Serial.println(wsen_hids_get_temperature());
        Serial.print("WSEN-HIDS humidity: ");
        Serial.println(wsen_hids_get_humidity());
    }
}