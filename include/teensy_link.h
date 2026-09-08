// SHROOM Flight Software
#pragma once

#include <Arduino.h>

void teensy_link_init();
void teensy_link_update();

// Secondary only. Drop a whole frame if the UART buffer is full; never wait
// for the primary. The original message has already been logged locally.
bool teensy_link_send_airdos(uint8_t sensor_id, const char* data);
uint32_t teensy_link_get_error_count();

// Primary only: reception health for remote AIRDOS IDs 1-7.
bool teensy_link_has_received(uint8_t sensor_id);
uint32_t teensy_link_last_received_ms(uint8_t sensor_id);
uint32_t teensy_link_remote_overflows(uint8_t sensor_id);
