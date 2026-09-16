// SHROOM Flight Software
// One-way live forwarding over the PCB's crossed Serial1 connection.
#include "teensy_link.h"

#include <cstring>
#include <cstdio>
#include "airdos.h"
#include "config.h"
#include "logger.h"
#include "telemetry.h"

namespace
{
uint32_t errors = 0;
uint8_t uart_buffer[TEENSY_LINK_BUFFER_SIZE];

#if FLIGHT_PRIMARY
char line[AIRDOS_LINE_BUFFER_SIZE + 8];
size_t length = 0;
bool discard = true; // A reset may occur in the middle of an incoming frame.
bool received[7] = {};
uint32_t last_received[7] = {};
uint32_t remote_overflows[7] = {};
struct StorageStatus {
    bool received = false;
    uint8_t state = 0; // 0 disabled, 1 OK, 2 fault
    uint32_t errors = 0;
    uint32_t received_ms = 0;
};
StorageStatus storage_status[2];

void receive_line()
{
    // Storage frame: !S,<0 internal|1 backup>,<0 disabled|1 OK|2 fault>,<errors>.
    if (strncmp(line, "!S,", 3) == 0)
    {
        if (length < 8 || line[3] < '0' || line[3] > '1' ||
            line[4] != ',' || line[5] < '0' || line[5] > '2' || line[6] != ',')
        {
            ++errors;
            return;
        }
        uint32_t count = 0;
        for (size_t i = 7; i < length; ++i)
        {
            if (line[i] < '0' || line[i] > '9' ||
                count > (UINT32_MAX - (line[i] - '0')) / 10)
            {
                ++errors;
                return;
            }
            count = count * 10 + (line[i] - '0');
        }
        storage_status[line[3] - '0'] = {true, static_cast<uint8_t>(line[5] - '0'), count, millis()};
        return;
    }
    // Periodic source parser-overflow count; no measurement timestamp is
    // inferred from this status message.
    if (length >= 6 && strncmp(line, "!O,", 3) == 0 &&
        line[3] >= '1' && line[3] <= '7' && line[4] == ',')
    {
        uint32_t value = 0;
        for (size_t i = 5; i < length; ++i)
        {
            if (line[i] < '0' || line[i] > '9' ||
                value > (UINT32_MAX - (line[i] - '0')) / 10)
            {
                ++errors;
                return;
            }
            value = value * 10 + (line[i] - '0');
        }
        remote_overflows[line[3] - '1'] = value;
        return;
    }
    // Wire format: !A,<1-7>,<original AIRDOS line>\n
    // Only the fixed prefix is parsed; commas in the payload stay untouched.
    if (length < 7 || strncmp(line, "!A,", 3) != 0 ||
        line[3] < '1' || line[3] > '7' || line[4] != ',' ||
        line[5] != '$' || length - 5 >= AIRDOS_LINE_BUFFER_SIZE)
    {
        ++errors;
        return;
    }

    const uint8_t id = line[3] - '0';
    received[id - 1] = true;
    last_received[id - 1] = millis();
    // Also keep a received copy on both primary SD devices. Its timestamps
    // describe primary reception, not the secondary's measurement time.
    logger_log_airdos(id, line + 5);
    telemetry_send_airdos(id, line + 5);
}
#endif
}

void teensy_link_init()
{
    errors = 0;
#if FLIGHT_PRIMARY
    length = 0;
    discard = true;
    memset(received, 0, sizeof(received));
    memset(last_received, 0, sizeof(last_received));
    memset(remote_overflows, 0, sizeof(remote_overflows));
    for (auto& status : storage_status) status = StorageStatus{};
    TEENSY_LINK_SERIAL.addMemoryForRead(uart_buffer, sizeof(uart_buffer));
#else
    TEENSY_LINK_SERIAL.addMemoryForWrite(uart_buffer, sizeof(uart_buffer));
#endif
    TEENSY_LINK_SERIAL.begin(TEENSY_LINK_BAUD_RATE);
}

bool teensy_link_send_airdos(uint8_t sensor_id, const char* data)
{
#if FLIGHT_SECONDARY
    if (sensor_id < 1 || sensor_id > 7 || data == nullptr || data[0] != '$')
    {
        ++errors;
        return false;
    }
    const size_t size = strlen(data);
    // Leading newline resynchronizes after either board resets or a frame
    // is interrupted. Never enqueue a partial frame when the buffer is full.
    if (size < 2 || size >= AIRDOS_LINE_BUFFER_SIZE ||
        strchr(data, '\n') || strchr(data, '\r') ||
        TEENSY_LINK_SERIAL.availableForWrite() < static_cast<int>(size + 7))
    {
        ++errors;
        return false;
    }
    const char prefix[] = {'\n', '!', 'A', ',', char('0' + sensor_id), ','};
    TEENSY_LINK_SERIAL.write(reinterpret_cast<const uint8_t*>(prefix), sizeof(prefix));
    TEENSY_LINK_SERIAL.write(reinterpret_cast<const uint8_t*>(data), size);
    TEENSY_LINK_SERIAL.write('\n');
    return true;
#else
    (void)sensor_id;
    (void)data;
    return false;
#endif
}

