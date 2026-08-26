
// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "data_logger.h"

#include <SD.h>

#include "config.h"
#include "rtc.h"


// ============================================================================
// Configuration
// ============================================================================

// How often buffered data is physically written to the SD card
static const uint32_t FLUSH_INTERVAL_MS = 1000;


// ============================================================================
// Files
// ============================================================================

static File sensorFile;

// One file for each AIRDOS sensor
static File airdosFiles[AIRDOS_SENSOR_COUNT];


// Time of the last SD flush
static uint32_t lastFlushTime = 0;


// ============================================================================
// Initialization
// ============================================================================

bool logger_init()
{
    // Initialize the Teensy 4.1 built-in SD card
    if (!SD.begin(BUILTIN_SDCARD))
    {
        Serial.println("ERROR: SD card initialization failed");
        return false;
    }

    Serial.println("SD card initialized");


    // ------------------------------------------------------------------------
    // Main sensor file
    // ------------------------------------------------------------------------

    sensorFile = SD.open("sensors.csv", FILE_WRITE);

    if (!sensorFile)
    {
        Serial.println("ERROR: Could not open sensors.csv");
        return false;
    }

    // Write the CSV header only if the file is new
    if (sensorFile.size() == 0)
    {
        sensorFile.println(
            "timestamp,time_ms,temperature_K,pressure_Pa"
        );

        sensorFile.flush();
    }


    // ------------------------------------------------------------------------
    // AIRDOS files
    // ------------------------------------------------------------------------

    for (uint8_t i = 0; i < AIRDOS_SENSOR_COUNT; i++)
    {
        char filename[24];

        snprintf(
            filename,
            sizeof(filename),
            "airdos_%u.log",
            i + 1
        );

        airdosFiles[i] = SD.open(filename, FILE_WRITE);

        if (!airdosFiles[i])
        {
            Serial.print("ERROR: Could not open ");
            Serial.println(filename);

            return false;
        }
    }


    Serial.println("Logger initialized");

    return true;
}


// ============================================================================
// Regular sensor data
// ============================================================================

void logger_log_sensor_data(
    float temperature_K,
    float pressure_Pa
)
{
    if (!sensorFile)
        return;


    // Get the current UTC timestamp from the RTC
    char timestamp[24];

    rtc_get_timestamp(
        timestamp,
        sizeof(timestamp)
    );


    // Example:
    // 2026-08-26T15:42:31Z,12345,298.153,95482.10

    sensorFile.print(timestamp);
    sensorFile.print(',');

    sensorFile.print(millis());
    sensorFile.print(',');

    sensorFile.print(temperature_K, 3);
    sensorFile.print(',');

    sensorFile.println(pressure_Pa, 2);
}


// ============================================================================
// AIRDOS UART data
// ============================================================================

void logger_log_airdos(
    uint8_t sensorIndex,
    const char* data
)
{
    // Protect against an invalid sensor number
    if (sensorIndex >= AIRDOS_SENSOR_COUNT)
        return;

    if (!airdosFiles[sensorIndex])
        return;


    // Get the current UTC timestamp
    char timestamp[24];

    rtc_get_timestamp(
        timestamp,
        sizeof(timestamp)
    );


    // Store the UART message without modifying it
    //
    // Example:
    // 2026-08-26T15:42:31Z,12345,$ENV,...

    airdosFiles[sensorIndex].print(timestamp);
    airdosFiles[sensorIndex].print(',');

    airdosFiles[sensorIndex].print(millis());
    airdosFiles[sensorIndex].print(',');

    airdosFiles[sensorIndex].println(data);
}


// ============================================================================
// SD card maintenance
// ============================================================================

void logger_update()
{
    const uint32_t currentTime = millis();


    // Do not flush on every measurement.
    // Frequent flushes unnecessarily slow down SD writes.
    if (currentTime - lastFlushTime < FLUSH_INTERVAL_MS)
        return;

    lastFlushTime = currentTime;


    // Flush regular sensor data
    if (sensorFile)
        sensorFile.flush();


    // Flush all AIRDOS files
    for (uint8_t i = 0; i < AIRDOS_SENSOR_COUNT; i++)
    {
        if (airdosFiles[i])
            airdosFiles[i].flush();
    }
}