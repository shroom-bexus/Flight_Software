// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "ethernet_link.h"

#include <NativeEthernet.h>

#include "config.h"


namespace
{
    // ============================================================================
    // TCP server
    // ============================================================================

    EthernetServer server(ETHERNET_TCP_PORT);
    EthernetClient client;


    // ============================================================================
    // Receive buffer
    // ============================================================================

    char receive_buffer[ETHERNET_RX_BUFFER_SIZE];

    size_t receive_length = 0;
    bool line_ready = false;


    // ============================================================================
    // Helper functions
    // ============================================================================

    void reset_receive_buffer()
    {
        receive_length = 0;
        line_ready = false;
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


// ============================================================================
// Initialization
// ============================================================================

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

    return true;
}


// ============================================================================
// Update
// ============================================================================

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


// ============================================================================
// Status
// ============================================================================

bool ethernet_link_connected()
{
    return client &&
        client.connected();
}


// ============================================================================
// Receive
// ============================================================================

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


// ============================================================================
// Transmit
// ============================================================================

bool ethernet_link_send_line(
    const char* message
)
{
    if (!ethernet_link_connected() ||
        message == nullptr)
    {
        return false;
    }

    client.println(message);

    return true;
}