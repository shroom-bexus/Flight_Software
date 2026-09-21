#include "Arduino.h"
#include "airdos.h"
#include "teensy_link.h"
#include "config.h"
#include <iostream>
#include <vector>
#include <utility>

std::vector<std::pair<uint8_t, std::string>> logged, downlinked;
void logger_log_airdos(uint8_t id, const char* data) { logged.emplace_back(id, data); }
void telemetry_send_airdos(uint8_t id, const char* data) { downlinked.emplace_back(id, data); }

bool logger_internal_sd_is_ready() { return true; }
bool logger_backup_sd_is_ready() { return false; }
uint32_t logger_get_internal_sd_error_count() { return 0; }
uint32_t logger_get_backup_sd_error_count() { return 3; }

int main() {
    teensy_link_init();
    airdos_init();
    assert(Serial1.baud == 2000000);
#if FLIGHT_PRIMARY
    assert(std::string(teensy_link_state()) == "WAITING");
    fake_time = 3 * HEALTH_TELEMETRY_PERIOD_MS;
    assert(std::string(teensy_link_state()) == "FAULT");
    Serial1.inject("\n!S,0,1,0\n");
    teensy_link_update();
    assert(std::string(teensy_link_state()) == "OK");
    fake_time += 3 * HEALTH_TELEMETRY_PERIOD_MS;
    assert(std::string(teensy_link_state()) == "FAULT");
    Serial1.inject("!S,0,1,bad\n");
    teensy_link_update();
    assert(std::string(teensy_link_state()) == "FAULT");
    Serial1.inject("!O,1,0\n");
    teensy_link_update();
    assert(std::string(teensy_link_state()) == "OK");
    // Unsigned subtraction must handle the 32-bit millis rollover.
    fake_time = UINT32_MAX - 5000;
    teensy_link_init();
    Serial1.inject("\n!S,0,1,0\n");
    teensy_link_update();
    fake_time = 1000;
    assert(std::string(teensy_link_state()) == "OK");
    fake_time = 15000;
    assert(std::string(teensy_link_state()) == "FAULT");
    fake_time = 0;
    teensy_link_init();
    assert(std::string(teensy_link_storage_state(0)) == "WAITING");
    // A primary reboot midway through a frame must not accept its tail.
    Serial1.inject("!A,1,$E,cut\n\n!A,1,$E,10,20\n");
    fake_time = 123;
    teensy_link_update();
    assert(logged.size() == 1 && logged == downlinked);
    assert(logged[0].first == 1 && logged[0].second == "$E,10,20");
    assert(teensy_link_last_received_ms(1) == 123);
    assert(!teensy_link_has_received(0) && !teensy_link_has_received(8));
    // Fragmented frame and every remote ID.
    Serial1.inject("\n!A,7,$ENV,1");
    teensy_link_update();
    assert(logged.size() == 1);
    Serial1.inject(",2\n");
    teensy_link_update();
    assert(logged.back().second == "$ENV,1,2");
    for (uint8_t id = 1; id <= 7; ++id) {
        Serial1.inject("\n!A," + std::to_string(id) + ",$E,ok\n");
        teensy_link_update();
        assert(logged.back().first == id);
    }
    auto before = logged.size();
    Serial1.inject("\n!A,0,$E,bad\n!A,8,$E,bad\n!A,1,garbage\n");
    Serial1.inject("!A,1,$" + std::string(300, 'x') + "\n");
    Serial1.inject(std::string("!A,1,$E,") + char(0) + "tail\n");
    teensy_link_update();
    assert(logged.size() == before && teensy_link_get_error_count() == 5);
    // Largest accepted source line and recovery after malformed input.
    const std::string maximum = "$" + std::string(254, 'x');
    Serial1.inject("\n!A,2," + maximum + "\n");
    teensy_link_update();
    assert(logged.back().second == maximum && logged == downlinked);
    Serial1.inject("\n!O,2,4294967295\n!O,2,4294967296\n!O,2,-1\n");
    teensy_link_update();
    assert(teensy_link_remote_overflows(2) == UINT32_MAX);
    assert(teensy_link_get_error_count() == 7);
    assert(teensy_link_remote_overflows(0) == 0);
    // Independent storage state, fragmented input and strict numeric validation.
    Serial1.inject("\n!S,0,1,0\n!S,1,2,");
    teensy_link_update();
    assert(std::string(teensy_link_storage_state(0)) == "OK");
    assert(std::string(teensy_link_storage_state(1)) == "WAITING");
    Serial1.inject("3\n");
    teensy_link_update();
    assert(std::string(teensy_link_storage_state(1)) == "FAULT");
    assert(teensy_link_storage_errors(1) == 3);
    Serial1.inject("!S,1,2,4294967296\n!S,2,1,0\n!S,1,3,0\n!S,1,1,-1\n!S,1,1,\n");
    teensy_link_update();
    assert(teensy_link_get_error_count() == 12);
    assert(teensy_link_storage_errors(1) == 3);
    fake_time += 3 * HEALTH_TELEMETRY_PERIOD_MS + 1;
    assert(std::string(teensy_link_storage_state(0)) == "STALE");
    assert(std::string(teensy_link_storage_state(1)) == "STALE");
    Serial1.inject("!S,1,1,0\n!S,0,0,0\n"); // Secondary reboot / disabled card.
    teensy_link_update();
    assert(std::string(teensy_link_storage_state(1)) == "OK");
    assert(teensy_link_storage_errors(1) == 0);
    assert(std::string(teensy_link_storage_state(0)) == "DISABLED");
    teensy_link_init();
    assert(std::string(teensy_link_storage_state(1)) == "STALE");
    fake_time = 0;
    assert(std::string(teensy_link_storage_state(1)) == "WAITING");
#else
    // Check physical serial mapping and preservation of comma-rich payloads.
    HardwareSerialIMXRT* inputs[] = {&Serial2, &Serial3, &Serial4, &Serial5,
        &Serial6, &Serial7, &Serial8};
    for (uint8_t i = 0; i < 7; ++i) {
        assert(inputs[i]->baud == 115200);
        inputs[i]->inject("$E,1,2,3\r\n");
        assert(airdos_update(i));
        assert(airdos_get_sensor_id(i) == i + 1);
        assert(teensy_link_send_airdos(i + 1, airdos_get_data(i)));
        assert(Serial1.tx.find("\n!A," + std::to_string(i + 1) + ",$E,1,2,3\n") != std::string::npos);
    }
    auto before = Serial1.tx;
    Serial1.room = 3;
    assert(!teensy_link_send_airdos(1, "$E,full"));
    assert(Serial1.tx == before && teensy_link_get_error_count() == 1);
    Serial1.room = 32768;
    assert(!teensy_link_send_airdos(0, "$E,bad"));
    assert(!teensy_link_send_airdos(1, "$E,embedded\nnewline"));
    assert(!teensy_link_send_airdos(1, nullptr));
    assert(teensy_link_send_airdos(7, "$E,recovered"));
    // Source overflow discards the whole line, then recovers at newline.
    Serial2.inject("$" + std::string(300, 'x') + "\n$E,next\n");
    assert(!airdos_update(0));
    assert(airdos_get_overflow_count(0) == 1);
    assert(airdos_update(0));
    assert(std::string(airdos_get_data(0)) == "$E,next");
    fake_time = 5000;
    teensy_link_update();
    assert(Serial1.tx.find("\n!O,1,1\n") != std::string::npos);
    assert(Serial1.tx.find("\n!S,0,1,0\n") != std::string::npos);
    assert(Serial1.tx.find("\n!S,1,2,3\n") != std::string::npos);
    before = Serial1.tx;
    Serial1.room = 1;
    fake_time += HEALTH_TELEMETRY_PERIOD_MS;
    teensy_link_update();
    assert(Serial1.tx == before); // Status cannot block when TX is full.
#endif
    std::cout << (FLIGHT_PRIMARY ? "primary" : "secondary") << " host tests passed\n";
}
