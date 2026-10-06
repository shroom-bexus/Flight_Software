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
}
