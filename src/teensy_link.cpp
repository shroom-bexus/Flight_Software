// SHROOM Flight Software
// One-way live forwarding over the PCB's crossed Serial1 connection.
#include "teensy_link.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
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
bool link_received = false;
uint32_t link_started_ms = 0;
uint32_t link_last_received_ms = 0;

void mark_link_received()
{
    link_received = true;
    link_last_received_ms = millis();
}

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
bool secondary_cpu_received = false;
float secondary_cpu_c = 0.0f;
uint32_t secondary_cpu_received_ms = 0;

struct EraseRequest
{
    bool pending = false;
    bool result_ready = false;
    uint16_t id = 0;
    uint32_t token = 0;
    char storage = 'I';
    const char* result = "TIMEOUT";
    uint32_t started_ms = 0;
    uint32_t sent_ms = 0;
    uint8_t attempts = 0;
};
EraseRequest erase_request;

void update_erase_request()
{
    if (!erase_request.pending) return;
    const uint32_t now = millis();
    if (now - erase_request.started_ms >= 12000 ||
        (erase_request.attempts >= 8 && now - erase_request.sent_ms >= 800))
    {
        erase_request.pending = false;
        erase_request.result_ready = true;
        erase_request.result = "TIMEOUT";
        return;
    }
    if (erase_request.attempts && now - erase_request.sent_ms < 800) return;
    char frame[32];
    const int size = snprintf(frame, sizeof(frame), "\n!C,%lu,%c\n",
        static_cast<unsigned long>(erase_request.token), erase_request.storage);
    if (size <= 0 || static_cast<size_t>(size) >= sizeof(frame) ||
        TEENSY_LINK_SERIAL.availableForWrite() < size) return;
    TEENSY_LINK_SERIAL.write(reinterpret_cast<const uint8_t*>(frame), size);
    erase_request.sent_ms = now;
    ++erase_request.attempts;
}

void receive_line()
{
    // The Secondary acknowledges each erase separately. Ignore late replies.
    if (strncmp(line, "!R,", 3) == 0)
    {
        unsigned long token = 0;
        char storage = 0, outcome = 0, extra = 0;
        if (sscanf(line, "!R,%lu,%c,%c%c", &token, &storage, &outcome, &extra) != 3 ||
            (storage != 'I' && storage != 'B') ||
            (outcome != '0' && outcome != '1'))
        {
            ++errors;
            return;
        }
        mark_link_received();
        if (erase_request.pending && erase_request.token == token &&
            erase_request.storage == storage)
        {
            erase_request.pending = false;
            erase_request.result_ready = true;
            erase_request.result = outcome == '1' ? "OK" : "FAILED";
        }
        return;
    }
    // Secondary die temperature: !T,<degrees_C>. Only complete, finite frames.
    if (strncmp(line, "!T,", 3) == 0)
    {
        char* end = nullptr;
        const float temperature_c = std::strtof(line + 3, &end);
        if (end == line + 3 || *end != '\0' ||
            !std::isfinite(temperature_c) ||
            temperature_c < -55.0f || temperature_c > 150.0f)
        {
            ++errors;
            return;
        }
        mark_link_received();
        secondary_cpu_received = true;
        secondary_cpu_c = temperature_c;
        secondary_cpu_received_ms = millis();
        // Duplicate this record to Primary internal and backup SD.
        logger_log_cpu_temperature("SECONDARY", temperature_c);
        return;
    }
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
        mark_link_received();
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
        mark_link_received();
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

    mark_link_received();
    const uint8_t id = line[3] - '0';
    received[id - 1] = true;
    last_received[id - 1] = millis();
    // Also keep a received copy on both primary SD devices. Its timestamps
    // describe primary reception, not the secondary's measurement time.
    logger_log_airdos(id, line + 5);
    telemetry_send_airdos(id, line + 5);
}
#else
// The Secondary also receives short, framed maintenance commands.
char command_line[32];
size_t command_length = 0;
bool command_discard = true;
bool ack_pending = false;
uint32_t last_erase_token = 0;
char last_erase_storage = 'I';
bool last_erase_ok = false;
bool have_last_erase = false;
uint32_t last_erase_ms = 0;

void handle_erase_command()
{
    unsigned long token = 0;
    char storage = 0, extra = 0;
    if (sscanf(command_line, "!C,%lu,%c%c", &token, &storage, &extra) != 2 ||
        (storage != 'I' && storage != 'B'))
    {
        ++errors;
        return;
    }
    // A retry after losing the ACK cannot erase newly logged samples again.
    if (!have_last_erase || token != last_erase_token ||
        storage != last_erase_storage || millis() - last_erase_ms > 15000)
    {
        last_erase_ok = logger_erase_storage(
            storage == 'I' ? LoggerStorage::INTERNAL : LoggerStorage::BACKUP);
        last_erase_token = static_cast<uint32_t>(token);
        last_erase_storage = storage;
        last_erase_ms = millis();
        have_last_erase = true;
    }
    ack_pending = true;
}

