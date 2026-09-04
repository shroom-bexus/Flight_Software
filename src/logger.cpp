// SHROOM Flight Software

#include "logger.h"

#include "config.h"
#include "rtc.h"

#if ENABLE_SD_LOGGING || ENABLE_BACKUP_SD_LOGGING
#include <SD.h>
#include <SPI.h>
#endif


namespace
{
#if ENABLE_SD_LOGGING || ENABLE_BACKUP_SD_LOGGING

    bool internal_sd_ready = false;
    bool backup_sd_ready = false;

    uint32_t internal_sd_error_count = 0;
    uint32_t backup_sd_error_count = 0;
    uint32_t last_flush_time = 0;
    bool download_mode = false;


#if ENABLE_SD_LOGGING
    File internal_download_file;
#endif


#if ENABLE_BACKUP_SD_LOGGING
    // Keep the global SD object assigned to the built-in SDIO card. The
    // soldered XTSD backup therefore gets its own independent SdFs instance.
    SdFs backup_sd;
    FsFile backup_download_file;
#endif

    LoggerStorage download_storage = LoggerStorage::INTERNAL;


    void disable_internal_sd();
    void disable_backup_sd();


    /**
     * @brief One logical log file mirrored to both storage devices.
     *
     * A failed destination is disabled independently. The other destination
     * continues to receive the complete byte stream.
     */
    class MirroredLogFile : public Print
    {
    public:
        bool open(const char* filename, const char* header)
        {
#if ENABLE_SD_LOGGING
            if (internal_sd_ready)
            {
                internal_file = SD.open(filename, FILE_WRITE);

                if (!internal_file ||
                    !write_internal_header_if_empty(header))
                {
                    ++internal_sd_error_count;
                    internal_sd_ready = false;
                    internal_file.close();
                }
            }
#endif

#if ENABLE_BACKUP_SD_LOGGING
            if (backup_sd_ready)
            {
                backup_file = backup_sd.open(
                    filename,
                    O_RDWR | O_CREAT | O_AT_END
                );

                if (!backup_file ||
                    !write_backup_header_if_empty(header))
                {
                    ++backup_sd_error_count;
                    backup_sd_ready = false;
                    backup_file.close();
                }
            }
#endif

            return static_cast<bool>(*this);
        }


        size_t write(uint8_t value) override
        {
            return write(&value, 1);
        }


        size_t write(const uint8_t* buffer, size_t size) override
        {
            bool written = false;

#if ENABLE_SD_LOGGING
            if (internal_sd_ready && internal_file)
            {
                if (internal_file.write(buffer, size) == size)
                {
                    written = true;
                }
                else
                {
                    disable_internal_sd();
                }
            }
#endif

#if ENABLE_BACKUP_SD_LOGGING
            if (backup_sd_ready && backup_file)
            {
                if (backup_file.write(buffer, size) == size)
                {
                    written = true;
                }
                else
                {
                    disable_backup_sd();
                }
            }
#endif

            return written ? size : 0;
        }


        void flush()
        {
#if ENABLE_SD_LOGGING
            if (internal_sd_ready && internal_file)
            {
                internal_file.flush();
                if (internal_file.getWriteError()) disable_internal_sd();
            }
#endif

#if ENABLE_BACKUP_SD_LOGGING
            if (backup_sd_ready && backup_file)
            {
                if (!backup_file.sync() || backup_file.getWriteError())
                {
                    disable_backup_sd();
                }
            }
#endif
        }


        void close()
        {
            close_internal();
            close_backup();
        }


        void close_internal()
        {
#if ENABLE_SD_LOGGING
            internal_file.close();
#endif
        }


        void close_backup()
        {
#if ENABLE_BACKUP_SD_LOGGING
            backup_file.close();
#endif
        }


        explicit operator bool()
        {
            bool open = false;

#if ENABLE_SD_LOGGING
            open = open || (internal_sd_ready && internal_file);
#endif

#if ENABLE_BACKUP_SD_LOGGING
            open = open || (backup_sd_ready && backup_file);
#endif

            return open;
        }


    private:
#if ENABLE_SD_LOGGING
        File internal_file;

