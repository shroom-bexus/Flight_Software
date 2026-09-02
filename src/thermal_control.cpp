// SHROOM Flight Software
// Thermal PID controller and persistent settings

#include "thermal_control.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <cmath>

#include "config.h"
#include "heater.h"
#include "max31865.h"

namespace
{
// Dynamic PID state is rebuilt after startup or a mode change.
struct ControllerState
{
    float integral = 0.0f;
    float previous_temperature_k = NAN;
    uint32_t previous_time_ms = 0;
    float output_percent = 0.0f;
    float target_k = THERMAL_TARGET_K;
    bool initialized = false;
    bool enabled = true;
};

// The marker distinguishes saved settings from unused EEPROM contents.
constexpr uint32_t SETTINGS_MAGIC = 0x5348524D; // "SHRM"
constexpr uint32_t SETTINGS_VERSION = 2;

// These operator settings must survive a short power interruption or reset.
struct PersistentSettings
{
    uint32_t magic;
    bool thermal_enabled;
    float target_k;
    float heater_power[HEATER_CHANNEL_COUNT];
    // Version follows the old layout so existing settings can be migrated.
    uint32_t version;
    float kp;
    float ki;
    float kd;
};

ControllerState controller;
PersistentSettings settings;

void save_settings()
{
    EEPROM.put(0, settings);
}

bool pid_values_valid(float kp, float ki, float kd)
{
    return std::isfinite(kp) && std::isfinite(ki) && std::isfinite(kd) &&
        kp >= 0.0f && ki >= 0.0f && kd >= 0.0f &&
        kp <= THERMAL_MAX_PID_GAIN &&
        ki <= THERMAL_MAX_PID_GAIN &&
        kd <= THERMAL_MAX_PID_GAIN;
}

void set_pid_defaults()
{
    settings.kp = THERMAL_DEFAULT_KP;
    settings.ki = THERMAL_DEFAULT_KI;
    settings.kd = THERMAL_DEFAULT_KD;
}

void load_settings()
{
    EEPROM.get(0, settings);
    if (settings.magic != SETTINGS_MAGIC)
    {
        // First start: initialize EEPROM with safe defaults.
        settings = {};
        settings.magic = SETTINGS_MAGIC;
        settings.thermal_enabled = true;
        settings.target_k = THERMAL_TARGET_K;
        settings.version = SETTINGS_VERSION;
        set_pid_defaults();
        save_settings();
        return;
    }

    bool settings_changed = false;

    // Older settings did not contain PID gains. Preserve their other values.
    if (settings.version != SETTINGS_VERSION ||
        !pid_values_valid(settings.kp, settings.ki, settings.kd))
    {
        settings.version = SETTINGS_VERSION;
        set_pid_defaults();
        settings_changed = true;
    }

    if (settings_changed) save_settings();
}

void reset_pid()
{
    // Keep target and enabled state, but discard the PID history.
    controller.integral = 0.0f;
    controller.previous_temperature_k = NAN;
    controller.previous_time_ms = 0;
    controller.output_percent = 0.0f;
    controller.initialized = false;
    heater_all_off();
}

void apply_output(float output_percent)
{
    // One command drives all four heaters on the shared thermal mass.
    controller.output_percent = constrain(
        output_percent,
        0.0f,
        THERMAL_MAX_OUTPUT_PERCENT
    );
    heater_set_all_power(controller.output_percent);
}
} // namespace

void thermal_control_init()
{
    load_settings();
    controller = {};
    controller.target_k = settings.target_k;
    controller.enabled = settings.thermal_enabled;
    // Outputs remain off until a valid and safe temperature is available.
    heater_all_off();
}

