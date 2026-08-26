
// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology//

#ifndef FLIGHT_SOFTWARE_LOGGER_H
#define FLIGHT_SOFTWARE_LOGGER_H

#include <Arduino.h>


// Initialize the SD card and open all log files
bool logger_init();


// Save one set of regularly sampled sensor data
void logger_log_sensor_data(
    float temperature_K,
    float pressure_Pa
);


// Save one raw AIRDOS UART message
// sensorIndex starts at 0
void logger_log_airdos(
    uint8_t sensorIndex,
    const char* data
);


// Call regularly from loop().
// Flushes buffered data to the SD card.
void logger_update();

#endif //FLIGHT_SOFTWARE_LOGGER_H