        bool write_internal_header_if_empty(const char* header)
        {
            if (internal_file.size() != 0) return true;

            internal_file.println(header);
            internal_file.flush();
            return !internal_file.getWriteError();
        }
#endif

#if ENABLE_BACKUP_SD_LOGGING
        FsFile backup_file;

        bool write_backup_header_if_empty(const char* header)
        {
            if (backup_file.size() != 0) return true;

            backup_file.println(header);
            return backup_file.sync() && !backup_file.getWriteError();
        }
#endif
    };


    // Log files

#if ENABLE_MAX31865
    MirroredLogFile max31865_file;
#endif

#if ENABLE_WSEN_PADS
    MirroredLogFile wsen_pads_file;
#endif

#if ENABLE_WSEN_HIDS
    MirroredLogFile wsen_hids_file;
#endif

#if ENABLE_WSEN_ISDS
    MirroredLogFile wsen_isds_file;
#endif

#if ENABLE_AIRDOS
    MirroredLogFile airdos_file;
#endif


    void close_internal_files()
    {
#if ENABLE_MAX31865
        max31865_file.close_internal();
#endif
#if ENABLE_WSEN_PADS
        wsen_pads_file.close_internal();
#endif
#if ENABLE_WSEN_HIDS
        wsen_hids_file.close_internal();
#endif
#if ENABLE_WSEN_ISDS
        wsen_isds_file.close_internal();
#endif
#if ENABLE_AIRDOS
        airdos_file.close_internal();
#endif
    }


    void close_backup_files()
    {
#if ENABLE_MAX31865
        max31865_file.close_backup();
#endif
#if ENABLE_WSEN_PADS
        wsen_pads_file.close_backup();
#endif
#if ENABLE_WSEN_HIDS
        wsen_hids_file.close_backup();
#endif
#if ENABLE_WSEN_ISDS
        wsen_isds_file.close_backup();
#endif
#if ENABLE_AIRDOS
        airdos_file.close_backup();
#endif
    }


    void disable_internal_sd()
    {
#if ENABLE_SD_LOGGING
        if (!internal_sd_ready) return;

        ++internal_sd_error_count;
        internal_sd_ready = false;
        close_internal_files();
#endif
    }


    void disable_backup_sd()
    {
#if ENABLE_BACKUP_SD_LOGGING
        if (!backup_sd_ready) return;

        ++backup_sd_error_count;
        backup_sd_ready = false;
        close_backup_files();
#endif
    }


    /**
     * @brief Open a mirrored log file and add its CSV header when empty.
     */
    void open_log_file(
        MirroredLogFile& file,
        const char* filename,
        const char* header
    )
    {
        file.open(filename, header);
    }


    /**
     * @brief Write the timestamp prefix shared by all log entries.
     *
     * Format:
     * UTC timestamp,milliseconds since boot,
     */
    void write_timestamp(MirroredLogFile& file)
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
     * @brief Flush a mirrored file if at least one copy is open.
     */
    void flush_file(MirroredLogFile& file)
    {
        if (file)
        {
            file.flush();
        }
    }


    void flush_all_files()
    {
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
    }


    void close_all_files()
    {
#if ENABLE_MAX31865
        max31865_file.close();
#endif
#if ENABLE_WSEN_PADS
        wsen_pads_file.close();
#endif
#if ENABLE_WSEN_HIDS
        wsen_hids_file.close();
#endif
#if ENABLE_WSEN_ISDS
        wsen_isds_file.close();
#endif
#if ENABLE_AIRDOS
        airdos_file.close();
#endif
    }

#endif // ENABLE_SD_LOGGING || ENABLE_BACKUP_SD_LOGGING
} // namespace


// Initialization

