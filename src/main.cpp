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
 *
 * Initializes all enabled modules and periodically updates the sensors
 * according to the sampling intervals defined in config.h.
 */

#include <Arduino.h>
#include <Wire.h>
#include <elapsedMillis.h>

#include "config.h"
#include "logo.h"
#include "rtc.h"
#include "logger.h"

#if ENABLE_MAX31865
#include "max31865.h"
#endif

#if ENABLE_WSEN_PADS
#include "wsen_pads.h"
#endif

#if ENABLE_WSEN_HIDS
#include "wsen_hids.h"
#endif

#if ENABLE_AIRDOS
#include "airdos.h"
#endif

#if ENABLE_ETHERNET
#include "ethernet_link.h"
#endif


namespace
{

// ============================================================================
// Sensor timers
// ============================================================================

#if ENABLE_MAX31865
elapsedMillis max31865_timer;
#endif

#if ENABLE_WSEN_PADS
elapsedMillis wsen_pads_timer;
bool wsen_pads_ready = false;
#endif

#if ENABLE_WSEN_HIDS
elapsedMillis wsen_hids_timer;
bool wsen_hids_ready = false;
#endif

#if ENABLE_ETHERNET
    bool ethernet_ready = false;

    char ethernet_rx_line[ETHERNET_RX_BUFFER_SIZE];
#endif

// ============================================================================
// Helper functions
// ============================================================================

/**
 * @brief Print the initialization result of one module.
 */
void print_init_result(
    const char* name,
    bool success
)
{
    Serial.print(name);
    Serial.println(success ? ": OK" : ": FAILED");
}

} // namespace


// ============================================================================
// Setup
// ============================================================================

void setup()
{
    // ------------------------------------------------------------------------
    // Serial interface
    // ------------------------------------------------------------------------

    Serial.begin(115200);
    delay(2000);


    // ------------------------------------------------------------------------
    // Startup information
    // ------------------------------------------------------------------------

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
    // Shared I2C bus
    // ------------------------------------------------------------------------

#if ENABLE_WSEN_PADS || ENABLE_WSEN_HIDS || ENABLE_WSEN_ISDS

    Wire.begin();
    Wire.setClock(I2C_CLOCK_HZ);

#endif


    // ------------------------------------------------------------------------
    // RTC
    // ------------------------------------------------------------------------

    print_init_result(
        "RTC",
        rtc_init()
    );


    // ------------------------------------------------------------------------
    // SD logger
    // ------------------------------------------------------------------------

#if ENABLE_SD_LOGGING

    print_init_result(
        "SD logger",
        logger_init()
    );

#endif


    // ------------------------------------------------------------------------
    // MAX31865 temperature sensors
    // ------------------------------------------------------------------------

#if ENABLE_MAX31865

    // Initialization may report FAILED if only one enabled channel fails.
    // The remaining channels can still be used independently.
    print_init_result(
        "MAX31865",
        max31865_init()
    );

#endif


    // ------------------------------------------------------------------------
    // WSEN-PADS pressure sensor
    // ------------------------------------------------------------------------

#if ENABLE_WSEN_PADS

    wsen_pads_ready = wsen_pads_init();

    print_init_result(
        "WSEN-PADS",
        wsen_pads_ready
    );

#endif


    // ------------------------------------------------------------------------
    // WSEN-HIDS humidity sensor
    // ------------------------------------------------------------------------

#if ENABLE_WSEN_HIDS

    wsen_hids_ready = wsen_hids_init();

    print_init_result(
        "WSEN-HIDS",
        wsen_hids_ready
    );

#endif


    // ------------------------------------------------------------------------
    // AIRDOS radiation sensor
    // ------------------------------------------------------------------------

#if ENABLE_AIRDOS

    airdos_init();

    Serial.println("AIRDOS: OK");

#endif


    // ------------------------------------------------------------------------
    // Ethernet
    // ------------------------------------------------------------------------

#if ENABLE_ETHERNET

    ethernet_ready = ethernet_link_init();

    print_init_result(
        "Ethernet",
        ethernet_ready
    );

#endif


    Serial.println();
    Serial.println("Initialization complete.");
    Serial.println();
}


// ============================================================================
// Main loop
// ============================================================================

void loop()
{

    // ------------------------------------------------------------------------
    // Ethernet
    // ------------------------------------------------------------------------

#if ENABLE_ETHERNET

    if (ethernet_ready)
    {
        ethernet_link_update();

        if (ethernet_link_read_line(
                ethernet_rx_line,
                sizeof(ethernet_rx_line)))
        {
            Serial.print("Ethernet RX: ");
            Serial.println(ethernet_rx_line);

            // Temporary connection test.
            ethernet_link_send_line("ACK");
        }
    }

#endif


    // ------------------------------------------------------------------------
    // MAX31865
    // ------------------------------------------------------------------------

#if ENABLE_MAX31865

    if (max31865_timer >= MAX31865_SAMPLE_PERIOD_MS)
    {
        max31865_timer = 0;

        // All enabled channels are updated independently.
        // A fault on one channel does not prevent the others from working.
        max31865_update();

        for (uint8_t i = 0; i < MAX31865_CHANNEL_COUNT; ++i)
        {
            const TempSensor sensor =
                static_cast<TempSensor>(i);

            // Only log measurements that were valid during this update.
            if (!max31865_data_valid(sensor))
            {
                continue;
            }

            logger_log_max31865(
                i,
                max31865_get_temperature(sensor)
            );
        }
    }

#endif


    // ------------------------------------------------------------------------
    // WSEN-PADS
    // ------------------------------------------------------------------------

#if ENABLE_WSEN_PADS

    if (wsen_pads_ready &&
        wsen_pads_timer >= WSEN_PADS_SAMPLE_PERIOD_MS)
    {
        wsen_pads_timer = 0;

        // Only use the measurement if new pressure and temperature
        // data were received successfully.
        if (wsen_pads_update())
        {
            logger_log_wsen_pads(
                wsen_pads_get_temperature(),
                wsen_pads_get_pressure()
            );
        }
    }

#endif


    // ------------------------------------------------------------------------
    // WSEN-HIDS
    // ------------------------------------------------------------------------

#if ENABLE_WSEN_HIDS

    if (wsen_hids_ready &&
        wsen_hids_timer >= WSEN_HIDS_SAMPLE_PERIOD_MS)
    {
        wsen_hids_timer = 0;

        if (wsen_hids_update())
        {
            logger_log_wsen_hids(
                wsen_hids_get_temperature(),
                wsen_hids_get_humidity()
            );
        }
    }

#endif


    // ------------------------------------------------------------------------
    // SD logger
    // ------------------------------------------------------------------------

#if ENABLE_SD_LOGGING

    // Periodically flush pending data to the SD card.
    logger_update();

#endif


    // ------------------------------------------------------------------------
    // AIRDOS
    // ------------------------------------------------------------------------

#if ENABLE_AIRDOS

    while (airdos_update())
    {
        logger_log_airdos(
            0,
            airdos_get_data()
        );
    }

#endif
}