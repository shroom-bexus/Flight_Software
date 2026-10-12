// SHROOM Flight Software
#pragma once

#include <Arduino.h>

void teensy_link_init();
void teensy_link_update();

// Secondary only. Drop a whole frame if the UART buffer is full; never wait
// for the primary. The original message has already been logged locally.
bool teensy_link_send_airdos(uint8_t sensor_id, const char* data);
// Secondary only: send one non-blocking on-die temperature status frame.
bool teensy_link_send_cpu_temperature(float temperature_c);
// Primary only: get the latest valid Secondary reading, fresh within 15 s.
bool teensy_link_secondary_cpu_temperature(float* temperature_c);
uint32_t teensy_link_get_error_count();

// Primary only: send a remote erase request to Secondary; one outstanding.
bool teensy_link_request_erase(uint16_t command_id, char storage);
// Result retained until explicitly cleared following a successful GS send.
bool teensy_link_get_erase_result(uint16_t& command_id, char& storage,
                                 const char*& result);
void teensy_link_clear_erase_result();

// Primary only: reception health for remote AIRDOS IDs 1-7.
bool teensy_link_has_received(uint8_t sensor_id);
uint32_t teensy_link_last_received_ms(uint8_t sensor_id);
uint32_t teensy_link_remote_overflows(uint8_t sensor_id);

// Primary UART liveness; valid AIRDOS or status frames prove activity.
const char* teensy_link_state();

// Secondary storage: index 0 = internal SD, 1 = backup XTSD.
// WAITING before first report, STALE after three missed health intervals.
const char* teensy_link_storage_state(uint8_t storage);
uint32_t teensy_link_storage_errors(uint8_t storage);