bool logger_init()
{
#if !ENABLE_SD_LOGGING && !ENABLE_BACKUP_SD_LOGGING

    return true;

#else

    internal_sd_ready = false;
    backup_sd_ready = false;
    internal_sd_error_count = 0;
    backup_sd_error_count = 0;
    download_mode = false;


#if ENABLE_SD_LOGGING

    internal_sd_ready = SD.begin(BUILTIN_SDCARD);

    if (!internal_sd_ready)
    {
        ++internal_sd_error_count;
    }

#endif


#if ENABLE_BACKUP_SD_LOGGING

    // Explicit pin selection documents and enforces the PCB routing.
    BACKUP_SD_SPI_BUS.setMOSI(BACKUP_SD_MOSI_PIN);
    BACKUP_SD_SPI_BUS.setMISO(BACKUP_SD_MISO_PIN);
    BACKUP_SD_SPI_BUS.setSCK(BACKUP_SD_SCK_PIN);

    // Keep the XTSD deselected until SdFat starts the SPI transaction.
    pinMode(BACKUP_SD_CS_PIN, OUTPUT);
    digitalWrite(BACKUP_SD_CS_PIN, HIGH);

    backup_sd_ready = backup_sd.begin(
        SdSpiConfig(
            BACKUP_SD_CS_PIN,
            SHARED_SPI,
            SD_SCK_MHZ(BACKUP_SD_SPI_CLOCK_MHZ),
            &BACKUP_SD_SPI_BUS
        )
    );

    if (!backup_sd_ready)
    {
        ++backup_sd_error_count;
    }

#endif


#if ENABLE_MAX31865
    open_log_file(
        max31865_file,
        "max31865.csv",
        "timestamp,time_ms,sensor,temperature_K"
    );
#endif

#if ENABLE_WSEN_PADS
    open_log_file(
        wsen_pads_file,
        "wsen_pads.csv",
        "timestamp,time_ms,temperature_K,pressure_Pa"
    );
#endif

#if ENABLE_WSEN_HIDS
    open_log_file(
        wsen_hids_file,
        "wsen_hids.csv",
        "timestamp,time_ms,temperature_K,humidity_percent"
    );
#endif

#if ENABLE_WSEN_ISDS
    open_log_file(
        wsen_isds_file,
        "wsen_isds.csv",
        "timestamp,time_ms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z"
    );
#endif

#if ENABLE_AIRDOS
    open_log_file(
        airdos_file,
        "airdos.csv",
        "timestamp,time_ms,sensor,data"
    );
#endif

    // If opening one required file failed, discard that incomplete copy while
    // keeping the other storage device operational.
    if (!internal_sd_ready) close_internal_files();
    if (!backup_sd_ready) close_backup_files();

    return internal_sd_ready || backup_sd_ready;

#endif
}

// MAX31865

void logger_log_max31865(
    uint8_t sensor_id,
    float temperature_k
)
{
#if (ENABLE_SD_LOGGING || ENABLE_BACKUP_SD_LOGGING) && ENABLE_MAX31865

    if (!max31865_file)
    {
        return;
    }

    write_timestamp(max31865_file);

    max31865_file.print(sensor_id);
    max31865_file.print(',');
    max31865_file.println(temperature_k, 3);

#else

    (void)sensor_id;
    (void)temperature_k;

#endif
}


// WSEN-PADS

void logger_log_wsen_pads(
    float temperature_k,
    float pressure_pa
)
{
#if (ENABLE_SD_LOGGING || ENABLE_BACKUP_SD_LOGGING) && ENABLE_WSEN_PADS

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
#if (ENABLE_SD_LOGGING || ENABLE_BACKUP_SD_LOGGING) && ENABLE_WSEN_HIDS

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
#if (ENABLE_SD_LOGGING || ENABLE_BACKUP_SD_LOGGING) && ENABLE_WSEN_ISDS

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
    uint8_t sensor_id,
    const char* data
)
{
#if (ENABLE_SD_LOGGING || ENABLE_BACKUP_SD_LOGGING) && ENABLE_AIRDOS

    if (!airdos_file || data == nullptr)
    {
        return;
    }

    write_timestamp(airdos_file);

    airdos_file.print(sensor_id);
    airdos_file.print(',');
    airdos_file.println(data);

#else

    (void)sensor_id;
    (void)data;

#endif
}


// Periodic flush

void logger_update()
{
#if ENABLE_SD_LOGGING || ENABLE_BACKUP_SD_LOGGING

    if (download_mode) return;

    const uint32_t current_time = millis();

    // Unsigned subtraction remains correct when millis() overflows.
    if (current_time - last_flush_time < SD_FLUSH_PERIOD_MS)
    {
        return;
    }

    last_flush_time = current_time;


    flush_all_files();

#endif
}