void thermal_control_update()
{
    const TempSensor sensor = THERMAL_CONTROL_SENSOR;
    // Never heat without a valid control sensor.
    if (!max31865_is_enabled(sensor) || !max31865_data_valid(sensor))
    {
        controller.enabled ? reset_pid() : heater_all_off();
        return;
    }

    const float temperature_k = max31865_get_temperature(sensor);
    // This cutoff also overrides stored manual heater outputs.
    if (temperature_k >= THERMAL_MAX_TEMPERATURE_K)
    {
        controller.enabled ? reset_pid() : heater_all_off();
        return;
    }

    // Manual outputs are restored only after the sensor and safety checks.
    if (!controller.enabled)
    {
        for (uint8_t i = 0; i < HEATER_CHANNEL_COUNT; ++i)
        {
            heater_set_power(static_cast<Heater>(i), settings.heater_power[i]);
        }
        return;
    }

    const uint32_t current_time_ms = millis();
    const float error = controller.target_k - temperature_k;

    // The first sample has no previous value for I or D calculations.
    if (!controller.initialized)
    {
        controller.previous_temperature_k = temperature_k;
        controller.previous_time_ms = current_time_ms;
        controller.initialized = true;
        apply_output(settings.kp * error);
        return;
    }

    // millis() subtraction remains valid across its overflow.
    const float dt = static_cast<float>(
        current_time_ms - controller.previous_time_ms
    ) / 1000.0f;
    if (dt <= 0.0f) return;

    const float p_term = settings.kp * error;

    // Derivative on the measurement avoids a kick after target changes.
    const float temperature_rate =
        (temperature_k - controller.previous_temperature_k) / dt;
    const float d_term = -settings.kd * temperature_rate;

    float integral_candidate = constrain(
        controller.integral + settings.ki * error * dt,
        -THERMAL_MAX_OUTPUT_PERCENT,
        THERMAL_MAX_OUTPUT_PERCENT
    );

    // Integrate unless doing so would drive the output farther into saturation.
    const float candidate_output = p_term + integral_candidate + d_term;
    const bool saturating_high =
        candidate_output > THERMAL_MAX_OUTPUT_PERCENT && error > 0.0f;
    const bool saturating_low = candidate_output < 0.0f && error < 0.0f;
    if (!saturating_high && !saturating_low)
    {
        controller.integral = integral_candidate;
    }

    apply_output(p_term + controller.integral + d_term);
    controller.previous_temperature_k = temperature_k;
    controller.previous_time_ms = current_time_ms;
}

float thermal_control_get_output()
{
    return controller.output_percent;
}

float thermal_control_get_target()
{
    return controller.target_k;
}

float thermal_control_get_temperature()
{
    const TempSensor sensor = THERMAL_CONTROL_SENSOR;
    if (!max31865_is_enabled(sensor) || !max31865_data_valid(sensor)) return NAN;
    return max31865_get_temperature(sensor);
}

float thermal_control_get_kp()
{
    return settings.kp;
}

float thermal_control_get_ki()
{
    return settings.ki;
}

float thermal_control_get_kd()
{
    return settings.kd;
}

bool thermal_control_set_target(float target_k)
{
    if (!std::isfinite(target_k) || target_k <= 0.0f ||
        target_k >= THERMAL_MAX_TEMPERATURE_K)
    {
        return false;
    }

    controller.target_k = target_k;
    settings.target_k = target_k;
    save_settings();
    return true;
}

bool thermal_control_set_pid(float kp, float ki, float kd)
{
    if (!pid_values_valid(kp, ki, kd)) return false;

    settings.kp = kp;
    settings.ki = ki;
    settings.kd = kd;
    save_settings();

    // Discard the history calculated with the previous gains.
    if (controller.enabled) reset_pid();
    return true;
}

void thermal_control_set_enabled(bool enabled)
{
    if (controller.enabled == enabled) return;

    controller.enabled = enabled;
    settings.thermal_enabled = enabled;
    // A mode change always starts with zero manual heater power.
    for (float& power : settings.heater_power) power = 0.0f;
    save_settings();
    reset_pid();
}

bool thermal_control_is_enabled()
{
    return controller.enabled;
}

void thermal_control_save_heater_state()
{
    for (uint8_t i = 0; i < HEATER_CHANNEL_COUNT; ++i)
    {
        settings.heater_power[i] = heater_get_power(static_cast<Heater>(i));
    }
    save_settings();
}
