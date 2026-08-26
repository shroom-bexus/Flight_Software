
// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "logger.h"

#include <SD.h>

#include "config.h"
#include "rtc.h"


// ============================================================================
// Log files
// ============================================================================

// Files only exist in firmware builds where they are actually needed.

#if ENABLE_SD_LOGGING && ENABLE_MAX31865 && LOG_MAX31865
static File max31865File;
#endif

#if ENABLE_SD_LOGGING && ENABLE_WSEN_PADS && LOG_WSEN_PADS
static File wsenPadsFile;
#endif

#if ENABLE_SD_LOGGING && ENABLE_WSEN_HIDS && LOG_WSEN_HIDS
static File wsenHidsFile;
#endif

#if ENABLE_SD_LOGGING && ENABLE_WSEN_ISDS && LOG_WSEN_ISDS
static File wsenIsdsFile;
#endif

#if ENABLE_SD_LOGGING && ENABLE_AIRDOS && LOG_AIRDOS
static File airdosFile;
#endif


// Time of the last forced SD flush
static uint32_t lastFlushTime = 0;


// ============================================================================
// Helper functions
// ============================================================================

#if ENABLE_SD_LOGGING

/**
 * @brief Open a log file and write its header if it is a new file.
 */
static bool open_log_file(
    File& file,
    const char* filename,
    const char* header
)
{
    file = SD.open(filename, FILE_WRITE);

    if (!file)
    {
        Serial.print("ERROR: Could not open ");
        Serial.println(filename);

        return false;
    }

    // If the file is empty, write the CSV header.
    if (file.size() == 0)
    {
        file.println(header);
        file.flush();
    }

    return true;
}


/**
 * @brief Write the timestamp and millis() prefix used by all logs.
 *
 * Result:
 * 2026-08-26T18:42:15Z,123456,
 */
static void write_timestamp(File& file)
{
    char timestamp[24];

    rtc_get_timestamp(
        timestamp,
        sizeof(timestamp)
    );

    file.print(timestamp);
    file.print(',');

    file.print(millis());
    file.print(',');
}

#endif


// ============================================================================
// Logger initialization
// ============================================================================

bool logger_init()
{
#if !ENABLE_SD_LOGGING

    // SD logging is disabled for this firmware build.
    return true;

#else

    // Initialize the Teensy 4.1 built-in SD card.
    if (!SD.begin(BUILTIN_SDCARD))
    {
        Serial.println("ERROR: SD card initialization failed");
        return false;
    }

    Serial.println("SD card initialized");


    // ------------------------------------------------------------------------
    // MAX31865
    // ------------------------------------------------------------------------

#if ENABLE_MAX31865 && LOG_MAX31865

    if (!open_log_file(
        max31865File,
        "max31865.csv",
        "timestamp,time_ms,sensor,temperature_K"
    ))
    {
        return false;
    }

#endif


    // ------------------------------------------------------------------------
    // WSEN-PADS
    // ------------------------------------------------------------------------

#if ENABLE_WSEN_PADS && LOG_WSEN_PADS

    if (!open_log_file(
        wsenPadsFile,
        "wsen_pads.csv",
        "timestamp,time_ms,temperature_K,pressure_Pa"
    ))
    {
        return false;
    }

#endif


    // ------------------------------------------------------------------------
    // WSEN-HIDS
    // ------------------------------------------------------------------------

#if ENABLE_WSEN_HIDS && LOG_WSEN_HIDS

    if (!open_log_file(
        wsenHidsFile,
        "wsen_hids.csv",
        "timestamp,time_ms,temperature_K,humidity_percent"
    ))
    {
        return false;
    }

#endif


    // ------------------------------------------------------------------------
    // WSEN-ISDS
    // ------------------------------------------------------------------------

#if ENABLE_WSEN_ISDS && LOG_WSEN_ISDS

    if (!open_log_file(
        wsenIsdsFile,
        "wsen_isds.csv",
        "timestamp,time_ms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z"
    ))
    {
        return false;
    }

#endif


    // ------------------------------------------------------------------------
    // AIRDOS
    // ------------------------------------------------------------------------

#if ENABLE_AIRDOS && LOG_AIRDOS

    if (!open_log_file(
        airdosFile,
        "airdos.csv",
        "timestamp,time_ms,sensor,data"
    ))
    {
        return false;
    }

#endif


    Serial.println("Data logger initialized");
    return true;

#endif
}


// ============================================================================
// MAX31865
// ============================================================================

