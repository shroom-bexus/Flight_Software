// SHROOM Flight Software
// Selectable PID / bang-bang controller and persistent settings

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
    bool bang_bang_on = false;
    bool initialized = false;
    bool enabled = true;
    bool plate_limit_tripped = false;
};

// The marker distinguishes saved settings from unused EEPROM contents.
constexpr uint32_t SETTINGS_MAGIC = 0x5348524D; // "SHRM"
constexpr uint32_t SETTINGS_VERSION = 5;

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
    ThermalMode mode;
    float hysteresis_k;
    float bang_bang_power;
    ThermalFusionMode fusion_mode;
    bool plate_limit_enabled;
    float plate_limit_k;
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

void set_bang_bang_defaults()
{
    settings.mode = ThermalMode::PID;
    settings.hysteresis_k = THERMAL_DEFAULT_HYSTERESIS_K;
    settings.bang_bang_power = THERMAL_DEFAULT_BANG_BANG_POWER_PERCENT;
}

void set_fusion_default()
{
    settings.fusion_mode = THERMAL_DEFAULT_FUSION_MODE;
}

void set_plate_limit_defaults()
{
    settings.plate_limit_enabled = HEATING_PLATE_LIMIT_DEFAULT_ENABLED;
    settings.plate_limit_k = HEATING_PLATE_LIMIT_DEFAULT_K;
}

bool plate_limit_value_valid(float value)
{
    return std::isfinite(value) &&
        value >= HEATING_PLATE_LIMIT_MIN_K &&
        value <= HEATING_PLATE_LIMIT_MAX_K;
}

bool fusion_mode_valid(ThermalFusionMode mode)
{
    return mode == ThermalFusionMode::MEAN ||
        mode == ThermalFusionMode::MEDIAN ||
        mode == ThermalFusionMode::MINIMUM ||
        mode == ThermalFusionMode::MAXIMUM;
}

bool hysteresis_valid(float value)
{
    return std::isfinite(value) && value > 0.0f && value <= THERMAL_MAX_HYSTERESIS_K;
}

bool bang_bang_power_valid(float value)
{
    return std::isfinite(value) && value >= 0.0f && value <= THERMAL_MAX_OUTPUT_PERCENT;
}

float fuse_temperatures(
    const float* values,
    uint8_t count,
    ThermalFusionMode mode
)
{
    if (values == nullptr || count == 0 || count > MAX31865_CHANNEL_COUNT)
    {
        return NAN;
    }

    if (mode == ThermalFusionMode::MEAN)
    {
        float sum = 0.0f;
        for (uint8_t i = 0; i < count; ++i) sum += values[i];
        return sum / static_cast<float>(count);
    }

    if (mode == ThermalFusionMode::MINIMUM)
    {
        float result = values[0];
        for (uint8_t i = 1; i < count; ++i)
            if (values[i] < result) result = values[i];
        return result;
    }

    if (mode == ThermalFusionMode::MAXIMUM)
    {
        float result = values[0];
        for (uint8_t i = 1; i < count; ++i)
            if (values[i] > result) result = values[i];
        return result;
    }

    if (mode != ThermalFusionMode::MEDIAN) return NAN;

    // Median: sort a small local copy so the caller's samples remain unchanged.
    float sorted[MAX31865_CHANNEL_COUNT];
    for (uint8_t i = 0; i < count; ++i) sorted[i] = values[i];

    for (uint8_t i = 1; i < count; ++i)
    {
        const float value = sorted[i];
        uint8_t j = i;
        while (j > 0 && sorted[j - 1] > value)
        {
            sorted[j] = sorted[j - 1];
            --j;
        }
        sorted[j] = value;
    }

    const uint8_t middle = count / 2;
    if ((count & 1U) != 0) return sorted[middle];
    return (sorted[middle - 1] + sorted[middle]) * 0.5f;
}

bool get_control_temperature(float& temperature_k)
{
    float temperatures[THERMAL_CONTROL_SENSOR_COUNT];
    uint8_t valid_count = 0;

    for (uint8_t i = 0; i < THERMAL_CONTROL_SENSOR_COUNT; ++i)
    {
        const TempSensor sensor = THERMAL_CONTROL_SENSORS[i];

        if (!max31865_is_enabled(sensor) || !max31865_data_valid(sensor))
        {
            continue;
        }

        const float value = max31865_get_temperature(sensor);
        if (!std::isfinite(value)) continue;

        temperatures[valid_count++] = value;
    }

    if (valid_count < THERMAL_MIN_VALID_SENSORS)
    {
        temperature_k = NAN;
        return false;
    }

    temperature_k = fuse_temperatures(
        temperatures,
        valid_count,
        settings.fusion_mode
    );
    return std::isfinite(temperature_k);
}

