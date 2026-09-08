#pragma once
#include <cstdint>
#include <cstddef>

#include <deque>
#include <string>
#include <cassert>

inline uint32_t fake_time = 0;
inline uint32_t millis() { return fake_time; }
struct HardwareSerialIMXRT {
    std::deque<char> rx;
    std::string tx;
    int room = 32768;
    uint32_t baud = 0;
    void addMemoryForRead(void*, size_t) {}
    void addMemoryForWrite(void*, size_t) {}
    void begin(uint32_t value) { baud = value; }
    int available() { return rx.size(); }
    int availableForWrite() { return room; }
    int read() { char c = rx.front(); rx.pop_front(); return static_cast<uint8_t>(c); }
    size_t write(const uint8_t* data, size_t size) {
        assert(static_cast<int>(size) <= room); // Any blocking write fails the test.
        room -= size;
        tx.append(reinterpret_cast<const char*>(data), size);
        return size;
    }
    size_t write(uint8_t c) { return write(&c, 1); }
    explicit operator bool() { return true; }
    void print(const char*) {}
    void println(uint32_t) {}
    void inject(const std::string& value) { for (char c : value) rx.push_back(c); }
};
inline HardwareSerialIMXRT Serial, Serial1, Serial2, Serial3, Serial4,
    Serial5, Serial6, Serial7, Serial8;

#include <cstdio>
template<class T> T constrain(T x, T lo, T hi) { return x < lo ? lo : x > hi ? hi : x; }


