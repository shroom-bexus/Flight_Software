#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <algorithm>
using std::max;
inline uint64_t clock_us=0;
inline uint32_t millis(){return clock_us/1000;}
inline uint32_t micros(){return clock_us;}
constexpr uint32_t HW_OCOTP_MAC1=0, HW_OCOTP_MAC0=0;
struct SerialMock { template<class... T> void printf(const char*,T...) {} };
inline SerialMock Serial;
