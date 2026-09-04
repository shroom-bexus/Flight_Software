// SHROOM Flight Software
// Main application scheduler

#include <Arduino.h>
#include <Wire.h>
#include <elapsedMillis.h>

#include "config.h"
#include "logger.h"
#include "logo.h"
#include "rtc.h"
#include "telemetry.h"

#if ENABLE_AIRDOS
#include "airdos.h"
#endif
#if ENABLE_ETHERNET
#include "commands.h"
#include "ethernet_link.h"
#endif
#if ENABLE_HEATERS
#include "heater.h"
#endif
#if ENABLE_MAX31865
#include "max31865.h"
#endif
#if ENABLE_THERMAL_CONTROL
#include "thermal_control.h"
#endif
#if ENABLE_WSEN_HIDS
#include "wsen_hids.h"
#endif
#if ENABLE_WSEN_ISDS
#include "wsen_isds.h"
#endif
#if ENABLE_WSEN_PADS
#include "wsen_pads.h"
#endif

namespace
{
// Each sensor keeps its own sampling interval.
#if ENABLE_MAX31865
elapsedMillis max31865_timer;
#endif
#if ENABLE_WSEN_PADS
elapsedMillis pads_timer;
#endif
#if ENABLE_WSEN_HIDS
elapsedMillis hids_timer;
#endif
#if ENABLE_WSEN_ISDS
elapsedMillis isds_timer;
#endif
#if ENABLE_ETHERNET
bool ethernet_ready = false;
char ethernet_rx_line[ETHERNET_RX_BUFFER_SIZE];
#endif

void print_init_result(const char* name, bool success)
{
    Serial.print(name);
    Serial.println(success ? ": OK" : ": FAILED");
}

void update_ethernet()
{
#if ENABLE_ETHERNET
    if (!ethernet_ready) return;

    // Service UDP traffic, then dispatch one complete command.
    ethernet_link_update();
    if (ethernet_link_read_line(ethernet_rx_line, sizeof(ethernet_rx_line)))
    {
        commands_handle(ethernet_rx_line);
    }
#endif
}

void update_max31865()
{
#if ENABLE_MAX31865
    // Only take a new measurement after the configured interval.
    if (max31865_timer < MAX31865_SAMPLE_PERIOD_MS) return;
    max31865_timer = 0;

    max31865_update();

#if ENABLE_THERMAL_CONTROL
    // The controller always uses the newly sampled temperature.
    thermal_control_update();
#endif
    telemetry_send_thermal();

    // Faulty or disconnected channels are skipped independently.
    for (uint8_t i = 0; i < MAX31865_CHANNEL_COUNT; ++i)
    {
        const TempSensor sensor = static_cast<TempSensor>(i);
        if (max31865_data_valid(sensor))
        {
            const uint8_t sensor_id = i + 1;
            const float temperature_k = max31865_get_temperature(sensor);

            // Use the same 1-based sensor ID on SD and downlink.
            logger_log_max31865(sensor_id, temperature_k);
            telemetry_send_max31865(sensor_id, temperature_k);
        }
    }
#endif
}

void update_pads()
{
#if ENABLE_WSEN_PADS
    if (pads_timer < WSEN_PADS_SAMPLE_PERIOD_MS) return;
    pads_timer = 0;

    // Only store and transmit complete measurements.
    if (wsen_pads_update())
    {
        logger_log_wsen_pads(
            wsen_pads_get_temperature(),
            wsen_pads_get_pressure()
        );
        telemetry_send_pads();
    }
#endif
}

void update_hids()
{
#if ENABLE_WSEN_HIDS
    if (hids_timer < WSEN_HIDS_SAMPLE_PERIOD_MS) return;
    hids_timer = 0;

    // HIDS follows the same update path as PADS.
    if (wsen_hids_update())
    {
        logger_log_wsen_hids(
            wsen_hids_get_temperature(),
            wsen_hids_get_humidity()
        );
        telemetry_send_hids();
    }
#endif
}

void update_isds()
{
#if ENABLE_WSEN_ISDS
    if (isds_timer < WSEN_ISDS_SAMPLE_PERIOD_MS) return;
    isds_timer = 0;

    // Sample at high rate, but only store configured motion events.
    if (wsen_isds_update() && wsen_isds_event_detected())
    {
        logger_log_wsen_isds(
            wsen_isds_get_accel_x(),
            wsen_isds_get_accel_y(),
            wsen_isds_get_accel_z(),
            wsen_isds_get_gyro_x(),
            wsen_isds_get_gyro_y(),
            wsen_isds_get_gyro_z()
        );
    }
#endif
}

void update_airdos()
{
#if ENABLE_AIRDOS
    // Drain every complete UART line already waiting on every AIRDOS channel.
    for (uint8_t i = 0; i < AIRDOS_CHANNEL_COUNT; ++i)
    {
        while (airdos_update(i))
        {
            const uint8_t sensor_id = airdos_get_sensor_id(i);
            const char* data = airdos_get_data(i);

            logger_log_airdos(sensor_id, data);
            telemetry_send_airdos(sensor_id, data);
        }
    }
#endif
}
} // namespace

void setup()
{
    Serial.begin(115200);
    // Give the USB serial connection time to appear after power-up.
    delay(2000);

    Serial.println();
    Serial.print(SHROOM_LOGO);
    Serial.println(FLIGHT_PRIMARY ? "Target: PRIMARY" : "Target: SECONDARY");
    Serial.print("Version: ");
    Serial.println(FLIGHT_VERSION);
    Serial.println();

#if ENABLE_WSEN_PADS || ENABLE_WSEN_HIDS || ENABLE_WSEN_ISDS
    // All enabled WSEN sensors share the main I2C bus.
    Wire.begin();
    Wire.setClock(I2C_CLOCK_HZ);
#endif

    print_init_result("RTC", rtc_init());

#if ENABLE_SD_LOGGING
    print_init_result("SD logger", logger_init());
#endif
#if ENABLE_HEATERS
    heater_init();
    Serial.println("Heaters: OK");
#endif
#if ENABLE_MAX31865
    print_init_result("MAX31865", max31865_init());
#endif
#if ENABLE_THERMAL_CONTROL
    thermal_control_init();
    Serial.println("Thermal control: OK");
#endif
#if ENABLE_WSEN_PADS
    print_init_result("WSEN-PADS", wsen_pads_init());
#endif
#if ENABLE_WSEN_HIDS
    print_init_result("WSEN-HIDS", wsen_hids_init());
#endif
#if ENABLE_WSEN_ISDS
    print_init_result("WSEN-ISDS", wsen_isds_init());
#endif
#if ENABLE_AIRDOS
    airdos_init();
    Serial.println("AIRDOS: OK");
#endif
#if ENABLE_ETHERNET
    ethernet_ready = ethernet_link_init();
    print_init_result("Ethernet", ethernet_ready);
#endif

    Serial.println("\nInitialization complete.\n");
}

void loop()
{
    // Cooperative scheduler: every module decides whether work is due.
    update_ethernet();
    update_max31865();
    update_pads();
    update_hids();
    update_isds();
    update_airdos();
    telemetry_update();
    logger_update();
}
