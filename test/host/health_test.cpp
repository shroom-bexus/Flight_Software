#include "../../src/telemetry.cpp"
#include <cassert>
#include <vector>
#include <string>
bool received[9] = {};
uint32_t times[9] = {}, errors[9] = {};
bool accept = true;
std::vector<std::string> reports;
bool teensy_link_has_received(uint8_t id) { return received[id-1]; }
uint32_t teensy_link_last_received_ms(uint8_t id) { return times[id-1]; }
uint32_t teensy_link_remote_overflows(uint8_t id) { return errors[id-1]; }
bool airdos_has_received_data(uint8_t i) { return received[i+7]; }
uint32_t airdos_get_last_message_ms(uint8_t i) { return times[i+7]; }
uint32_t airdos_get_overflow_count(uint8_t i) { return errors[i+7]; }
uint8_t airdos_get_sensor_id(uint8_t i) { return i+8; }
bool ethernet_link_send_line(const char* line) {
    if (accept) reports.emplace_back(line);
    return accept;
}
int main() {
    update_airdos_health(0,true);
    assert(reports.size()==9);
    reports.clear();
    for(unsigned i=0;i<9;++i) {received[i]=true; times[i]=100;}
    update_airdos_health(100,false);
    assert(reports.size()==9);
    for(const auto& s:reports) assert(s.find(",OK,")!=std::string::npos);
    reports.clear();
    update_airdos_health(200,false); assert(reports.empty());
    accept=false;
    update_airdos_health(15200,false); assert(reports.empty());
    accept=true;
    update_airdos_health(15300,false); assert(reports.size()==9);
    for(const auto& s:reports) assert(s.find(",FAULT,")!=std::string::npos);
    reports.clear();
    times[8]=15400;
    update_airdos_health(15400,false);
    assert(reports.size()==1 && reports[0].find("AIRDOS,9,OK")!=std::string::npos);
    reports.clear();
    update_airdos_health(60000,true); assert(reports.size()==9);
    puts("AIRDOS immediate transitions, retry, recovery and periodic health passed");
}