void teensy_link_update()
{
#if FLIGHT_PRIMARY
    // Bound work per visit even when the secondary sends continuously.
    for (size_t bytes = 0; bytes < 4096 && TEENSY_LINK_SERIAL.available(); ++bytes)
    {
        const char c = static_cast<char>(TEENSY_LINK_SERIAL.read());
        if (c == '\n')
        {
            if (!discard && length != 0)
            {
                line[length] = '\0';
                receive_line();
            }
            length = 0;
            discard = false;
        }
        else if (!discard)
        {
            if (c < 32 || c > 126 || length >= sizeof(line) - 1)
            {
                ++errors;
                discard = true;
            }
            else line[length++] = c;
        }
    }
#else
    static uint32_t last_status = 0;
    if (millis() - last_status >= HEALTH_TELEMETRY_PERIOD_MS)
    {
        last_status = millis();
        // Send storage before the overflow counters. Retry on the next period
        // if the UART queue is full; never block local measurement logging.
        for (uint8_t storage = 0; storage < 2; ++storage)
        {
            const bool enabled = storage == 0 ? ENABLE_SD_LOGGING : ENABLE_BACKUP_SD_LOGGING;
            const bool ready = storage == 0 ? logger_internal_sd_is_ready() : logger_backup_sd_is_ready();
            const uint32_t count = storage == 0 ? logger_get_internal_sd_error_count() : logger_get_backup_sd_error_count();
            char status[32];
            const int size = snprintf(status, sizeof(status), "\n!S,%u,%u,%lu\n",
                storage, enabled ? (ready ? 1 : 2) : 0, static_cast<unsigned long>(count));
            if (size > 0 && static_cast<size_t>(size) < sizeof(status) &&
                TEENSY_LINK_SERIAL.availableForWrite() >= size)
                TEENSY_LINK_SERIAL.write(reinterpret_cast<const uint8_t*>(status), size);
        }
        for (uint8_t i = 0; i < AIRDOS_CHANNEL_COUNT; ++i)
        {
            char status[32];
            const int size = snprintf(status, sizeof(status), "\n!O,%u,%lu\n",
                airdos_get_sensor_id(i),
                static_cast<unsigned long>(airdos_get_overflow_count(i)));
            if (TEENSY_LINK_SERIAL.availableForWrite() >= size)
                TEENSY_LINK_SERIAL.write(reinterpret_cast<const uint8_t*>(status), size);
        }
    }
    // UART interrupts drain the transmit buffer. Report local losses via USB
    // without making logging depend on a connected terminal.
    static uint32_t reported_errors = 0;
    static uint32_t last_report = 0;
    if (errors != reported_errors && millis() - last_report >= 5000)
    {
        if (Serial && Serial.availableForWrite() >= 64)
        {
            Serial.print("Secondary UART forwarding drops: ");
            Serial.println(errors);
            reported_errors = errors;
        }
        last_report = millis();
    }
#endif
}

uint32_t teensy_link_get_error_count() { return errors; }

bool teensy_link_has_received(uint8_t sensor_id)
{
#if FLIGHT_PRIMARY
    return sensor_id >= 1 && sensor_id <= 7 && received[sensor_id - 1];
#else
    (void)sensor_id;
    return false;
#endif
}

uint32_t teensy_link_last_received_ms(uint8_t sensor_id)
{
#if FLIGHT_PRIMARY
    if (sensor_id >= 1 && sensor_id <= 7) return last_received[sensor_id - 1];
#else
    (void)sensor_id;
#endif
    return 0;
}

uint32_t teensy_link_remote_overflows(uint8_t sensor_id)
{
#if FLIGHT_PRIMARY
    if (sensor_id >= 1 && sensor_id <= 7) return remote_overflows[sensor_id - 1];
#else
    (void)sensor_id;
#endif
    return 0;
}

const char* teensy_link_storage_state(uint8_t storage)
{
#if FLIGHT_PRIMARY
    if (storage >= 2) return "WAITING";
    const auto& status = storage_status[storage];
    if (!status.received)
        return millis() > 3 * HEALTH_TELEMETRY_PERIOD_MS ? "STALE" : "WAITING";
    if (millis() - status.received_ms > 3 * HEALTH_TELEMETRY_PERIOD_MS) return "STALE";
    return status.state == 0 ? "DISABLED" : (status.state == 1 ? "OK" : "FAULT");
#else
    (void)storage;
    return "WAITING";
#endif
}

uint32_t teensy_link_storage_errors(uint8_t storage)
{
#if FLIGHT_PRIMARY
    if (storage < 2) return storage_status[storage].errors;
#else
    (void)storage;
#endif
    return 0;
}
