#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
template<class T> T constrain(T x, T lo, T hi) { return x < lo ? lo : x > hi ? hi : x; }
uint32_t millis();
