#pragma once
// SHB1: lossless CSV field encoding. Explicit little endian; no native structs.
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace binary_telemetry {
constexpr const char* dictionary[] = {"AIRDOS", "$E", "$START", "$STOP", "$ENV", "HEALTH", "PADS", "THERMAL", "MAX31865", "RTC", "OK", "FAULT", "PLATE_LIMIT"};
inline void u16(uint8_t* p, size_t n) { p[0]=n; p[1]=n>>8; }
inline void header(uint8_t* p, uint32_t sequence) {
    std::memcpy(p,"SHB1",4);
    for (unsigned i=0;i<4;++i) p[4+i]=sequence>>(8*i);
}
// Return a complete length-prefixed record, or zero when it does not fit.
// Canonical unsigned integers use one byte (0..239) or tag 240 + uint32.
// Every other field is preserved verbatim using tag 241 + uint16 + bytes.
inline size_t record(const char* text, uint8_t* out, size_t capacity) {
    const size_t raw=std::strlen(text);
    if (raw>65534 || capacity<3) return 0;
    size_t used=3;
    bool fits=true;
    const char* field=text;
    do {
        const char* end=std::strchr(field,',');
        const size_t n=end ? static_cast<size_t>(end-field) : std::strlen(field);
        bool numeric=n>0 && (n==1 || field[0]!='0');
        uint32_t value=0;
        for (size_t i=0;i<n && numeric;++i) {
            if (field[i]<'0'||field[i]>'9') { numeric=false; break; }
            const unsigned digit=field[i]-'0';
            if (value>(UINT32_MAX-digit)/10) { numeric=false; break; }
            value=value*10+digit;
        }
        int word=-1;
        for (size_t i=0;i<sizeof(dictionary)/sizeof(dictionary[0]);++i)
            if(std::strlen(dictionary[i])==n && std::memcmp(field,dictionary[i],n)==0) word=i;
        const size_t need=word>=0 ? 2 : numeric ? (value<240 ? 1 : 5) : n+3;
        if (used+need>capacity) { fits=false; break; }
        if (word>=0) { out[used++]=242; out[used++]=word; }
        else if (numeric) {
            if(value<240) out[used++]=value;
            else { out[used++]=240; for(unsigned i=0;i<4;++i) out[used++]=value>>(8*i); }
        } else {
            out[used++]=241; u16(out+used,n); used+=2;
            std::memcpy(out+used,field,n); used+=n;
        }
        if (!end) break;
        field=end+1;
    } while(true);
    if (!fits || used>=raw+3) {
        if(raw+3>capacity) return 0;
        out[2]=0; std::memcpy(out+3,text,raw); used=raw+3;
    } else out[2]=1;
    u16(out,used-2);
    return used;
}
}
