// g++ -std=c++17 -DFLIGHT_PRIMARY=1 -Itest/host -Iinclude test/host/thermal_test.cpp -o /tmp/shroom-thermal-test
#include <cassert>
#include <cmath>
#include "../../src/thermal_control.cpp"
FakeEEPROM EEPROM;
float temperature = 298.15f, powers[4] = {};
bool valid = true, sensor_enabled = true;

bool max31865_is_enabled(TempSensor) { return sensor_enabled; }
bool max31865_data_valid(TempSensor) { return valid; }
float max31865_get_temperature(TempSensor) { return temperature; }
void heater_set_power(Heater h, float p) { powers[static_cast<unsigned>(h)] = p; }
float heater_get_power(Heater h) { return powers[static_cast<unsigned>(h)]; }
void heater_set_all_power(float p) { for (float& v : powers) v = p; }
void heater_all_off() { heater_set_all_power(0); }
void sample(float t, float expected) {
    temperature = t; fake_time += 1000; thermal_control_update();
    for (float p : powers) assert(std::abs(p - expected) < 0.001f);
}
int main() {
    std::memset(EEPROM.bytes, 0xff, sizeof(EEPROM.bytes));
    thermal_control_init();
    assert(thermal_control_get_mode() == ThermalMode::PID);
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
    assert(thermal_control_get_kp() == 3 && thermal_control_get_ki() == 4 && thermal_control_get_kd() == 5);
    assert(thermal_control_get_target() == 300 && !thermal_control_is_enabled());
    sample(299, 11);
    puts("Thermal controller regression checks passed");
}