bool control_sensors_safe()
{
    // Safety is evaluated per physical sensor, not on the fused value. This
    // prevents MEAN or MEDIAN from hiding one locally overheated sensor.
    for (uint8_t i = 0; i < THERMAL_CONTROL_SENSOR_COUNT; ++i)
    {
        const TempSensor sensor = THERMAL_CONTROL_SENSORS[i];

        if (!max31865_is_enabled(sensor) || !max31865_data_valid(sensor))
        {
            continue;
        }

        const float value = max31865_get_temperature(sensor);
        if (std::isfinite(value) && value >= THERMAL_MAX_TEMPERATURE_K)
        {
            return false;
        }
    }

    return true;
}

bool plate_heater_selected(Heater heater)
{
    if (HEATING_PLATE_HEATER == 0) return true;
    return static_cast<uint8_t>(heater) + 1 == HEATING_PLATE_HEATER;
}

bool read_plate_temperature(float& temperature_k)
{
    const TempSensor sensor = HEATING_PLATE_TEMP_SENSOR;
    if (!max31865_is_enabled(sensor) || !max31865_data_valid(sensor))
    {
        temperature_k = NAN;
        return false;
    }

    temperature_k = max31865_get_temperature(sensor);
    return std::isfinite(temperature_k);
}

void update_plate_limit_state()
{
    if (!settings.plate_limit_enabled)
    {
        controller.plate_limit_tripped = false;
        return;
    }

    float temperature_k = NAN;
    if (!read_plate_temperature(temperature_k))
    {
        // Fail safe: an enabled limiter never permits heating without a valid
        // temperature from the configured plate sensor.
        controller.plate_limit_tripped = true;
        return;
    }

    if (controller.plate_limit_tripped)
    {
        if (temperature_k <=
            settings.plate_limit_k - HEATING_PLATE_LIMIT_HYSTERESIS_K)
        {
            controller.plate_limit_tripped = false;
        }
    }
    else if (temperature_k >= settings.plate_limit_k)
    {
        controller.plate_limit_tripped = true;
    }
}

void apply_plate_limit_to_outputs()
{
    update_plate_limit_state();
    if (!controller.plate_limit_tripped) return;

    for (uint8_t i = 0; i < HEATER_CHANNEL_COUNT; ++i)
    {
        const Heater heater = static_cast<Heater>(i);
        if (plate_heater_selected(heater)) heater_off(heater);
    }
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
        set_bang_bang_defaults();
        set_fusion_default();
        set_plate_limit_defaults();
        save_settings();
        return;
    }

    bool settings_changed = false;
    const uint32_t stored_version = settings.version;

    // Version 2 already contained PID gains. Versions 3 and 4 keep the same
    // prefix, so append-only migration preserves all operator settings.
    if ((stored_version != 2 && stored_version != 3 &&
         stored_version != 4 && stored_version != SETTINGS_VERSION) ||
        !pid_values_valid(settings.kp, settings.ki, settings.kd))
    {
        set_pid_defaults();
        settings_changed = true;
    }

    // Version 2 did not yet contain bang-bang settings. Version 3 did.
    if (stored_version == 2 ||
        (settings.mode != ThermalMode::PID && settings.mode != ThermalMode::BANG_BANG) ||
        !hysteresis_valid(settings.hysteresis_k) ||
        !bang_bang_power_valid(settings.bang_bang_power))
    {
        set_bang_bang_defaults();
        settings_changed = true;
    }

    // Fusion mode was appended in version 4.
    if (stored_version < 4 || !fusion_mode_valid(settings.fusion_mode))
    {
        set_fusion_default();
        settings_changed = true;
    }

    // Heating-plate limiter settings were appended in version 5.
    if (stored_version != SETTINGS_VERSION ||
        !plate_limit_value_valid(settings.plate_limit_k))
    {
        set_plate_limit_defaults();
        settings_changed = true;
    }

    if (settings.version != SETTINGS_VERSION)
    {
        settings.version = SETTINGS_VERSION;
        settings_changed = true;
    }

    if (settings_changed) save_settings();
}

