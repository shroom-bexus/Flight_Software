// SHROOM Flight Software
// Ground-station telemetry formatting

#include "telemetry.h"

#include <Arduino.h>

#include "config.h"
#include "teensy_link.h"

#if ENABLE_ETHERNET
#include "airdos.h"
#include "ethernet_link.h"
#include "logger.h"
#include "rtc.h"
#include "max31865.h"
#include "wsen_hids.h"
#include "wsen_isds.h"
#include "wsen_pads.h"

#if ENABLE_THERMAL_CONTROL
#include "heater.h"
#include "thermal_control.h"
#endif

namespace
{
uint32_t last_health_time = 0;
bool system_telemetry_sent = false;

const char* health_state(bool initialized, bool valid, uint32_t errors)
{
    // WAITING means initialized, but no valid sample has arrived yet.
    if (!initialized) return "FAULT";
    if (valid) return "OK";
    return errors == 0 ? "WAITING" : "FAULT";
}

void send_sensor_health(
    uint32_t time_ms,
    const char* name,
    bool initialized,
    bool valid,
    uint32_t errors
)
{
    char message[96];
    snprintf(
        message,
        sizeof(message),
        "HEALTH,%lu,%s,%s,%lu",
        static_cast<unsigned long>(time_ms),
        name,
        health_state(initialized, valid, errors),
        static_cast<unsigned long>(errors)
    );
    ethernet_link_send_line(message);
}
} // namespace
#endif

void telemetry_send_max31865(uint8_t sensor_id, float temperature_k)
{
#if ENABLE_ETHERNET && ENABLE_MAX31865
    if (!ethernet_link_connected()) return;

    char message[64];
    snprintf(
        message,
        sizeof(message),
        "MAX31865,%lu,%u,%.3f",
        static_cast<unsigned long>(millis()),
        sensor_id,
        temperature_k
    );
    ethernet_link_send_line(message);
#else
    (void)sensor_id;
    (void)temperature_k;
#endif
}

void telemetry_send_thermal()
{
#if ENABLE_ETHERNET && ENABLE_THERMAL_CONTROL
    if (!ethernet_link_connected()) return;

    const uint32_t time_ms = millis();
    char message[96];

    // Controller state and physical heater outputs are separate messages.
    snprintf(
        message,
        sizeof(message),
        "THERMAL,%lu,%u,%.2f,%.3f,%.1f",
        static_cast<unsigned long>(time_ms),
        thermal_control_is_enabled() ? 1 : 0,
        thermal_control_get_target(),
        thermal_control_get_temperature(),
        thermal_control_get_output()
    );
    ethernet_link_send_line(message);

    snprintf(
        message,
        sizeof(message),
        "PID,%lu,%.6g,%.6g,%.6g",
        static_cast<unsigned long>(time_ms),
        thermal_control_get_kp(),
        thermal_control_get_ki(),
        thermal_control_get_kd()
    );
    ethernet_link_send_line(message);

    snprintf(
        message,
        sizeof(message),
        "HEATERS,%lu,%.1f,%.1f,%.1f,%.1f",
        static_cast<unsigned long>(time_ms),
        heater_get_power(Heater::HEATER_1),
        heater_get_power(Heater::HEATER_2),
        heater_get_power(Heater::HEATER_3),
        heater_get_power(Heater::HEATER_4)
    );
    ethernet_link_send_line(message);
#endif
}

void telemetry_send_pads()
{
#if ENABLE_ETHERNET && ENABLE_WSEN_PADS
    if (!ethernet_link_connected()) return;

    char message[96];
    snprintf(
        message,
        sizeof(message),
        "PADS,%lu,%.3f,%.2f",
        static_cast<unsigned long>(millis()),
        wsen_pads_get_temperature(),
        wsen_pads_get_pressure()
    );
    ethernet_link_send_line(message);
#endif
}

void telemetry_send_hids()
{
#if ENABLE_ETHERNET && ENABLE_WSEN_HIDS
    if (!ethernet_link_connected()) return;

    char message[96];
    snprintf(
        message,
        sizeof(message),
        "HIDS,%lu,%.3f,%.2f",
        static_cast<unsigned long>(millis()),
        wsen_hids_get_temperature(),
        wsen_hids_get_humidity()
    );
    ethernet_link_send_line(message);
#endif
}


