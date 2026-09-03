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
constexpr float TOKEN_CAPACITY_SECONDS = 0.6f;
constexpr size_t SEQUENCE_HEADER_MAX = 32;

EthernetUDP udp;

IPAddress ground_station_ip;
uint16_t ground_station_port = 0;
uint32_t last_ground_station_rx_ms = 0;
bool ground_station_known = false;

char receive_buffer[ETHERNET_RX_BUFFER_SIZE];
bool command_ready = false;

char telemetry_buffer[ETHERNET_UDP_PAYLOAD_MAX];
size_t telemetry_length = 0;
uint32_t telemetry_sequence = 0;
uint32_t last_telemetry_send_ms = 0;

float downlink_limit_kbit_s = ETHERNET_DEFAULT_DOWNLINK_LIMIT_KBIT_S;

struct TokenBucket
{
    float tokens_bits = 0.0f;
    float rate_bits_s = 0.0f;
    float capacity_bits = 0.0f;
    uint32_t last_refill_us = 0;
};

TokenBucket downlink_bucket;


void reset_receive_buffer()
{
    receive_buffer[0] = '\0';
    command_ready = false;
}


void reset_telemetry_buffer()
{
    telemetry_length = 0;
    telemetry_buffer[0] = '\0';
}


void reset_downlink_bucket()
{
    const float rate_bits_s = downlink_limit_kbit_s * 1000.0f;
    downlink_bucket.rate_bits_s = rate_bits_s;
    downlink_bucket.capacity_bits = rate_bits_s * TOKEN_CAPACITY_SECONDS;
    downlink_bucket.tokens_bits = downlink_bucket.capacity_bits;
    downlink_bucket.last_refill_us = micros();
}


void refill_downlink_bucket(uint32_t now_us)
{
    const uint32_t elapsed_us = now_us - downlink_bucket.last_refill_us;
    downlink_bucket.last_refill_us = now_us;
    downlink_bucket.tokens_bits = min(
        downlink_bucket.capacity_bits,
        downlink_bucket.tokens_bits +
            downlink_bucket.rate_bits_s * static_cast<float>(elapsed_us) /
                1000000.0f
    );
}


size_t estimated_packet_bits(size_t payload_bytes)
{
    return max(
        payload_bytes + PACKET_OVERHEAD_BYTES,
        MINIMUM_PACKET_BYTES
    ) * 8;
}


bool reserve_downlink(size_t payload_bytes, bool priority)
{
    if (downlink_limit_kbit_s == 0.0f) return true;

    refill_downlink_bucket(micros());

    // A batch may exceed the bucket capacity. Send one packet and carry its
    // debt forward so the long-term rate remains limited.
    if (!priority && downlink_bucket.tokens_bits < 0.0f) return false;

    downlink_bucket.tokens_bits -= static_cast<float>(
        estimated_packet_bits(payload_bytes)
    );
    return true;
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
    if (!reserve_downlink(payload_length, true)) return false;
    if (!begin_datagram()) return false;

    udp.write(reinterpret_cast<const uint8_t*>(message), message_length);
    udp.write(static_cast<uint8_t>('\n'));
    return udp.endPacket() == 1;
}


void flush_telemetry()
{
    if (telemetry_length == 0 || !ethernet_link_connected()) return;

    const uint32_t now_ms = millis();
    if (now_ms - last_telemetry_send_ms <
        ETHERNET_TELEMETRY_BATCH_PERIOD_MS)
    {
        return;
    }

    char sequence_header[32];
    const uint32_t next_sequence = telemetry_sequence + 1;
    const int header_length = snprintf(
        sequence_header,
        sizeof(sequence_header),
        "SEQ,%lu\n",
        static_cast<unsigned long>(next_sequence)
    );
    if (header_length <= 0) return;

    const size_t payload_length =
        static_cast<size_t>(header_length) + telemetry_length;
    if (payload_length > ETHERNET_UDP_PAYLOAD_MAX ||
        !reserve_downlink(payload_length, false) ||
        !begin_datagram())
    {
        return;
    }

    udp.write(
        reinterpret_cast<const uint8_t*>(sequence_header),
        static_cast<size_t>(header_length)
    );
    udp.write(
        reinterpret_cast<const uint8_t*>(telemetry_buffer),
        telemetry_length
    );

    if (udp.endPacket() == 1)
    {
        telemetry_sequence = next_sequence;
        last_telemetry_send_ms = now_ms;
        reset_telemetry_buffer();
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
    reset_telemetry_buffer();
    reset_downlink_bucket();
    last_telemetry_send_ms = millis();

    return udp.begin(ETHERNET_UDP_PORT) == 1;
}


void ethernet_link_update()
{
    if (ground_station_known &&
        millis() - last_ground_station_rx_ms >
            ETHERNET_GROUND_STATION_TIMEOUT_MS)
    {
        ground_station_known = false;
        reset_telemetry_buffer();
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

    const size_t message_length = std::strlen(message);
    const size_t required = message_length + 1;
    if (required >
        ETHERNET_UDP_PAYLOAD_MAX - SEQUENCE_HEADER_MAX - telemetry_length)
    {
        return false;
    }

    std::memcpy(telemetry_buffer + telemetry_length, message, message_length);
    telemetry_length += message_length;
    telemetry_buffer[telemetry_length++] = '\n';
    return true;
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

    downlink_limit_kbit_s = limit_kbit_s;
    reset_downlink_bucket();
    return true;
}


float ethernet_link_get_downlink_limit()
{
    return downlink_limit_kbit_s;
}
