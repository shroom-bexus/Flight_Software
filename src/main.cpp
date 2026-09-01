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
    #include "commands.h"
#endif

#if ENABLE_HEATERS
    #include "heater.h"
#endif

#if ENABLE_THERMAL_CONTROL
    #include "thermal_control.h"
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
    elapsedMillis health_telemetry_timer;
#endif


// ============================================================================
// Ethernet state
// ============================================================================

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
    // Heaters
    // ------------------------------------------------------------------------

#if ENABLE_HEATERS

    heater_init();

    Serial.println("Heaters: OK");

#endif


    // ------------------------------------------------------------------------
    // MAX31865 temperature sensors
    // ------------------------------------------------------------------------

#if ENABLE_MAX31865

    // One failed channel does not prevent the remaining channels
    // from being used.
    print_init_result(
        "MAX31865",
        max31865_init()
    );

#endif


    // ------------------------------------------------------------------------
    // Thermal control
    // ------------------------------------------------------------------------

#if ENABLE_THERMAL_CONTROL

    thermal_control_init();

    Serial.println("Thermal control: OK");

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
            commands_handle(
                ethernet_rx_line
            );
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

        max31865_update();


        // --------------------------------------------------------------------
        // Thermal control
        // --------------------------------------------------------------------

#if ENABLE_THERMAL_CONTROL

        thermal_control_update();

#endif


        // --------------------------------------------------------------------
        // Thermal telemetry
        // --------------------------------------------------------------------

#if ENABLE_ETHERNET && ENABLE_THERMAL_CONTROL

        if (ethernet_link_connected())
        {
            char message[96];

            snprintf(
                message,
                sizeof(message),
                "THERMAL,%lu,%u,%.2f,%.3f,%.1f",
                millis(),
                thermal_control_is_active() ? 1 : 0,
                thermal_control_get_target(),
                thermal_control_get_temperature(),
                thermal_control_get_output()
            );

            ethernet_link_send_line(
                message
            );
        }

#endif


        // --------------------------------------------------------------------
        // MAX31865 logging
        // --------------------------------------------------------------------

        for (uint8_t i = 0; i < MAX31865_CHANNEL_COUNT; ++i)
        {
            const TempSensor sensor =
                static_cast<TempSensor>(i);

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

        if (wsen_pads_update())
        {
            const float temperature_k =
                wsen_pads_get_temperature();

            const float pressure_pa =
                wsen_pads_get_pressure();


            // Log measurement.
            logger_log_wsen_pads(
                temperature_k,
                pressure_pa
            );


            // ----------------------------------------------------------------
            // PADS telemetry
            // ----------------------------------------------------------------

#if ENABLE_ETHERNET

            if (ethernet_link_connected())
            {
                char message[96];

                snprintf(
                    message,
                    sizeof(message),
                    "PADS,%lu,%.3f,%.2f",
                    millis(),
                    temperature_k,
                    pressure_pa
                );

                ethernet_link_send_line(
                    message
                );
            }

#endif
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

    // ========================================================================
// Health telemetry
// ========================================================================

#if ENABLE_ETHERNET

if (ethernet_link_connected() &&
    health_telemetry_timer >= HEALTH_TELEMETRY_PERIOD_MS)
{
    health_telemetry_timer = 0;

    char message[128];

    const uint32_t time_ms = millis();


    // --------------------------------------------------------------------
    // SD card
    // --------------------------------------------------------------------

#if ENABLE_SD_LOGGING

    snprintf(
        message,
        sizeof(message),
        "HEALTH,%lu,SD,%s,%lu",
        time_ms,
        logger_is_ready() ? "OK" : "FAULT",
        logger_get_error_count()
    );

    ethernet_link_send_line(message);

#endif


    // --------------------------------------------------------------------
    // MAX31865
    // --------------------------------------------------------------------

#if ENABLE_MAX31865

    for (uint8_t i = 0; i < MAX31865_CHANNEL_COUNT; ++i)
    {
        const TempSensor sensor =
            static_cast<TempSensor>(i);

        if (!max31865_is_enabled(sensor))
        {
            continue;
        }


        const char* state;

        if (!max31865_is_initialized(sensor))
        {
            state = "FAULT";
        }
        else if (max31865_data_valid(sensor))
        {
            state = "OK";
        }
        else if (max31865_get_error_count(sensor) == 0)
        {
            state = "WAITING";
        }
        else
        {
            state = "FAULT";
        }


        snprintf(
            message,
            sizeof(message),
            "HEALTH,%lu,MAX31865,%u,%s,%u,%lu",
            time_ms,
            i + 1,
            state,
            max31865_get_fault(sensor),
            max31865_get_error_count(sensor)
        );

        ethernet_link_send_line(message);
    }

#endif


    // --------------------------------------------------------------------
    // WSEN-PADS
    // --------------------------------------------------------------------

#if ENABLE_WSEN_PADS

    const char* pads_state;

    if (!wsen_pads_is_initialized())
    {
        pads_state = "FAULT";
    }
    else if (wsen_pads_data_valid())
    {
        pads_state = "OK";
    }
    else if (wsen_pads_get_error_count() == 0)
    {
        pads_state = "WAITING";
    }
    else
    {
        pads_state = "FAULT";
    }


    snprintf(
        message,
        sizeof(message),
        "HEALTH,%lu,PADS,%s,%lu",
        time_ms,
        pads_state,
        wsen_pads_get_error_count()
    );

    ethernet_link_send_line(message);

#endif


    // --------------------------------------------------------------------
    // WSEN-HIDS
    // --------------------------------------------------------------------

#if ENABLE_WSEN_HIDS

    const char* hids_state;

    if (!wsen_hids_is_initialized())
    {
        hids_state = "FAULT";
    }
    else if (wsen_hids_data_valid())
    {
        hids_state = "OK";
    }
    else if (wsen_hids_get_error_count() == 0)
    {
        hids_state = "WAITING";
    }
    else
    {
        hids_state = "FAULT";
    }


    snprintf(
        message,
        sizeof(message),
        "HEALTH,%lu,HIDS,%s,%lu",
        time_ms,
        hids_state,
        wsen_hids_get_error_count()
    );

    ethernet_link_send_line(message);

#endif


    // --------------------------------------------------------------------
    // AIRDOS
    // --------------------------------------------------------------------

#if ENABLE_AIRDOS

    const char* airdos_state;
    uint32_t last_message_age = 0;

    if (!airdos_has_received_data())
    {
        if (time_ms > AIRDOS_TIMEOUT_MS)
        {
            airdos_state = "FAULT";
        }
        else
        {
            airdos_state = "WAITING";
        }
    }
    else
    {
        last_message_age =
            time_ms - airdos_get_last_message_ms();

        if (last_message_age > AIRDOS_TIMEOUT_MS)
        {
            airdos_state = "FAULT";
        }
        else
        {
            airdos_state = "OK";
        }
    }

    snprintf(
        message,
        sizeof(message),
        "HEALTH,%lu,AIRDOS,%s,%lu,%lu",
        time_ms,
        airdos_state,
        last_message_age,
        airdos_get_overflow_count()
    );

    ethernet_link_send_line(message);

#endif
}

#endif

}