bool logger_is_ready()
{
    bool ready = true;

#if ENABLE_SD_LOGGING
    ready = ready && internal_sd_ready;
#endif

#if ENABLE_BACKUP_SD_LOGGING
    ready = ready && backup_sd_ready;
#endif

    return ready;
}


bool logger_internal_sd_is_ready()
{
    return internal_sd_ready;
}


bool logger_backup_sd_is_ready()
{
    return backup_sd_ready;
}


uint32_t logger_get_error_count()
{
    return internal_sd_error_count + backup_sd_error_count;
}


uint32_t logger_get_internal_sd_error_count()
{
    return internal_sd_error_count;
}


uint32_t logger_get_backup_sd_error_count()
{
    return backup_sd_error_count;
}


uint8_t logger_get_backup_sd_error_code()
{
#if ENABLE_BACKUP_SD_LOGGING
    return backup_sd.sdErrorCode();
#else
    return 0;
#endif
}


uint8_t logger_get_backup_sd_error_data()
{
#if ENABLE_BACKUP_SD_LOGGING
    return backup_sd.sdErrorData();
#else
    return 0;
#endif
}


bool logger_enter_download_mode()
{
#if !ENABLE_SD_LOGGING && !ENABLE_BACKUP_SD_LOGGING
    return false;
#else
    if (download_mode) return true;

    // No writer may remain open while the same filesystem is downloaded.
    flush_all_files();
    close_all_files();
    download_mode = true;

    return internal_sd_ready || backup_sd_ready;
#endif
}


bool logger_download_storage_available(LoggerStorage storage)
{
    if (!download_mode) return false;

    if (storage == LoggerStorage::INTERNAL)
    {
#if ENABLE_SD_LOGGING
        return internal_sd_ready;
#else
        return false;
#endif
    }

#if ENABLE_BACKUP_SD_LOGGING
    return backup_sd_ready;
#else
    return false;
#endif
}


bool logger_download_file_size(
    LoggerStorage storage,
    const char* filename,
    uint64_t& size
)
{
    size = 0;
    if (!logger_download_storage_available(storage) || filename == nullptr)
    {
        return false;
    }

    if (storage == LoggerStorage::INTERNAL)
    {
#if ENABLE_SD_LOGGING
        File file = SD.open(filename, FILE_READ);
        if (!file) return false;

        size = file.size();
        file.close();
        return true;
#endif
    }
    else
    {
#if ENABLE_BACKUP_SD_LOGGING
        FsFile file = backup_sd.open(filename, O_RDONLY);
        if (!file) return false;

        size = file.size();
        file.close();
        return true;
#endif
    }

    return false;
}


bool logger_download_open(
    LoggerStorage storage,
    const char* filename,
    uint64_t& size
)
{
    logger_download_close();
    size = 0;

    if (!logger_download_storage_available(storage) || filename == nullptr)
    {
        return false;
    }

    download_storage = storage;

    if (storage == LoggerStorage::INTERNAL)
    {
#if ENABLE_SD_LOGGING
        internal_download_file = SD.open(filename, FILE_READ);
        if (!internal_download_file) return false;

        size = internal_download_file.size();
        return true;
#endif
    }
    else
    {
#if ENABLE_BACKUP_SD_LOGGING
        backup_download_file = backup_sd.open(filename, O_RDONLY);
        if (!backup_download_file) return false;

        size = backup_download_file.size();
        return true;
#endif
    }

    return false;
}


int32_t logger_download_read(uint8_t* buffer, size_t size)
{
    if (buffer == nullptr || size == 0) return 0;

    if (download_storage == LoggerStorage::INTERNAL)
    {
#if ENABLE_SD_LOGGING
        if (!internal_download_file) return -1;
        return internal_download_file.read(buffer, size);
#endif
    }
    else
    {
#if ENABLE_BACKUP_SD_LOGGING
        if (!backup_download_file) return -1;
        return backup_download_file.read(buffer, size);
#endif
    }

    return -1;
}


void logger_download_close()
{
#if ENABLE_SD_LOGGING
    internal_download_file.close();
#endif
#if ENABLE_BACKUP_SD_LOGGING
    backup_download_file.close();
#endif
}
