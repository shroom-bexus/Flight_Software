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
}
