// SHROOM Flight Software

#include "ethernet_link.h"

#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

#include <cmath>
#include <cstring>

#include "config.h"


namespace
{
// Conservative UDP wire-size estimate including preamble and inter-frame gap.
constexpr size_t PACKET_OVERHEAD_BYTES = 66;
constexpr size_t MINIMUM_PACKET_BYTES = 84;
constexpr size_t SEQUENCE_HEADER_MAX = 32;


struct TelemetryLine
{
    char text[ETHERNET_TELEMETRY_LINE_MAX] = {};
    uint16_t length = 0;
    uint8_t sensor_id = 0;
};


template <size_t Capacity>
class LineQueue
{
public:
    bool push(const char* message, uint8_t sensor_id = 0)
    {
        if (message == nullptr || count_ >= Capacity) return false;

        const size_t length = std::strlen(message);
        if (length >= ETHERNET_TELEMETRY_LINE_MAX) return false;

        TelemetryLine& line = lines_[tail_];
        std::memcpy(line.text, message, length + 1);
        line.length = static_cast<uint16_t>(length);
        line.sensor_id = sensor_id;

        tail_ = (tail_ + 1) % Capacity;
        ++count_;
        return true;
    }

    TelemetryLine* front()
    {
        return at(0);
    }

    TelemetryLine* at(size_t offset)
    {
        if (offset >= count_) return nullptr;
        return &lines_[(head_ + offset) % Capacity];
    }

    void pop(size_t number = 1)
    {
        if (number > count_) number = count_;
        head_ = (head_ + number) % Capacity;
        count_ -= number;
    }

