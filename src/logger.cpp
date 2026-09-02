// SHROOM Flight Software

#include "logger.h"

#include "config.h"
#include "rtc.h"

#if ENABLE_SD_LOGGING
#include <SD.h>
#endif


namespace
{
#if ENABLE_SD_LOGGING

    // Log files

#if ENABLE_MAX31865
    File max31865_file;
#endif

#if ENABLE_WSEN_PADS
    File wsen_pads_file;
#endif

#if ENABLE_WSEN_HIDS
    File wsen_hids_file;
#endif

#if ENABLE_WSEN_ISDS
    File wsen_isds_file;
#endif

#if ENABLE_AIRDOS
    File airdos_file;
#endif


    uint32_t last_flush_time = 0;

    bool logger_ready = false;
    uint32_t logger_error_count = 0;


    // Helper functions

    /**
 * @brief Open a log file and add the CSV header if the file is empty.
 */
    bool open_log_file(
        File& file,
        const char* filename,
        const char* header
    )
    {
        file = SD.open(filename, FILE_WRITE);

        if (!file)
        {
            return false;
        }

        // Only write the header when creating a new file.
        if (file.size() == 0)
        {
            file.println(header);
            file.flush();
        }

        return true;
    }


    /**
 * @brief Write the timestamp prefix shared by all log entries.
 *
 * Format:
 * UTC timestamp,milliseconds since boot,
 */
    void write_timestamp(File& file)
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


    /**
 * @brief Flush a file if it is currently open.
 */
    void flush_file(File& file)
    {
        if (file)
        {
            file.flush();
        }
    }

#endif // ENABLE_SD_LOGGING
} // namespace


// Initialization

bool logger_init()
{
    logger_ready = false;
    logger_error_count = 0;

#if !ENABLE_SD_LOGGING

    return true;

#else

    if (!SD.begin(BUILTIN_SDCARD))
    {
        ++logger_error_count;
        return false;
    }

    bool success = true;


#if ENABLE_MAX31865

    if (!open_log_file(
        max31865_file,
        "max31865.csv",
        "timestamp,time_ms,sensor,temperature_K"))
    {
        ++logger_error_count;
        success = false;
    }

#endif


#if ENABLE_WSEN_PADS

    if (!open_log_file(
        wsen_pads_file,
        "wsen_pads.csv",
        "timestamp,time_ms,temperature_K,pressure_Pa"))
    {
        ++logger_error_count;
        success = false;
    }

#endif


#if ENABLE_WSEN_HIDS

    if (!open_log_file(
        wsen_hids_file,
        "wsen_hids.csv",
        "timestamp,time_ms,temperature_K,humidity_percent"))
    {
        ++logger_error_count;
        success = false;
    }

#endif


#if ENABLE_WSEN_ISDS

    if (!open_log_file(
        wsen_isds_file,
        "wsen_isds.csv",
        "timestamp,time_ms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z"))
    {
        ++logger_error_count;
        success = false;
    }

#endif


#if ENABLE_AIRDOS

    if (!open_log_file(
        airdos_file,
        "airdos.csv",
        "timestamp,time_ms,sensor,data"))
    {
        ++logger_error_count;
        success = false;
    }

#endif

    logger_ready = success;
    return success;

#endif
}


// MAX31865

void logger_log_max31865(
    uint8_t sensor_index,
    float temperature_k
)
{
#if ENABLE_SD_LOGGING && ENABLE_MAX31865

    if (!max31865_file)
    {
        return;
    }

    write_timestamp(max31865_file);

    max31865_file.print(sensor_index);
    max31865_file.print(',');
    max31865_file.println(temperature_k, 3);

#else

    (void)sensor_index;
    (void)temperature_k;

#endif
}


// WSEN-PADS

void logger_log_wsen_pads(
    float temperature_k,
    float pressure_pa
)
{
#if ENABLE_SD_LOGGING && ENABLE_WSEN_PADS

    if (!wsen_pads_file)
    {
        return;
    }

    write_timestamp(wsen_pads_file);

    wsen_pads_file.print(temperature_k, 3);
    wsen_pads_file.print(',');
    wsen_pads_file.println(pressure_pa, 2);

#else

    (void)temperature_k;
    (void)pressure_pa;

#endif
}


// WSEN-HIDS

void logger_log_wsen_hids(
    float temperature_k,
    float humidity_percent
)
{
#if ENABLE_SD_LOGGING && ENABLE_WSEN_HIDS

    if (!wsen_hids_file)
    {
        return;
    }

    write_timestamp(wsen_hids_file);

    wsen_hids_file.print(temperature_k, 3);
    wsen_hids_file.print(',');
    wsen_hids_file.println(humidity_percent, 2);

#else

    (void)temperature_k;
    (void)humidity_percent;

#endif
}


// WSEN-ISDS

void logger_log_wsen_isds(
    float accel_x,
    float accel_y,
    float accel_z,
    float gyro_x,
    float gyro_y,
    float gyro_z
)
{
#if ENABLE_SD_LOGGING && ENABLE_WSEN_ISDS

    if (!wsen_isds_file)
    {
        return;
    }

    write_timestamp(wsen_isds_file);

    wsen_isds_file.print(accel_x, 4);
    wsen_isds_file.print(',');
    wsen_isds_file.print(accel_y, 4);
    wsen_isds_file.print(',');
    wsen_isds_file.print(accel_z, 4);
    wsen_isds_file.print(',');
    wsen_isds_file.print(gyro_x, 4);
    wsen_isds_file.print(',');
    wsen_isds_file.print(gyro_y, 4);
    wsen_isds_file.print(',');
    wsen_isds_file.println(gyro_z, 4);

#else

    (void)accel_x;
    (void)accel_y;
    (void)accel_z;
    (void)gyro_x;
    (void)gyro_y;
    (void)gyro_z;

#endif
}


// AIRDOS

void logger_log_airdos(
    uint8_t sensor_index,
    const char* data
)
{
#if ENABLE_SD_LOGGING && ENABLE_AIRDOS

    if (!airdos_file)
    {
        return;
    }

    write_timestamp(airdos_file);

    airdos_file.print(sensor_index);
    airdos_file.print(',');
    airdos_file.println(data);

#else

    (void)sensor_index;
    (void)data;

#endif
}


// Periodic flush

void logger_update()
{
#if ENABLE_SD_LOGGING

    const uint32_t current_time = millis();

    // Unsigned subtraction remains correct when millis() overflows.
    if (current_time - last_flush_time < SD_FLUSH_PERIOD_MS)
    {
        return;
    }

    last_flush_time = current_time;


#if ENABLE_MAX31865
    flush_file(max31865_file);
#endif

#if ENABLE_WSEN_PADS
    flush_file(wsen_pads_file);
#endif

#if ENABLE_WSEN_HIDS
    flush_file(wsen_hids_file);
#endif

#if ENABLE_WSEN_ISDS
    flush_file(wsen_isds_file);
#endif

#if ENABLE_AIRDOS
    flush_file(airdos_file);
#endif

#endif
}

bool logger_is_ready()
{
    return logger_ready;
}


uint32_t logger_get_error_count()
{
    return logger_error_count;
}
