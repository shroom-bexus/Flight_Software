// SHROOM Flight Software

#include "ethernet_link.h"

#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

#include <cmath>
#include <cstring>

#include "config.h"
#include "binary_telemetry.h"


namespace
{
// Conservative UDP wire-size estimate including preamble and inter-frame gap.
constexpr size_t PACKET_OVERHEAD_BYTES = 66;
constexpr size_t MINIMUM_PACKET_BYTES = 84;
constexpr uint32_t AIRDOS_BATCH_WAIT_MS = 20;
constexpr size_t BURST_HISTORY_CAPACITY = 32;


struct TelemetryLine
{
    size_t offset = 0;
    uint16_t length = 0;
    uint8_t sensor_id = 0;
    uint32_t queued_ms = 0;
};

// Variable-length binary records share a byte ring. A short event does not
// reserve a full 384-byte text slot. Both byte and descriptor limits are bounded.
template <size_t Capacity>
class LineQueue
{
public:
    bool push(const char* message, uint8_t sensor_id = 0)
    {
        if (message == nullptr || std::strlen(message) >= ETHERNET_TELEMETRY_LINE_MAX)
            return false;
        uint8_t record[ETHERNET_TELEMETRY_LINE_MAX + 3];
        const size_t length = binary_telemetry::record(message, record, sizeof(record));
        return length && push_record(record, length, sensor_id, millis());
    }

    bool push_record(const uint8_t* record, size_t length, uint8_t sensor_id,
                     uint32_t queued_ms)
    {
        if (count_ >= Capacity || length > sizeof(bytes_) - used_) return false;
        TelemetryLine& line = lines_[tail_];
        line = {byte_tail_, static_cast<uint16_t>(length), sensor_id, queued_ms};
        for (size_t i=0; i<length; ++i)
            bytes_[(byte_tail_ + i) % sizeof(bytes_)] = record[i];
        byte_tail_ = (byte_tail_ + length) % sizeof(bytes_);
        used_ += length;
        tail_ = (tail_ + 1) % Capacity;
        ++count_;
        return true;
    }