    void clear()
    {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

    size_t size() const
    {
        return count_;
    }

    bool empty() const
    {
        return count_ == 0;
    }

private:
    TelemetryLine lines_[Capacity];
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};


EthernetUDP udp;

IPAddress ground_station_ip;
uint16_t ground_station_port = 0;
uint32_t last_ground_station_rx_ms = 0;
bool ground_station_known = false;

char receive_buffer[ETHERNET_RX_BUFFER_SIZE];
bool command_ready = false;

LineQueue<ETHERNET_SYSTEM_QUEUE_DEPTH> system_queue;
LineQueue<ETHERNET_AIRDOS_QUEUE_DEPTH> airdos_queue;

uint32_t telemetry_sequence = 0;
float downlink_limit_kbit_s = ETHERNET_DEFAULT_DOWNLINK_LIMIT_KBIT_S;
uint32_t next_regular_send_us = 0;

uint8_t airdos_downlink_level = AIRDOS_DOWNLINK_MAX_LEVEL;
uint32_t airdos_queue_high_since_ms = 0;
uint32_t airdos_queue_low_since_ms = 0;

uint32_t telemetry_drop_count = 0;
uint32_t airdos_suppressed_count = 0;


void reset_receive_buffer()
{
    receive_buffer[0] = '\0';
    command_ready = false;
}


void reset_telemetry_queues()
{
    system_queue.clear();
    airdos_queue.clear();
}


size_t estimated_packet_bits(size_t payload_bytes)
{
    return max(
        payload_bytes + PACKET_OVERHEAD_BYTES,
        MINIMUM_PACKET_BYTES
    ) * 8;
}


bool send_time_reached(uint32_t now_us, uint32_t target_us)
{
    return static_cast<int32_t>(now_us - target_us) >= 0;
}


void reset_rate_scheduler()
{
    next_regular_send_us = micros();
}


bool regular_send_ready()
{
    return downlink_limit_kbit_s == 0.0f ||
        send_time_reached(micros(), next_regular_send_us);
}


void charge_downlink(size_t payload_bytes)
{
    if (downlink_limit_kbit_s == 0.0f) return;

    const float rate_bits_s = downlink_limit_kbit_s * 1000.0f;
    const float interval_us_f =
        static_cast<float>(estimated_packet_bits(payload_bytes)) *
        1000000.0f / rate_bits_s;

    const uint32_t interval_us = static_cast<uint32_t>(ceilf(interval_us_f));
    const uint32_t now_us = micros();
    const uint32_t base_us = send_time_reached(now_us, next_regular_send_us)
        ? now_us
        : next_regular_send_us;

    // Priority packets are sent immediately, but their wire time is charged to
    // the same schedule. Normal telemetry therefore waits afterwards.
    next_regular_send_us = base_us + interval_us;
}


bool begin_datagram()
{
    return udp.beginPacket(ground_station_ip, ground_station_port) == 1;
}


bool send_immediate(const char* message)
{
    if (!ethernet_link_connected() || message == nullptr) return false;

    const size_t message_length = std::strlen(message);
    const size_t payload_length = message_length + 1;
    if (payload_length > ETHERNET_UDP_PAYLOAD_MAX) return false;
    if (!begin_datagram()) return false;

    udp.write(reinterpret_cast<const uint8_t*>(message), message_length);
    udp.write(static_cast<uint8_t>('\n'));

    if (udp.endPacket() != 1) return false;

    charge_downlink(payload_length);
    return true;
}


bool airdos_sensor_selected(uint8_t sensor_id)
{
    for (uint8_t group = 0; group < AIRDOS_SAMPLE_GROUP_COUNT; ++group)
    {
        for (uint8_t priority = 0;
             priority < AIRDOS_SENSORS_PER_GROUP;
             ++priority)
        {
            if (AIRDOS_DOWNLINK_PRIORITY[group][priority] != sensor_id)
            {
                continue;
            }

            return priority < airdos_downlink_level;
        }
    }

    // Unknown sensor IDs are never put on the raw-data downlink.
    return false;
}


void purge_unselected_airdos()
{
    const size_t queued = airdos_queue.size();

    for (size_t i = 0; i < queued; ++i)
    {
        TelemetryLine* front = airdos_queue.front();
        if (front == nullptr) break;

        const TelemetryLine line = *front;
        airdos_queue.pop();

        if (airdos_sensor_selected(line.sensor_id))
        {
            // Space is guaranteed because one element was just removed.
            airdos_queue.push(line.text, line.sensor_id);
        }
        else
        {
            ++airdos_suppressed_count;
        }
    }
}


void set_airdos_downlink_level(uint8_t level)
{
    if (level > AIRDOS_DOWNLINK_MAX_LEVEL)
    {
        level = AIRDOS_DOWNLINK_MAX_LEVEL;
    }

    if (level == airdos_downlink_level) return;

    airdos_downlink_level = level;
    airdos_queue_high_since_ms = 0;
    airdos_queue_low_since_ms = 0;
    purge_unselected_airdos();
}


void reduce_airdos_downlink_level()
{
    if (airdos_downlink_level == 0) return;
    set_airdos_downlink_level(airdos_downlink_level - 1);
}


bool queue_at_or_above_percent(size_t count, size_t capacity, uint8_t percent)
{
    return count * 100 >= capacity * percent;
}


bool queue_at_or_below_percent(size_t count, size_t capacity, uint8_t percent)
{
    return count * 100 <= capacity * percent;
}


void update_airdos_downlink_level()
{
    if (downlink_limit_kbit_s == 0.0f)
    {
        set_airdos_downlink_level(AIRDOS_DOWNLINK_MAX_LEVEL);
        return;
    }

    const uint32_t now_ms = millis();
    const bool queue_high = queue_at_or_above_percent(
        airdos_queue.size(),
        ETHERNET_AIRDOS_QUEUE_DEPTH,
        AIRDOS_DOWNLINK_QUEUE_HIGH_PERCENT
    );

    if (queue_high && airdos_downlink_level > 0)
    {
        airdos_queue_low_since_ms = 0;

        if (airdos_queue_high_since_ms == 0)
        {
            airdos_queue_high_since_ms = now_ms;
        }
        else if (now_ms - airdos_queue_high_since_ms >=
            AIRDOS_DOWNLINK_REDUCE_HOLD_MS)
        {
            reduce_airdos_downlink_level();
            airdos_queue_high_since_ms = now_ms;
        }

        return;
    }

    airdos_queue_high_since_ms = 0;

    const bool queue_low = queue_at_or_below_percent(
        airdos_queue.size(),
        ETHERNET_AIRDOS_QUEUE_DEPTH,
        AIRDOS_DOWNLINK_QUEUE_LOW_PERCENT
    );

    if (!queue_low || airdos_downlink_level >= AIRDOS_DOWNLINK_MAX_LEVEL)
    {
        airdos_queue_low_since_ms = 0;
        return;
    }

    if (airdos_queue_low_since_ms == 0)
    {
        airdos_queue_low_since_ms = now_ms;
        return;
    }

    if (now_ms - airdos_queue_low_since_ms >=
        AIRDOS_DOWNLINK_RESTORE_HOLD_MS)
    {
        set_airdos_downlink_level(airdos_downlink_level + 1);
        airdos_queue_low_since_ms = now_ms;
    }
}


bool send_regular_payload(const char* payload, size_t payload_length)
{
    if (payload == nullptr || payload_length == 0) return false;
    if (payload_length > ETHERNET_UDP_PAYLOAD_MAX) return false;
    if (!regular_send_ready() || !begin_datagram()) return false;

    udp.write(
        reinterpret_cast<const uint8_t*>(payload),
        payload_length
    );

    if (udp.endPacket() != 1) return false;

    ++telemetry_sequence;
    charge_downlink(payload_length);
    return true;
}


bool send_system_packet()
{
    if (system_queue.empty()) return false;

    char payload[ETHERNET_SYSTEM_PACKET_PAYLOAD_MAX];
    const uint32_t next_sequence = telemetry_sequence + 1;
    const int header_length = snprintf(
        payload,
        sizeof(payload),
        "SEQ,%lu\n",
        static_cast<unsigned long>(next_sequence)
    );
    if (header_length <= 0) return false;

    size_t payload_length = static_cast<size_t>(header_length);
    size_t lines_added = 0;

    while (lines_added < system_queue.size())
    {
        TelemetryLine* line = system_queue.at(lines_added);
        if (line == nullptr) break;

        const size_t required = line->length + 1;
        if (payload_length + required > sizeof(payload)) break;

        std::memcpy(payload + payload_length, line->text, line->length);
        payload_length += line->length;
        payload[payload_length++] = '\n';
        ++lines_added;
    }

    if (lines_added == 0) return false;
    if (!send_regular_payload(payload, payload_length)) return false;

    system_queue.pop(lines_added);
    return true;
}


bool send_airdos_packet(const TelemetryLine& line)
{
    char payload[ETHERNET_TELEMETRY_LINE_MAX + SEQUENCE_HEADER_MAX];
    const uint32_t next_sequence = telemetry_sequence + 1;
    const int header_length = snprintf(
        payload,
        sizeof(payload),
        "SEQ,%lu\n",
        static_cast<unsigned long>(next_sequence)
    );
    if (header_length <= 0) return false;

    size_t payload_length = static_cast<size_t>(header_length);
    if (payload_length + line.length + 1 > sizeof(payload)) return false;

    std::memcpy(payload + payload_length, line.text, line.length);
    payload_length += line.length;
    payload[payload_length++] = '\n';

    return send_regular_payload(payload, payload_length);
}


void flush_telemetry()
{
    if (!ethernet_link_connected()) return;

    // Housekeeping/system telemetry always has priority over AIRDOS raw data.
    if (!system_queue.empty())
    {
        send_system_packet();
        return;
    }

    // A level reduction can make already queued AIRDOS lines ineligible.
    // Remove them before selecting the next line to transmit.
    while (!airdos_queue.empty())
    {
        TelemetryLine* line = airdos_queue.front();
        if (line == nullptr) return;

        if (airdos_sensor_selected(line->sensor_id)) break;

        airdos_queue.pop();
        ++airdos_suppressed_count;
    }

    TelemetryLine* line = airdos_queue.front();
    if (line != nullptr && send_airdos_packet(*line))
    {
        airdos_queue.pop();
    }
}


void get_teensy_mac(uint8_t* mac)
{
    // Teensy 4.1 stores its unique factory MAC in the IMXRT1062 fuses.
    for (uint8_t i = 0; i < 2; ++i)
    {
        mac[i] = (HW_OCOTP_MAC1 >> ((1 - i) * 8)) & 0xFF;
    }

    for (uint8_t i = 0; i < 4; ++i)
    {
        mac[i + 2] = (HW_OCOTP_MAC0 >> ((3 - i) * 8)) & 0xFF;
    }
}
} // namespace


bool ethernet_link_init()
{
    uint8_t mac[6];
    get_teensy_mac(mac);

    Serial.printf(
        "Ethernet MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );

    const IPAddress local_ip(
        ETHERNET_LOCAL_IP[0],
        ETHERNET_LOCAL_IP[1],
        ETHERNET_LOCAL_IP[2],
        ETHERNET_LOCAL_IP[3]
    );
    const IPAddress dns(
        ETHERNET_DNS[0],
        ETHERNET_DNS[1],
        ETHERNET_DNS[2],
        ETHERNET_DNS[3]
    );
    const IPAddress gateway(
        ETHERNET_GATEWAY[0],
        ETHERNET_GATEWAY[1],
        ETHERNET_GATEWAY[2],
        ETHERNET_GATEWAY[3]
    );
    const IPAddress subnet(
        ETHERNET_SUBNET[0],
        ETHERNET_SUBNET[1],
        ETHERNET_SUBNET[2],
        ETHERNET_SUBNET[3]
    );

    Ethernet.begin(mac, local_ip, dns, gateway, subnet);

    reset_receive_buffer();
    reset_telemetry_queues();
    reset_rate_scheduler();
    set_airdos_downlink_level(AIRDOS_DOWNLINK_MAX_LEVEL);

    telemetry_sequence = 0;
    telemetry_drop_count = 0;
    airdos_suppressed_count = 0;

    return udp.begin(ETHERNET_UDP_PORT) == 1;
}


void ethernet_link_update()
{
    if (ground_station_known &&
        millis() - last_ground_station_rx_ms >
            ETHERNET_GROUND_STATION_TIMEOUT_MS)
    {
        ground_station_known = false;
        reset_telemetry_queues();
    }

    if (!command_ready)
    {
        const int packet_size = udp.parsePacket();
        if (packet_size > 0)
        {
            const int bytes_read = udp.read(
                reinterpret_cast<uint8_t*>(receive_buffer),
                sizeof(receive_buffer) - 1
            );

            // Drain oversized datagrams and reject their truncated content.
            while (udp.available()) udp.read();

            if (bytes_read > 0 &&
                packet_size < static_cast<int>(sizeof(receive_buffer)))
            {
                size_t length = static_cast<size_t>(bytes_read);
                while (length > 0 &&
                    (receive_buffer[length - 1] == '\n' ||
                     receive_buffer[length - 1] == '\r'))
                {
                    --length;
                }
                receive_buffer[length] = '\0';

                if (length > 0)
                {
                    ground_station_ip = udp.remoteIP();
                    ground_station_port = udp.remotePort();
                    last_ground_station_rx_ms = millis();
                    ground_station_known = true;
                    command_ready = true;
                }
            }
        }
    }

    update_airdos_downlink_level();
    flush_telemetry();
}


bool ethernet_link_connected()
{
    return ground_station_known &&
        millis() - last_ground_station_rx_ms <=
            ETHERNET_GROUND_STATION_TIMEOUT_MS;
}


bool ethernet_link_read_line(char* buffer, size_t buffer_size)
{
    if (!command_ready || buffer == nullptr || buffer_size == 0) return false;

    std::strncpy(buffer, receive_buffer, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    reset_receive_buffer();
    return true;
}


bool ethernet_link_send_line(const char* message)
{
    if (!ethernet_link_connected() || message == nullptr) return false;

    if (system_queue.push(message)) return true;

    ++telemetry_drop_count;

    // If system telemetry cannot be queued, remove AIRDOS load immediately.
    if (downlink_limit_kbit_s != 0.0f)
    {
        reduce_airdos_downlink_level();
    }

    return false;
}


bool ethernet_link_send_airdos_line(uint8_t sensor_id, const char* message)
{
    if (!ethernet_link_connected() || message == nullptr) return false;

    if (!airdos_sensor_selected(sensor_id))
    {
        ++airdos_suppressed_count;
        return true;
    }

    if (airdos_queue.push(message, sensor_id)) return true;

    // Queue pressure reached the hard limit before the hysteresis timer could
    // react. Drop one complete priority level immediately and retry this line
    // if the sensor is still selected afterwards.
    if (downlink_limit_kbit_s != 0.0f && airdos_downlink_level > 0)
    {
        reduce_airdos_downlink_level();

        if (!airdos_sensor_selected(sensor_id))
        {
            ++airdos_suppressed_count;
            return true;
        }

        if (airdos_queue.push(message, sensor_id)) return true;
    }

    ++telemetry_drop_count;
    return false;
}


bool ethernet_link_send_priority_line(const char* message)
{
    return send_immediate(message);
}


bool ethernet_link_set_downlink_limit(float limit_kbit_s)
{
    const bool unlimited = limit_kbit_s == 0.0f;
    const bool within_range =
        limit_kbit_s >= ETHERNET_MIN_DOWNLINK_LIMIT_KBIT_S &&
        limit_kbit_s <= ETHERNET_MAX_DOWNLINK_LIMIT_KBIT_S;

    if (!std::isfinite(limit_kbit_s) || (!unlimited && !within_range))
    {
        return false;
    }

    // The ground station periodically refreshes its saved setting. Do not
    // reset the automatic AIRDOS level when the requested limit is unchanged.
    if (limit_kbit_s == downlink_limit_kbit_s) return true;

    downlink_limit_kbit_s = limit_kbit_s;
    reset_rate_scheduler();

    // Re-evaluate the full science downlink after an actual operator change.
    // Queue pressure will step it down again if needed.
    set_airdos_downlink_level(AIRDOS_DOWNLINK_MAX_LEVEL);
    return true;
}


float ethernet_link_get_downlink_limit()
{
    return downlink_limit_kbit_s;
}


uint8_t ethernet_link_get_airdos_downlink_level()
{
    return airdos_downlink_level;
}


uint8_t ethernet_link_get_airdos_selected_count()
{
    return airdos_downlink_level * AIRDOS_SAMPLE_GROUP_COUNT;
}


uint32_t ethernet_link_get_telemetry_drop_count()
{
    return telemetry_drop_count;
}


uint32_t ethernet_link_get_airdos_suppressed_count()
{
    return airdos_suppressed_count;
}


uint16_t ethernet_link_get_system_queue_size()
{
    return static_cast<uint16_t>(system_queue.size());
}


uint16_t ethernet_link_get_airdos_queue_size()
{
    return static_cast<uint16_t>(airdos_queue.size());
}
