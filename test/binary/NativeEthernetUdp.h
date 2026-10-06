#pragma once
#include "NativeEthernet.h"
#include <string>
#include <vector>
#include <utility>
inline std::vector<std::pair<uint64_t,std::string>> sent;
inline bool fail_send=false;
struct EthernetUDP {
    std::string payload;
    int begin(int){return 1;}
    int beginPacket(IPAddress,int){payload.clear();return 1;}
    size_t write(const uint8_t* p,size_t n){payload.append(reinterpret_cast<const char*>(p),n);return n;}
    size_t write(uint8_t p){payload+=char(p);return 1;}
    int endPacket(){if(fail_send)return 0;sent.emplace_back(clock_us,payload);return 1;}
    int parsePacket(){return 0;}
    int read(uint8_t*,size_t){return 0;}
    int read(){return 0;}
    int available(){return 0;}
    IPAddress remoteIP(){return {};}
    int remotePort(){return 5000;}
};