void reset_controller()
{
    // Keep target and enabled state, but discard the PID history.
    controller.bang_bang_on = false;
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
    apply_plate_limit_to_outputs();
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
    float temperature_k = NAN;

    // Only run when enough configured control sensors produced a fresh value.
    // Invalid channels are excluded from fusion, but the configured minimum
    // decides whether degraded operation is still permitted.
    if (!get_control_temperature(temperature_k))
    {
        controller.enabled ? reset_controller() : heater_all_off();
        return;
    }

    // This cutoff also overrides stored manual heater outputs. It is checked
    // per sensor so one hot location cannot be hidden by sensor fusion.
    if (!control_sensors_safe())
    {
        controller.enabled ? reset_controller() : heater_all_off();
        return;
    }

    // Manual outputs are restored only after the sensor and safety checks.
    if (!controller.enabled)
    {
        for (uint8_t i = 0; i < HEATER_CHANNEL_COUNT; ++i)
        {
            heater_set_power(static_cast<Heater>(i), settings.heater_power[i]);
        }
        apply_plate_limit_to_outputs();
        return;
    }

    if (settings.mode == ThermalMode::BANG_BANG)
    {
        // Inside the band retain the previous state; after reset start OFF.
        if (temperature_k <= controller.target_k - settings.hysteresis_k)
            controller.bang_bang_on = true;
        else if (temperature_k >= controller.target_k + settings.hysteresis_k)
            controller.bang_bang_on = false;
        apply_output(controller.bang_bang_on ? settings.bang_bang_power : 0.0f);
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
    float temperature_k = NAN;
    return get_control_temperature(temperature_k) ? temperature_k : NAN;
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
    if (controller.enabled && settings.mode == ThermalMode::BANG_BANG) reset_controller();
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
    if (controller.enabled && settings.mode == ThermalMode::PID) reset_controller();
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
    reset_controller();
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

void thermal_control_save_heater_power(uint8_t heater_index)
{
    if (heater_index >= HEATER_CHANNEL_COUNT) return;

    settings.heater_power[heater_index] = heater_get_power(
        static_cast<Heater>(heater_index)
    );
    save_settings();
}

void thermal_control_set_plate_limit_enabled(bool enabled)
{
    if (settings.plate_limit_enabled == enabled)
    {
        thermal_control_enforce_plate_limit();
        return;
    }

    settings.plate_limit_enabled = enabled;
    save_settings();
    thermal_control_enforce_plate_limit();
}

bool thermal_control_plate_limit_is_enabled()
{
    return settings.plate_limit_enabled;
}

bool thermal_control_set_plate_limit(float limit_k)
{
    if (!plate_limit_value_valid(limit_k)) return false;

    settings.plate_limit_k = limit_k;
    save_settings();
    thermal_control_enforce_plate_limit();
    return true;
}

float thermal_control_get_plate_limit()
{
    return settings.plate_limit_k;
}

float thermal_control_get_plate_temperature()
{
    float temperature_k = NAN;
    return read_plate_temperature(temperature_k) ? temperature_k : NAN;
}

bool thermal_control_plate_limit_tripped()
{
    return controller.plate_limit_tripped;
}

void thermal_control_enforce_plate_limit()
{
    apply_plate_limit_to_outputs();
}


ThermalMode thermal_control_get_mode() { return settings.mode; }
const char* thermal_control_get_mode_name()
{
    return settings.mode == ThermalMode::BANG_BANG ? "BANG_BANG" : "PID";
}

ThermalFusionMode thermal_control_get_fusion_mode()
{
    return settings.fusion_mode;
}

const char* thermal_control_get_fusion_mode_name()
{
    switch (settings.fusion_mode)
    {
        case ThermalFusionMode::MEAN: return "MEAN";
        case ThermalFusionMode::MEDIAN: return "MEDIAN";
        case ThermalFusionMode::MINIMUM: return "MINIMUM";
        case ThermalFusionMode::MAXIMUM: return "MAXIMUM";
    }
    return "UNKNOWN";
}

bool thermal_control_set_fusion_mode(ThermalFusionMode mode)
{
    if (!fusion_mode_valid(mode)) return false;
    if (settings.fusion_mode == mode) return true;

    settings.fusion_mode = mode;
    save_settings();

    // A changed measurement source invalidates PID derivative/integral history
    // and bang-bang state, but does not change target or enable state.
    if (controller.enabled) reset_controller();
    return true;
}

float thermal_control_get_hysteresis() { return settings.hysteresis_k; }
float thermal_control_get_bang_bang_power() { return settings.bang_bang_power; }

bool thermal_control_set_mode(ThermalMode mode)
{
    if (mode != ThermalMode::PID && mode != ThermalMode::BANG_BANG) return false;
    if (settings.mode == mode) return true;
    settings.mode = mode;
    save_settings();
    // Selecting a regulator does not enable thermal control or alter manual outputs.
    if (controller.enabled) reset_controller();
    return true;
}

bool thermal_control_set_hysteresis(float hysteresis_k)
{
    if (!hysteresis_valid(hysteresis_k)) return false;
    if (settings.hysteresis_k == hysteresis_k) return true;
    settings.hysteresis_k = hysteresis_k;
    save_settings();
    if (controller.enabled && settings.mode == ThermalMode::BANG_BANG) reset_controller();
    return true;
}

bool thermal_control_set_bang_bang_power(float power_percent)
{
    if (!bang_bang_power_valid(power_percent)) return false;
    if (settings.bang_bang_power == power_percent) return true;
    settings.bang_bang_power = power_percent;
    save_settings();
    if (controller.enabled && settings.mode == ThermalMode::BANG_BANG) reset_controller();
    return true;
}