void update_secondary_erase()
{
    for (size_t count = 0; count < 64 && TEENSY_LINK_SERIAL.available(); ++count)
    {
        const char c = static_cast<char>(TEENSY_LINK_SERIAL.read());
        if (c == '\n')
        {
            if (!command_discard && command_length)
            {
                command_line[command_length] = '\0';
                handle_erase_command();
            }
            command_length = 0;
            command_discard = false;
        }
        else if (!command_discard)
        {
            if (c < 32 || c > 126 || command_length >= sizeof(command_line) - 1)
                command_discard = true;
            else command_line[command_length++] = c;
        }
    }
    if (!ack_pending) return;
    char reply[32];
    const int size = snprintf(reply, sizeof(reply), "\n!R,%lu,%c,%c\n",
        static_cast<unsigned long>(last_erase_token), last_erase_storage,
        last_erase_ok ? '1' : '0');
    if (size > 0 && static_cast<size_t>(size) < sizeof(reply) &&
        TEENSY_LINK_SERIAL.availableForWrite() >= size)
    {
        TEENSY_LINK_SERIAL.write(reinterpret_cast<const uint8_t*>(reply), size);
        ack_pending = false;
    }
}
#endif
}

void teensy_link_init()
{
    errors = 0;
#if FLIGHT_PRIMARY
    link_received = false;
    link_started_ms = millis();
    link_last_received_ms = 0;
    length = 0;
    discard = true;
    memset(received, 0, sizeof(received));
    memset(last_received, 0, sizeof(last_received));
    memset(remote_overflows, 0, sizeof(remote_overflows));
    for (auto& status : storage_status) status = StorageStatus{};
    erase_request = EraseRequest{};
    secondary_cpu_received = false;
    secondary_cpu_c = 0.0f;
    secondary_cpu_received_ms = 0;
    TEENSY_LINK_SERIAL.addMemoryForRead(uart_buffer, sizeof(uart_buffer));
#else
    command_length = 0;
    command_discard = true;
    ack_pending = false;
    have_last_erase = false;
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

bool teensy_link_send_cpu_temperature(float temperature_c)
{
#if FLIGHT_SECONDARY
    if (!std::isfinite(temperature_c) ||
        temperature_c < -55.0f || temperature_c > 150.0f) return false;
    char status[32];
    const int size = snprintf(status, sizeof(status), "\n!T,%.2f\n",
                              static_cast<double>(temperature_c));
    if (size <= 0 || static_cast<size_t>(size) >= sizeof(status) ||
        TEENSY_LINK_SERIAL.availableForWrite() < size) return false;
    TEENSY_LINK_SERIAL.write(reinterpret_cast<const uint8_t*>(status), size);
    return true;
#else
    (void)temperature_c;
    return false;
#endif
}

bool teensy_link_secondary_cpu_temperature(float* temperature_c)
{
#if FLIGHT_PRIMARY
    if (!temperature_c || !secondary_cpu_received ||
        millis() - secondary_cpu_received_ms > 3 * HEALTH_TELEMETRY_PERIOD_MS)
        return false;
    *temperature_c = secondary_cpu_c;
    return true;
#else
    (void)temperature_c;
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
    update_erase_request();
#else
    update_secondary_erase();
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

bool teensy_link_request_erase(uint16_t command_id, char storage)
{
#if FLIGHT_PRIMARY
    if ((storage != 'I' && storage != 'B') ||
        erase_request.pending || erase_request.result_ready) return false;
    erase_request = EraseRequest{};
    erase_request.pending = true;
    erase_request.id = command_id;
    erase_request.token = (millis() * 2654435761UL) ^
        (static_cast<uint32_t>(command_id) << 16) ^ 0xA5967C31UL;
    erase_request.storage = storage;
    erase_request.started_ms = millis();
    return true;
#else
    (void)command_id;
    (void)storage;
    return false;
#endif
}

bool teensy_link_get_erase_result(uint16_t& command_id, char& storage,
                                  const char*& result)
{
#if FLIGHT_PRIMARY
    if (!erase_request.result_ready) return false;
    command_id = erase_request.id;
    storage = erase_request.storage;
    result = erase_request.result;
    return true;
#else
    (void)command_id;
    (void)storage;
    (void)result;
    return false;
#endif
}

void teensy_link_clear_erase_result()
{
#if FLIGHT_PRIMARY
    erase_request.result_ready = false;
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

const char* teensy_link_state()
{
#if FLIGHT_PRIMARY
    const uint32_t age = millis() - (link_received ? link_last_received_ms : link_started_ms);
    if (age >= 3 * HEALTH_TELEMETRY_PERIOD_MS) return "FAULT";
    return link_received ? "OK" : "WAITING";
#else
    return "DISABLED";
#endif
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
