// SHROOM Flight Software

#include "ethernet_link.h"

#include <NativeEthernet.h>

#include <cmath>
#include <cstring>

#include "config.h"


namespace
{
    // One TCP packet also carries Ethernet, IP, and TCP overhead. Including
    // preamble and inter-frame gap makes this estimate deliberately safe.
    constexpr size_t PACKET_OVERHEAD_BYTES = 78;
    constexpr size_t MINIMUM_PACKET_BYTES = 84;

    // With continuous refill, 0.6 seconds of tokens permits at most 0.8
    // seconds of data in any 200 ms interval (the BEXUS 4x rule of thumb).
    constexpr float TOKEN_CAPACITY_SECONDS = 0.6f;
    constexpr float TELEMETRY_RATE_SHARE = 0.8f;

    // TCP server

    EthernetServer server(ETHERNET_TCP_PORT);
    EthernetClient client;


    // Receive buffer

    char receive_buffer[ETHERNET_RX_BUFFER_SIZE];

    size_t receive_length = 0;
    bool line_ready = false;


    // Downlink rate limiting

    float downlink_limit_kbit_s =
        ETHERNET_DEFAULT_DOWNLINK_LIMIT_KBIT_S;

    struct TokenBucket
    {
        float tokens_bits = 0.0f;
        float rate_bits_s = 0.0f;
        float capacity_bits = 0.0f;
        uint32_t last_refill_us = 0;
    };

    TokenBucket total_bucket;
    TokenBucket telemetry_bucket;

    char transmit_buffer[ETHERNET_TX_BUFFER_SIZE];


    // Helper functions

    void reset_receive_buffer()
    {
        receive_length = 0;
        line_ready = false;
    }


    void configure_bucket(TokenBucket& bucket, float rate_bits_s)
    {
        bucket.rate_bits_s = rate_bits_s;
        bucket.capacity_bits = rate_bits_s * TOKEN_CAPACITY_SECONDS;
        bucket.tokens_bits = bucket.capacity_bits;
        bucket.last_refill_us = micros();
    }


    void reset_downlink_buckets()
    {
        const float rate_bits_s = downlink_limit_kbit_s * 1000.0f;
        configure_bucket(total_bucket, rate_bits_s);
        configure_bucket(
            telemetry_bucket,
            rate_bits_s * TELEMETRY_RATE_SHARE
        );
    }


    void refill_bucket(TokenBucket& bucket, uint32_t now_us)
    {
        const uint32_t elapsed_us = now_us - bucket.last_refill_us;
        bucket.last_refill_us = now_us;
        bucket.tokens_bits = min(
            bucket.capacity_bits,
            bucket.tokens_bits +
                bucket.rate_bits_s * static_cast<float>(elapsed_us) / 1000000.0f
        );
    }


    size_t estimated_packet_bits(size_t payload_bytes)
    {
        const size_t packet_bytes = max(
            payload_bytes + PACKET_OVERHEAD_BYTES,
            MINIMUM_PACKET_BYTES
        );
        return packet_bytes * 8;
    }


    bool send_line(const char* message, bool priority)
    {
        if (!ethernet_link_connected() || message == nullptr)
        {
            return false;
        }

        const size_t message_length = std::strlen(message);
        const size_t payload_bytes = message_length + 1;
        if (payload_bytes > sizeof(transmit_buffer))
        {
            return false;
        }

        const float packet_bits = static_cast<float>(
            estimated_packet_bits(payload_bytes)
        );

        if (downlink_limit_kbit_s > 0.0f)
        {
            const uint32_t now_us = micros();
            refill_bucket(total_bucket, now_us);
            refill_bucket(telemetry_bucket, now_us);

            if (!priority &&
                (packet_bits > total_bucket.tokens_bits ||
                 packet_bits > telemetry_bucket.tokens_bits))
            {
                return false;
            }

            // Replies are rare and must not disappear. If necessary they
            // borrow future capacity; telemetry then pauses until recovery.
            total_bucket.tokens_bits -= packet_bits;
            if (!priority) telemetry_bucket.tokens_bits -= packet_bits;
        }

        // One write keeps the line ending from becoming a separate TCP frame.
        std::memcpy(transmit_buffer, message, message_length);
        transmit_buffer[message_length] = '\n';
        const size_t bytes_sent = client.write(
            reinterpret_cast<const uint8_t*>(transmit_buffer),
            payload_bytes
        );
        return bytes_sent == payload_bytes;
    }