void telemetry_send_airdos(uint8_t sensor_id, const char* data)
{
#if ENABLE_ETHERNET && ENABLE_AIRDOS
    if (!ethernet_link_connected() || data == nullptr) return;

    // AIRDOS raw messages may contain many comma-separated fields. Keep the
    // complete UART line unchanged after the sensor identifier.
    char message[ETHERNET_TELEMETRY_LINE_MAX];
    const int length = snprintf(
        message,
        sizeof(message),
        "AIRDOS,%lu,%u,%s",
        static_cast<unsigned long>(millis()),
        sensor_id,
        data
    );

    if (length <= 0 || static_cast<size_t>(length) >= sizeof(message)) return;
    ethernet_link_send_airdos_line(sensor_id, message);
#else
    (void)sensor_id;
    (void)data;
#endif
}

void telemetry_update()
{
#if ENABLE_ETHERNET
    if (!ethernet_link_connected()) return;

    // Health data is periodic and uses one timestamp for the complete batch.
    const uint32_t time_ms = millis();
    if (system_telemetry_sent &&
        time_ms - last_health_time < SYSTEM_TELEMETRY_PERIOD_MS)
    {
        return;
    }
    last_health_time = time_ms;
    system_telemetry_sent = true;

    char message[96];

    snprintf(message, sizeof(message), "HEALTH,%lu,SECONDARY,%s,%lu",
        static_cast<unsigned long>(time_ms), teensy_link_state(),
        static_cast<unsigned long>(teensy_link_get_error_count()));
    ethernet_link_send_line(message);

    // Same UTC source as SD timestamps; never label an unset clock as valid.
    char timestamp[21] = "";
    const bool rtc_valid = rtc_is_valid();
    if (rtc_valid) rtc_get_timestamp(timestamp, sizeof(timestamp));
    snprintf(message, sizeof(message), "RTC,%lu,%u,%s",
        static_cast<unsigned long>(time_ms), rtc_valid ? 1 : 0, timestamp);
    ethernet_link_send_line(message);

    // One entry per card, including Secondary cards with reception freshness.
    const char* storage_names[] = {"SD_INTERNAL", "SD_BACKUP",
        "SD_SECONDARY_INTERNAL", "SD_SECONDARY_BACKUP"};
    const char* storage_states[] = {
        !ENABLE_SD_LOGGING ? "DISABLED" : (logger_internal_sd_is_ready() ? "OK" : "FAULT"),
        !ENABLE_BACKUP_SD_LOGGING ? "DISABLED" : (logger_backup_sd_is_ready() ? "OK" : "FAULT"),
        teensy_link_storage_state(0), teensy_link_storage_state(1)
    };
    const uint32_t storage_errors[] = {
        logger_get_internal_sd_error_count(), logger_get_backup_sd_error_count(),
        teensy_link_storage_errors(0), teensy_link_storage_errors(1)
    };
    for (uint8_t i = 0; i < 4; ++i)
    {
        snprintf(message, sizeof(message), "HEALTH,%lu,%s,%s,%lu",
            static_cast<unsigned long>(time_ms), storage_names[i], storage_states[i],
            static_cast<unsigned long>(storage_errors[i]));
        ethernet_link_send_line(message);
    }

#if ENABLE_MAX31865
    // Report every enabled RTD channel separately.
    for (uint8_t i = 0; i < MAX31865_CHANNEL_COUNT; ++i)
    {
        const TempSensor sensor = static_cast<TempSensor>(i);
        if (!max31865_is_enabled(sensor)) continue;

        snprintf(
            message,
            sizeof(message),
            "HEALTH,%lu,MAX31865,%u,%s,%u,%lu",
            static_cast<unsigned long>(time_ms),
            i + 1,
            health_state(
                max31865_is_initialized(sensor),
                max31865_data_valid(sensor),
                max31865_get_error_count(sensor)
            ),
            max31865_get_fault(sensor),
            static_cast<unsigned long>(max31865_get_error_count(sensor))
        );
        ethernet_link_send_line(message);
    }
#endif

#if ENABLE_WSEN_PADS
    send_sensor_health(
        time_ms,
        "PADS",
        wsen_pads_is_initialized(),
        wsen_pads_data_valid(),
        wsen_pads_get_error_count()
    );
#endif

#if ENABLE_WSEN_HIDS
    send_sensor_health(
        time_ms,
        "HIDS",
        wsen_hids_is_initialized(),
        wsen_hids_data_valid(),
        wsen_hids_get_error_count()
    );
#endif

#if ENABLE_WSEN_ISDS
    send_sensor_health(
        time_ms,
        "ISDS",
        wsen_isds_is_initialized(),
        wsen_isds_data_valid(),
        wsen_isds_get_error_count()
    );
#endif

#if ENABLE_AIRDOS
    // For remote sensors, health here describes reception at the primary.
    // Source parser-overflow counters arrive separately every five seconds.
    for (uint8_t id = 1; id <= 7; ++id)
    {
        const bool received = teensy_link_has_received(id);
        const uint32_t age = received
            ? time_ms - teensy_link_last_received_ms(id) : 0;
        const char* state = received
            ? (age <= AIRDOS_TIMEOUT_MS ? "OK" : "FAULT")
            : (time_ms <= AIRDOS_TIMEOUT_MS ? "WAITING" : "FAULT");
        snprintf(message, sizeof(message), "HEALTH,%lu,AIRDOS,%u,%s,%lu,%lu",
            static_cast<unsigned long>(time_ms), id, state,
            static_cast<unsigned long>(age),
            static_cast<unsigned long>(teensy_link_remote_overflows(id)));
        ethernet_link_send_line(message);
    }

    // Report each AIRDOS channel independently.
    for (uint8_t i = 0; i < AIRDOS_CHANNEL_COUNT; ++i)
    {
        uint32_t last_message_age = 0;
        const bool received = airdos_has_received_data(i);
        if (received)
        {
            last_message_age = time_ms - airdos_get_last_message_ms(i);
        }

        const char* state = received
            ? (last_message_age <= AIRDOS_TIMEOUT_MS ? "OK" : "FAULT")
            : (time_ms <= AIRDOS_TIMEOUT_MS ? "WAITING" : "FAULT");

        snprintf(
            message,
            sizeof(message),
            "HEALTH,%lu,AIRDOS,%u,%s,%lu,%lu",
            static_cast<unsigned long>(time_ms),
            airdos_get_sensor_id(i),
            state,
            static_cast<unsigned long>(last_message_age),
            static_cast<unsigned long>(airdos_get_overflow_count(i))
        );
        ethernet_link_send_line(message);
    }
#endif

#if ENABLE_THERMAL_CONTROL
    snprintf(
        message,
        sizeof(message),
        "THERMAL_CONFIG,%lu,%s,%.6g,%.6g,%s",
        static_cast<unsigned long>(time_ms),
        thermal_control_get_mode_name(),
        thermal_control_get_hysteresis(),
        thermal_control_get_bang_bang_power(),
        thermal_control_get_fusion_mode_name()
    );
    ethernet_link_send_line(message);
#endif

    // Confirm the limiter state and AIRDOS selection policy at the ground
    // station. Counts are cumulative since the current flight-computer boot.
    snprintf(
        message,
        sizeof(message),
        "DOWNLINK,%lu,%.2f,%u,%u,%lu,%lu,%u,%u",
        static_cast<unsigned long>(time_ms),
        ethernet_link_get_downlink_limit(),
        ethernet_link_get_airdos_downlink_level(),
        ethernet_link_get_airdos_selected_count(),
        static_cast<unsigned long>(ethernet_link_get_telemetry_drop_count()),
        static_cast<unsigned long>(ethernet_link_get_airdos_suppressed_count()),
        static_cast<unsigned int>(ethernet_link_get_system_queue_size()),
        static_cast<unsigned int>(ethernet_link_get_airdos_queue_size())
    );
    ethernet_link_send_line(message);
#endif
}