void logger_log_max31865(
    uint8_t sensorIndex,
    float temperature_K
)
{
#if ENABLE_SD_LOGGING && ENABLE_MAX31865 && LOG_MAX31865

    if (!max31865File)
        return;

    write_timestamp(max31865File);

    max31865File.print(sensorIndex);
    max31865File.print(',');

    max31865File.println(temperature_K, 3);

#else

    // Prevent unused-parameter compiler warnings.
    (void)sensorIndex;
    (void)temperature_K;

#endif
}


// ============================================================================
// WSEN-PADS
// ============================================================================

void logger_log_wsen_pads(
    float temperature_K,
    float pressure_Pa
)
{
#if ENABLE_SD_LOGGING && ENABLE_WSEN_PADS && LOG_WSEN_PADS

    if (!wsenPadsFile)
        return;

    write_timestamp(wsenPadsFile);

    wsenPadsFile.print(temperature_K, 3);
    wsenPadsFile.print(',');

    wsenPadsFile.println(pressure_Pa, 2);

#else

    (void)temperature_K;
    (void)pressure_Pa;

#endif
}


// ============================================================================
// WSEN-HIDS
// ============================================================================

void logger_log_wsen_hids(
    float temperature_K,
    float humidity_percent
)
{
#if ENABLE_SD_LOGGING && ENABLE_WSEN_HIDS && LOG_WSEN_HIDS

    if (!wsenHidsFile)
        return;

    write_timestamp(wsenHidsFile);

    wsenHidsFile.print(temperature_K, 3);
    wsenHidsFile.print(',');

    wsenHidsFile.println(humidity_percent, 2);

#else

    (void)temperature_K;
    (void)humidity_percent;

#endif
}


// ============================================================================
// WSEN-ISDS
// ============================================================================

void logger_log_wsen_isds(
    float accelX,
    float accelY,
    float accelZ,
    float gyroX,
    float gyroY,
    float gyroZ
)
{
#if ENABLE_SD_LOGGING && ENABLE_WSEN_ISDS && LOG_WSEN_ISDS

    if (!wsenIsdsFile)
        return;

    write_timestamp(wsenIsdsFile);

    wsenIsdsFile.print(accelX, 4);
    wsenIsdsFile.print(',');

    wsenIsdsFile.print(accelY, 4);
    wsenIsdsFile.print(',');

    wsenIsdsFile.print(accelZ, 4);
    wsenIsdsFile.print(',');

    wsenIsdsFile.print(gyroX, 4);
    wsenIsdsFile.print(',');

    wsenIsdsFile.print(gyroY, 4);
    wsenIsdsFile.print(',');

    wsenIsdsFile.println(gyroZ, 4);

#else

    (void)accelX;
    (void)accelY;
    (void)accelZ;
    (void)gyroX;
    (void)gyroY;
    (void)gyroZ;

#endif
}


// ============================================================================
// AIRDOS
// ============================================================================

void logger_log_airdos(
    uint8_t sensorIndex,
    const char* data
)
{
#if ENABLE_SD_LOGGING && ENABLE_AIRDOS && LOG_AIRDOS

    if (!airdosFile)
        return;

    write_timestamp(airdosFile);

    airdosFile.print(sensorIndex);
    airdosFile.print(',');

    // Store the received UART message.
    airdosFile.println(data);

#else

    (void)sensorIndex;
    (void)data;

#endif
}


// ============================================================================
// Logger update
// ============================================================================

void logger_update()
{
#if ENABLE_SD_LOGGING

    const uint32_t currentTime = millis();

    // Data is written continuously using File.print().
    // flush() periodically makes sure all pending data is committed.
    if (currentTime - lastFlushTime < SD_FLUSH_PERIOD_MS)
        return;

    lastFlushTime = currentTime;


#if ENABLE_MAX31865 && LOG_MAX31865

    if (max31865File)
        max31865File.flush();

#endif


#if ENABLE_WSEN_PADS && LOG_WSEN_PADS

    if (wsenPadsFile)
        wsenPadsFile.flush();

#endif


#if ENABLE_WSEN_HIDS && LOG_WSEN_HIDS

    if (wsenHidsFile)
        wsenHidsFile.flush();

#endif


#if ENABLE_WSEN_ISDS && LOG_WSEN_ISDS

    if (wsenIsdsFile)
        wsenIsdsFile.flush();

#endif


#if ENABLE_AIRDOS && LOG_AIRDOS

    if (airdosFile)
        airdosFile.flush();

#endif

#endif
}