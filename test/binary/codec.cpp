#include "binary_telemetry.h"
#include <iostream>
#include <string>
#include <iomanip>
int main() {
    std::string line;
    while(std::getline(std::cin,line)) {
        uint8_t packet[1200];
        binary_telemetry::header(packet,42);
        size_t n=binary_telemetry::record(line.c_str(),packet+8,sizeof(packet)-8);
        if(!n) return 1;
        for(size_t i=0;i<n+8;++i) std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(packet[i]);
        std::cout<<'\n';
    }
}
