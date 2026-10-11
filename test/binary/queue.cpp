#include "../../src/ethernet_link.cpp"
#include <cassert>
#include <deque>
#include <string>
#include <iostream>
int main() {
    LineQueue<4> q;
    std::deque<std::string> expected;
    for (unsigned i=0;i<10000;++i) {
        const std::string text="X,"+std::to_string(i)+","+std::string(i%140,'a');
        uint8_t bytes[400];size_t size=binary_telemetry::record(text.c_str(),bytes,sizeof(bytes));
        if(q.push(text.c_str()))expected.emplace_back(reinterpret_cast<char*>(bytes),size);
        if(i%3 || q.size()==4) {
            assert(!q.empty());uint8_t actual[400];q.copy(*q.front(),actual);
            assert(std::string(reinterpret_cast<char*>(actual),q.front()->length)==expected.front());
            q.pop();expected.pop_front();
        }
    }
    q.clear();assert(q.empty());assert(q.push("X,1"));
    ethernet_link_init();ground_station_known=true;
    clock_us=uint64_t(UINT32_MAX)*1000-10000;
    last_ground_station_rx_ms=millis();
    assert(ethernet_link_send_airdos_line(1,"AIRDOS,1,1,$E,1,2"));
    clock_us+=21000;last_ground_station_rx_ms=millis();
    ethernet_link_update();assert(airdos_queue.empty());
    std::cout<<"Queue wraparound and clock rollover passed; queue RAM: "
             <<sizeof(airdos_queue)+sizeof(system_queue)<<" host bytes\n";

    // At 120 kbit/s the 50 ms slot target is 750 wire bytes minus the
    // conservative 66-byte Ethernet/IP/UDP overhead.
    ethernet_link_set_downlink_limit(120);
    assert(telemetry_packet_target() == 684);

    sent.clear();
    ground_station_known = true;
    last_ground_station_rx_ms = millis();
    assert(ethernet_link_send_line("RTC,1,UNKNOWN"));
    ethernet_link_update();
    assert(sent.size() == 1);

    assert(ethernet_link_send_line("RTC,2,UNKNOWN"));
    clock_us += 49000;
    last_ground_station_rx_ms = millis();
    ethernet_link_update();
    assert(sent.size() == 1);
    clock_us += 1000;
    last_ground_station_rx_ms = millis();
    ethernet_link_update();
    assert(sent.size() == 2);

    // Long AIRDOS lines above the old 255-byte limit must survive queueing.
    std::string long_airdos = "AIRDOS,3,1,$X," + std::string(1000, 'a');
    assert(long_airdos.size() < ETHERNET_TELEMETRY_LINE_MAX);
    assert(ethernet_link_send_airdos_line(1, long_airdos.c_str()));
    assert(!airdos_queue.empty());
    assert(airdos_queue.front()->length > 384);

    // A full normal queue cannot lose any of the 26 health identities.
    system_queue.clear(); airdos_queue.clear();
    for (auto& item : health_snapshots) item = HealthSnapshot{};
    for (unsigned i=0; i<ETHERNET_SYSTEM_QUEUE_DEPTH; ++i)
        assert(ethernet_link_send_line("RTC,1,UNKNOWN"));
    const auto drops = telemetry_drop_count;
    for (const char* name : {"SECONDARY", "SD_INTERNAL", "SD_BACKUP",
                            "SD_SECONDARY_INTERNAL", "SD_SECONDARY_BACKUP",
                            "PADS", "HIDS", "ISDS"}) {
        std::string line = std::string("HEALTH,1,") + name + ",OK,0";
        assert(ethernet_link_send_line(line.c_str()));
    }
    for (unsigned id=1; id<=9; ++id) {
        for (const char* name : {"AIRDOS", "MAX31865"}) {
            std::string line = std::string("HEALTH,1,") + name + "," +
                std::to_string(id) + ",OK,0,0";
            assert(ethernet_link_send_line(line.c_str()));
        }
    }
    assert(telemetry_drop_count == drops);
    unsigned pending = 0;
    for (const auto& item : health_snapshots) pending += item.pending;
    assert(pending == 26);
    assert(ethernet_link_send_line("HEALTH,2,AIRDOS,9,FAULT,16000,0"));
    assert(ethernet_link_send_line("HEALTH,3,AIRDOS,9,OK,0,0"));
    uint8_t latest[128];
    size_t latest_size = binary_telemetry::record("HEALTH,3,AIRDOS,9,OK,0,0", latest, sizeof(latest));
    bool found = false;
    for (const auto& item : health_snapshots) if (std::string(item.key) == "AIRDOS,9") {
        found = true;
        assert(item.length == latest_size);
        assert(memcmp(item.record, latest, latest_size) == 0);
    }
    assert(found);
    // UDP send failure also retains every pending report.
    clock_us += 1000000;
    last_ground_station_rx_ms = millis();
    fail_send = true;
    assert(!send_mixed_packet());
    pending = 0;
    for (const auto& item : health_snapshots) pending += item.pending;
    assert(pending == 26);
    fail_send = false;
    // A blocked send retains pending health; all reports drain once allowed.
    next_regular_send_us = micros() + 1000000;
    assert(!send_mixed_packet());
    assert(health_pending());
    for (unsigned i=0; i<100 && health_pending(); ++i) {
        clock_us += 100000;
        last_ground_station_rx_ms = millis();
        ethernet_link_update();
    }
    assert(!health_pending());
    assert(telemetry_drop_count == drops);
    std::cout << "Full-queue health retention, replacement and drain passed; health RAM: "
              << sizeof(health_snapshots) << " host bytes\n";
}
