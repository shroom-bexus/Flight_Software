#pragma once
#pragma once
struct IPAddress { IPAddress(){} IPAddress(int,int,int,int){} };
struct EthernetMock { template<class... T> void begin(T...){} };
inline EthernetMock Ethernet;
