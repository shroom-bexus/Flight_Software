// g++ -std=c++17 -DFLIGHT_PRIMARY=1 -Itest/host -Iinclude test/host/thermal_test.cpp -o /tmp/shroom-thermal-test
#include <cassert>
#include <cmath>
#include <cstring>
#include "../../src/thermal_control.cpp"
FakeEEPROM EEPROM;
float temperature = 298.15f, plate_temperature = 298.15f, powers[4] = {};
bool valid = true, plate_valid = true, sensor_enabled = true;

bool max31865_is_enabled(TempSensor) { return sensor_enabled; }
bool max31865_data_valid(TempSensor sensor) {
    return sensor == HEATING_PLATE_TEMP_SENSOR ? plate_valid : valid;
}
float max31865_get_temperature(TempSensor sensor) {
    return sensor == HEATING_PLATE_TEMP_SENSOR ? plate_temperature : temperature;
}
void heater_set_power(Heater h, float p) { powers[static_cast<unsigned>(h)] = p; }
float heater_get_power(Heater h) { return powers[static_cast<unsigned>(h)]; }
void heater_set_all_power(float p) { for (float& v : powers) v = p; }
void heater_all_off() { heater_set_all_power(0); }
void sample(float t, float expected) {
    temperature = t;
    plate_temperature = t;
    fake_time += 1000;
    thermal_control_update();
    for (float p : powers) assert(std::abs(p - expected) < 0.001f);
}
int main() {
    // Sensor-fusion math is independent of the selected flight sensor list.
    const float fusion_values[] = {294.0f, 300.0f, 298.0f, 296.0f};
    assert(fuse_temperatures(fusion_values, 4, ThermalFusionMode::MEAN) == 297.0f);
    assert(fuse_temperatures(fusion_values, 4, ThermalFusionMode::MEDIAN) == 297.0f);
    assert(fuse_temperatures(fusion_values, 4, ThermalFusionMode::MINIMUM) == 294.0f);
    assert(fuse_temperatures(fusion_values, 4, ThermalFusionMode::MAXIMUM) == 300.0f);

    const float odd_values[] = {301.0f, 295.0f, 299.0f};
    assert(fuse_temperatures(odd_values, 3, ThermalFusionMode::MEDIAN) == 299.0f);
    assert(std::isnan(fuse_temperatures(nullptr, 0, ThermalFusionMode::MEAN)));
    assert(std::isnan(fuse_temperatures(
        fusion_values,
        4,
        static_cast<ThermalFusionMode>(99)
    )));

    std::memset(EEPROM.bytes, 0xff, sizeof(EEPROM.bytes));
    thermal_control_init();
    assert(thermal_control_get_mode() == ThermalMode::PID);
    assert(thermal_control_get_fusion_mode() == ThermalFusionMode::MEAN);
    assert(std::strcmp(thermal_control_get_fusion_mode_name(), "MEAN") == 0);
    assert(!thermal_control_plate_limit_is_enabled());
    assert(std::abs(
        thermal_control_get_plate_limit() - HEATING_PLATE_LIMIT_DEFAULT_K
    ) < 0.001f);
    assert(!thermal_control_set_plate_limit(HEATING_PLATE_LIMIT_MIN_K - 0.1f));
    assert(!thermal_control_set_plate_limit(HEATING_PLATE_LIMIT_MAX_K + 0.1f));
    assert(!thermal_control_set_plate_limit(NAN));
    assert(!thermal_control_set_fusion_mode(static_cast<ThermalFusionMode>(99)));
    assert(thermal_control_set_fusion_mode(ThermalFusionMode::MAXIMUM));
    assert(std::strcmp(thermal_control_get_fusion_mode_name(), "MAXIMUM") == 0);
    thermal_control_init();
    assert(thermal_control_get_fusion_mode() == ThermalFusionMode::MAXIMUM);
    assert(thermal_control_set_fusion_mode(ThermalFusionMode::MEAN));

    // The plate limiter is independent of the control target and persists.
    assert(thermal_control_set_plate_limit(300.0f));
    thermal_control_set_plate_limit_enabled(true);
    assert(thermal_control_plate_limit_is_enabled());
    assert(thermal_control_set_bang_bang_power(30));
    assert(thermal_control_set_hysteresis(0.5f));
    assert(thermal_control_set_target(305.0f));
    assert(thermal_control_set_mode(ThermalMode::BANG_BANG));
    sample(301.0f, 0);
    assert(thermal_control_plate_limit_tripped());
    sample(299.5f, 0);
    sample(298.5f, 30);
    assert(!thermal_control_plate_limit_tripped());

    // The plate sensor itself is fail-safe independently of the fused control
    // sensors: losing TEMP_3 inhibits heating until a valid cool sample returns.
    plate_valid = false;
    sample(298.5f, 0);
    assert(thermal_control_plate_limit_tripped());
    plate_valid = true;
    sample(298.5f, 30);
    assert(!thermal_control_plate_limit_tripped());

    thermal_control_init();
    assert(thermal_control_plate_limit_is_enabled());
    assert(std::abs(thermal_control_get_plate_limit() - 300.0f) < 0.001f);
    thermal_control_set_plate_limit_enabled(false);

    assert(thermal_control_set_target(298.15f));
    assert(thermal_control_set_mode(ThermalMode::PID));
    assert(thermal_control_set_pid(2, 0, 0));
    sample(297.15f, 2);
    assert(thermal_control_set_bang_bang_power(30));
    assert(thermal_control_set_mode(ThermalMode::BANG_BANG));
    assert(powers[0] == 0);
    sample(298.15f, 0);
    sample(297.65f, 30);
    sample(298.15f, 30);
    sample(298.65f, 0);
    sample(298.15f, 0);
    sample(290, 30);
    valid = false; sample(290, 0);
    valid = true; sample(298.15f, 0);
    sample(290, 30); sample(NAN, 0);
    sample(290, 30); sample(INFINITY, 0);
    sample(290, 30); sample(313.15f, 0);
    sensor_enabled = false; sample(290, 0); sensor_enabled = true;
    assert(!thermal_control_set_hysteresis(0));
    assert(!thermal_control_set_hysteresis(NAN));
    assert(!thermal_control_set_hysteresis(11));
    assert(!thermal_control_set_bang_bang_power(101));
    assert(!thermal_control_set_bang_bang_power(INFINITY));
    assert(!thermal_control_set_mode(static_cast<ThermalMode>(2)));
    assert(thermal_control_set_hysteresis(1));
    assert(thermal_control_set_bang_bang_power(40));
    thermal_control_init();
    assert(thermal_control_get_mode() == ThermalMode::BANG_BANG);
    assert(thermal_control_get_hysteresis() == 1);
    assert(thermal_control_get_bang_bang_power() == 40);
    sample(298.15f, 0); sample(290, 40);
    thermal_control_set_target(299.15f); assert(powers[0] == 0);
    sample(299.15f, 0);
    thermal_control_set_mode(ThermalMode::PID);
    sample(298.15f, 2);
    thermal_control_set_enabled(false);
    heater_set_all_power(12); thermal_control_save_heater_state();
    thermal_control_set_mode(ThermalMode::BANG_BANG);
    assert(!thermal_control_is_enabled()); sample(298.15f, 12);
    thermal_control_init(); sample(298.15f, 12);
    // Real v2 layout: upgrade preserves gains, target, enabled/manual values.
    struct V2 { uint32_t magic; bool enabled; float target; float power[4]; uint32_t version; float kp, ki, kd; };
    V2 old{SETTINGS_MAGIC, false, 300, {11,11,11,11}, 2, 3, 4, 5};
    std::memset(EEPROM.bytes, 0xff, sizeof(EEPROM.bytes)); EEPROM.put(0, old);
    thermal_control_init();
    assert(thermal_control_get_mode() == ThermalMode::PID);
    assert(thermal_control_get_fusion_mode() == THERMAL_DEFAULT_FUSION_MODE);
    assert(thermal_control_get_kp() == 3 && thermal_control_get_ki() == 4 && thermal_control_get_kd() == 5);
    assert(thermal_control_get_target() == 300 && !thermal_control_is_enabled());
    sample(299, 11);

    // Version 3 already contained bang-bang settings but no fusion mode.
    struct V3 {
        uint32_t magic; bool enabled; float target; float power[4];
        uint32_t version; float kp, ki, kd; ThermalMode mode;
        float hysteresis, bb_power;
    };
    V3 old3{
        SETTINGS_MAGIC, true, 301, {0,0,0,0}, 3, 8, 0.01f, 0.5f,
        ThermalMode::BANG_BANG, 1.25f, 35.0f
    };
    std::memset(EEPROM.bytes, 0xff, sizeof(EEPROM.bytes)); EEPROM.put(0, old3);
    thermal_control_init();
    assert(thermal_control_get_mode() == ThermalMode::BANG_BANG);
    assert(thermal_control_get_hysteresis() == 1.25f);
    assert(thermal_control_get_bang_bang_power() == 35.0f);
    assert(thermal_control_get_fusion_mode() == THERMAL_DEFAULT_FUSION_MODE);
    assert(thermal_control_get_target() == 301);
    assert(!thermal_control_plate_limit_is_enabled());
    assert(std::abs(
        thermal_control_get_plate_limit() - HEATING_PLATE_LIMIT_DEFAULT_K
    ) < 0.001f);

    // Version 4 added fusion but predates the plate limiter. Preserve all v4
    // thermal settings and initialize only the newly appended plate fields.
    struct V4 {
        uint32_t magic; bool enabled; float target; float power[4];
        uint32_t version; float kp, ki, kd; ThermalMode mode;
        float hysteresis, bb_power; ThermalFusionMode fusion_mode;
    };
    V4 old4{
        SETTINGS_MAGIC, true, 302, {0,0,0,0}, 4, 7, 0.02f, 0.4f,
        ThermalMode::BANG_BANG, 0.75f, 42.0f, ThermalFusionMode::MAXIMUM
    };
    std::memset(EEPROM.bytes, 0xff, sizeof(EEPROM.bytes)); EEPROM.put(0, old4);
    thermal_control_init();
    assert(thermal_control_get_kp() == 7);
    assert(thermal_control_get_ki() == 0.02f);
    assert(thermal_control_get_kd() == 0.4f);
    assert(thermal_control_get_mode() == ThermalMode::BANG_BANG);
    assert(thermal_control_get_hysteresis() == 0.75f);
    assert(thermal_control_get_bang_bang_power() == 42.0f);
    assert(thermal_control_get_fusion_mode() == ThermalFusionMode::MAXIMUM);
    assert(thermal_control_get_target() == 302);
    assert(!thermal_control_plate_limit_is_enabled());
    assert(std::abs(
        thermal_control_get_plate_limit() - HEATING_PLATE_LIMIT_DEFAULT_K
    ) < 0.001f);

    puts("Thermal controller regression checks passed");
}
