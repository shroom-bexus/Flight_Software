// Compiles the production queue, codec and rate scheduler against a fake UDP clock.
#include "../../src/ethernet_link.cpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <iomanip>
void tick() {
    ground_station_known=true;
    last_ground_station_rx_ms=millis();
    ethernet_link_update();
}
int main(int argc,char**argv) {
    if(argc!=2)return 1;
    ethernet_link_init();
    ethernet_link_set_downlink_limit(120);
    ground_station_known=true;
    // Failed sends must not pop data or increment sequence.
    ethernet_link_send_line("RTC,0,UNKNOWN");
    fail_send=true;tick();assert(system_queue.size()==1 && telemetry_sequence==0);
    fail_send=false;tick();assert(system_queue.empty() && telemetry_sequence==1);
    sent.clear();
    std::ifstream in(argv[1]);std::string input;
    size_t max_queue=0, offered=0;
    uint64_t next_health=1000000;
    while(std::getline(in,input)) {
        const auto tab=input.find('\t');
        const uint64_t due=std::stoull(input.substr(0,tab))*1000;
        while(clock_us<due) {
            clock_us=std::min(clock_us+1000,due);
            if(clock_us>=next_health) {
                ethernet_link_send_line("PADS,123,293.15,100000");
                next_health=clock_us+1000000;
            }
            tick();
        }
        const auto raw=input.substr(tab+1);
        const auto split=raw.find(",1,");
        assert(split!=std::string::npos);
        for(unsigned id=1;id<=9;++id) {
            auto line=raw.substr(0,split+1)+std::to_string(id)+raw.substr(split+2);
            ethernet_link_send_airdos_line(id,line.c_str());
            ++offered;
        }
        max_queue=std::max(max_queue,airdos_queue.size());
        tick();
    }
    for(unsigned i=0;i<10000 && (!airdos_queue.empty()||!system_queue.empty());++i){clock_us+=1000;tick();}
    uint64_t wire=0;
    for(auto &p:sent) {
        wire+=estimated_packet_bits(p.second.size());
        std::cout<<std::dec<<p.first<<'\t';
        for(unsigned char c:p.second)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(c);
        std::cout<<'\n';
    }
    std::cerr<<"offered="<<offered<<" packets="<<sent.size()<<" wire_bytes="<<wire/8<<" max_queue="<<max_queue<<" drops="<<telemetry_drop_count<<" suppressed="<<airdos_suppressed_count<<" remaining="<<airdos_queue.size()<<'\n';
}