    TelemetryLine* front() { return at(0); }
    TelemetryLine* at(size_t offset)
    {
        return offset < count_ ? &lines_[(head_ + offset) % Capacity] : nullptr;
    }
    void copy(const TelemetryLine& line, uint8_t* target) const
    {
        for (size_t i=0; i<line.length; ++i)
            target[i] = bytes_[(line.offset + i) % sizeof(bytes_)];
    }
    void pop(size_t number = 1)
    {
        while (number-- && count_)
        {
            used_ -= lines_[head_].length;
            head_ = (head_ + 1) % Capacity;
            --count_;
        }
    }
    void clear() { head_=tail_=count_=byte_tail_=used_=0; }
    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    size_t used_bytes() const { return used_; }
    size_t byte_capacity() const { return sizeof(bytes_); }
private:
    TelemetryLine lines_[Capacity];
    uint8_t bytes_[Capacity * (Capacity > 32 ? 32 : 128)];
    size_t head_=0, tail_=0, count_=0, byte_tail_=0, used_=0;
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
uint32_t next_telemetry_slot_us = 0;

struct BurstEntry
{
    uint32_t sent_us = 0;
    uint32_t bits = 0;
};
BurstEntry burst_history[BURST_HISTORY_CAPACITY];
size_t burst_head = 0;
size_t burst_count = 0;
uint32_t burst_bits = 0;

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


void reset_burst_history()
{
    burst_head = 0;
    burst_count = 0;
    burst_bits = 0;
}


void expire_burst_history(uint32_t now_us)
{
    while (burst_count > 0)
    {
        const BurstEntry& entry = burst_history[burst_head];
        if (now_us - entry.sent_us < ETHERNET_BURST_WINDOW_US) break;
        burst_bits -= entry.bits;
        burst_head = (burst_head + 1) % BURST_HISTORY_CAPACITY;
        --burst_count;
    }
}


void record_sent_packet(size_t payload_bytes)
{
    if (downlink_limit_kbit_s == 0.0f) return;

    const uint32_t now_us = micros();
    expire_burst_history(now_us);
    const uint32_t bits = static_cast<uint32_t>(
        estimated_packet_bits(payload_bytes));

    if (burst_count < BURST_HISTORY_CAPACITY)
    {
        const size_t index =
            (burst_head + burst_count) % BURST_HISTORY_CAPACITY;
        burst_history[index] = {now_us, bits};
        ++burst_count;
    }
    else
    {
        // This can only happen under a burst of immediate command replies.
        // Merge into the newest bucket and extend its lifetime. This is
        // conservative: regular telemetry may wait longer, never shorter.
        const size_t index =
            (burst_head + burst_count - 1) % BURST_HISTORY_CAPACITY;
        burst_history[index].bits += bits;
        burst_history[index].sent_us = now_us;
    }

    burst_bits += bits;
}


uint32_t burst_limit_bits()
{
    if (downlink_limit_kbit_s == 0.0f) return UINT32_MAX;
    return static_cast<uint32_t>(floorf(
        downlink_limit_kbit_s * 1000.0f *
        (static_cast<float>(ETHERNET_BURST_WINDOW_US) / 1000000.0f)
    ));
}


bool burst_allows(size_t payload_bytes)
{
    if (downlink_limit_kbit_s == 0.0f) return true;

    const uint32_t now_us = micros();
    expire_burst_history(now_us);
    const uint32_t packet_bits = static_cast<uint32_t>(
        estimated_packet_bits(payload_bytes));
    const uint32_t limit_bits = burst_limit_bits();

    // At very small configured rates, one minimum Ethernet frame can exceed
    // a 200 ms budget. Allow one only when the window is otherwise empty; the
    // long-term rate scheduler still enforces the configured average.
    if (packet_bits > limit_bits) return burst_count == 0;

    return burst_bits <= limit_bits - packet_bits;
}


size_t telemetry_packet_target()
{
    if (downlink_limit_kbit_s == 0.0f)
    {
        return ETHERNET_UDP_PAYLOAD_MAX;
    }

    const float slot_wire_bytes =
        downlink_limit_kbit_s * 1000.0f *
        static_cast<float>(ETHERNET_TELEMETRY_SLOT_US) /
        8000000.0f;

    // For very low limits a single minimum frame needs more than one slot.
    // Let the exact rate scheduler determine the spacing in that case.
    if (slot_wire_bytes < static_cast<float>(MINIMUM_PACKET_BYTES))
    {
        return ETHERNET_UDP_PAYLOAD_MAX;
    }

    const size_t wire_bytes = static_cast<size_t>(floorf(slot_wire_bytes));
    if (wire_bytes <= PACKET_OVERHEAD_BYTES)
    {
        return ETHERNET_UDP_PAYLOAD_MAX;
    }

    return min(
        ETHERNET_UDP_PAYLOAD_MAX,
        wire_bytes - PACKET_OVERHEAD_BYTES
    );
}


void reset_rate_scheduler()
{
    const uint32_t now_us = micros();
    next_regular_send_us = now_us;
    next_telemetry_slot_us = now_us;
    reset_burst_history();
}


bool regular_send_ready()
{
    if (downlink_limit_kbit_s == 0.0f) return true;
    const uint32_t now_us = micros();
    return send_time_reached(now_us, next_regular_send_us) &&
        send_time_reached(now_us, next_telemetry_slot_us);
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

    // Immediate command replies can create rate debt. Regular telemetry never
    // catches up in a burst: after any send it waits for a fresh 50 ms slot.
    next_regular_send_us = base_us + interval_us;
    next_telemetry_slot_us = now_us + ETHERNET_TELEMETRY_SLOT_US;
    record_sent_packet(payload_bytes);
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
        uint8_t record[ETHERNET_TELEMETRY_LINE_MAX + 3];
        airdos_queue.copy(line, record);
        airdos_queue.pop();

        if (airdos_sensor_selected(line.sensor_id))
        {
            // Space is guaranteed because one element was just removed.
            airdos_queue.push_record(record, line.length, line.sensor_id, line.queued_ms);
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
    const bool queue_high =
        queue_at_or_above_percent(
            airdos_queue.size(),
            ETHERNET_AIRDOS_QUEUE_DEPTH,
            AIRDOS_DOWNLINK_QUEUE_HIGH_PERCENT
        ) ||
        queue_at_or_above_percent(
            airdos_queue.used_bytes(),
            airdos_queue.byte_capacity(),
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

    const bool queue_low =
        queue_at_or_below_percent(
            airdos_queue.size(),
            ETHERNET_AIRDOS_QUEUE_DEPTH,
            AIRDOS_DOWNLINK_QUEUE_LOW_PERCENT
        ) &&
        queue_at_or_below_percent(
            airdos_queue.used_bytes(),
            airdos_queue.byte_capacity(),
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
    if (!regular_send_ready() || !burst_allows(payload_length) ||
        !begin_datagram()) return false;

    udp.write(
        reinterpret_cast<const uint8_t*>(payload),
        payload_length
    );

    if (udp.endPacket() != 1) return false;

    ++telemetry_sequence;
    charge_downlink(payload_length);
    return true;
}


template <size_t Capacity>
size_t append_records(
    LineQueue<Capacity>& queue,
    uint8_t* payload,
    size_t& length,
    size_t maximum
)
{
    size_t count = 0;
    while (count < queue.size())
    {
        const TelemetryLine& line = *queue.at(count);
        if (length + line.length > maximum) break;
        queue.copy(line, payload + length);
        length += line.length;
        ++count;
    }
    return count;
}


bool send_mixed_packet()
{
    if ((system_queue.empty() && airdos_queue.empty()) ||
        !regular_send_ready())
    {
        return false;
    }

    uint8_t payload[ETHERNET_UDP_PAYLOAD_MAX];
    binary_telemetry::header(payload, telemetry_sequence + 1);
    size_t length = 8;
    const size_t target = telemetry_packet_target();

    // Housekeeping remains first, but AIRDOS fills unused space in the same
    // datagram. This reduces overhead and avoids a burst of tiny system packets.
    size_t system_count = append_records(
        system_queue, payload, length, target);
    size_t airdos_count = 0;

    // If the first system record is larger than the slot target, send that
    // record alone. AIRDOS must never jump ahead of queued housekeeping.
    if (!system_queue.empty() && system_count == 0)
    {
        if (length + system_queue.front()->length <= sizeof(payload))
        {
            system_queue.copy(*system_queue.front(), payload + length);
            length += system_queue.front()->length;
            system_count = 1;
        }
    }
    else
    {
        airdos_count = append_records(
            airdos_queue, payload, length, target);
    }

    // A single long AIRDOS record may be larger than the 50 ms target. Send it
    // whole rather than fragmenting scientific data. Exact-rate and 200 ms
    // guards then delay following packets as required.
    if (system_count == 0 && airdos_count == 0 && system_queue.empty())
    {
        if (!airdos_queue.empty() &&
            length + airdos_queue.front()->length <= sizeof(payload))
        {
            airdos_queue.copy(*airdos_queue.front(), payload + length);
            length += airdos_queue.front()->length;
            airdos_count = 1;
        }
    }

    if (system_count == 0 && airdos_count == 0) return false;
    if (!send_regular_payload(
            reinterpret_cast<const char*>(payload), length))
    {
        return false;
    }

    system_queue.pop(system_count);
    airdos_queue.pop(airdos_count);
    return true;
}


void flush_telemetry()
{
    if (!ethernet_link_connected()) return;

    // A level reduction can make already queued AIRDOS lines ineligible.
    // Remove them before building the next mixed packet.
    while (!airdos_queue.empty())
    {
        TelemetryLine* line = airdos_queue.front();
        if (line == nullptr) return;

        if (airdos_sensor_selected(line->sensor_id)) break;

        airdos_queue.pop();
        ++airdos_suppressed_count;
    }

    // System telemetry may send on the next available slot immediately.
    if (!system_queue.empty())
    {
        send_mixed_packet();
        return;
    }

    // With AIRDOS only, keep the short batching delay so a just-arrived record
    // can share the next paced packet with neighbours from the same burst.
    TelemetryLine* line = airdos_queue.front();
    if (line != nullptr &&
        (millis() - line->queued_ms >= AIRDOS_BATCH_WAIT_MS ||
         airdos_queue.size() >= ETHERNET_AIRDOS_QUEUE_DEPTH / 2))
    {
        send_mixed_packet();
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