    void get_teensy_mac(uint8_t* mac)
    {
        // Teensy 4.1 stores its unique factory MAC address
        // in the IMXRT1062 OCOTP fuse registers.
        for (uint8_t i = 0; i < 2; ++i)
        {
            mac[i] =
                (HW_OCOTP_MAC1 >> ((1 - i) * 8)) & 0xFF;
        }

        for (uint8_t i = 0; i < 4; ++i)
        {
            mac[i + 2] =
                (HW_OCOTP_MAC0 >> ((3 - i) * 8)) & 0xFF;
        }
    }
} // namespace


// Initialization

bool ethernet_link_init()
{
    uint8_t mac[6];

    get_teensy_mac(mac);


    // Print MAC address for debugging.
    Serial.printf(
        "Ethernet MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]
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


    Ethernet.begin(
        mac,
        local_ip,
        dns,
        gateway,
        subnet
    );

    server.begin();

    reset_receive_buffer();
    reset_downlink_buckets();

    return true;
}


// Update

void ethernet_link_update()
{
    // Remove a client after connection loss.
    if (client && !client.connected())
    {
        client.stop();
        reset_receive_buffer();
    }


    // Accept a ground station connection.
    if (!client)
    {
        EthernetClient new_client = server.accept();

        if (new_client)
        {
            client = new_client;
            reset_receive_buffer();
            reset_downlink_buckets();

            Serial.println("Ground station connected.");
        }
    }


    if (!client || line_ready)
    {
        return;
    }


    // Process incoming bytes without blocking the main loop.
    while (client.available() && !line_ready)
    {
        const char c =
            static_cast<char>(client.read());


        // Ignore carriage returns.
        if (c == '\r')
        {
            continue;
        }


        // Newline terminates one message.
        if (c == '\n')
        {
            if (receive_length == 0)
            {
                continue;
            }

            receive_buffer[receive_length] = '\0';
            line_ready = true;

            continue;
        }


        // Store normal characters.
        if (receive_length < ETHERNET_RX_BUFFER_SIZE - 1)
        {
            receive_buffer[receive_length++] = c;
        }
        else
        {
            // Discard an oversized message.
            reset_receive_buffer();
        }
    }
}


// Status

bool ethernet_link_connected()
{
    return client &&
        client.connected();
}


// Receive

bool ethernet_link_read_line(
    char* buffer,
    size_t buffer_size
)
{
    if (!line_ready ||
        buffer == nullptr ||
        buffer_size == 0)
    {
        return false;
    }


    strncpy(
        buffer,
        receive_buffer,
        buffer_size - 1
    );

    buffer[buffer_size - 1] = '\0';


    receive_length = 0;
    line_ready = false;

    return true;
}


// Transmit

bool ethernet_link_send_line(
    const char* message
)
{
    return send_line(message, false);
}


bool ethernet_link_send_priority_line(
    const char* message
)
{
    return send_line(message, true);
}


bool ethernet_link_set_downlink_limit(float limit_kbit_s)
{
    const bool unlimited = limit_kbit_s == 0.0f;
    const bool within_range =
        limit_kbit_s >= ETHERNET_MIN_DOWNLINK_LIMIT_KBIT_S &&
        limit_kbit_s <= ETHERNET_MAX_DOWNLINK_LIMIT_KBIT_S;

    if (!std::isfinite(limit_kbit_s) ||
        (!unlimited && !within_range))
    {
        return false;
    }

    downlink_limit_kbit_s = limit_kbit_s;
    reset_downlink_buckets();
    return true;
}


float ethernet_link_get_downlink_limit()
{
    return downlink_limit_kbit_s;
}
