#pragma once
#include <cstring>
struct FakeEEPROM {
    unsigned char bytes[4096];
    template<class T> void get(int address, T& value) { std::memcpy(&value, bytes + address, sizeof(T)); }
    template<class T> void put(int address, const T& value) { std::memcpy(bytes + address, &value, sizeof(T)); }
};
extern FakeEEPROM EEPROM;
