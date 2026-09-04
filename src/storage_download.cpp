// SHROOM Flight Software
// Post-flight log download over the Teensy USB serial connection

#include "storage_download.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include "logger.h"


namespace
{
constexpr size_t COMMAND_BUFFER_SIZE = 128;
constexpr size_t TRANSFER_BUFFER_SIZE = 1024;

char command_buffer[COMMAND_BUFFER_SIZE];
size_t command_length = 0;

bool download_mode = false;
bool transfer_active = false;
LoggerStorage transfer_storage = LoggerStorage::INTERNAL;
char transfer_filename[48];
uint64_t transfer_size = 0;
uint64_t transfer_remaining = 0;
uint32_t transfer_crc = 0xFFFFFFFF;
uint8_t transfer_buffer[TRANSFER_BUFFER_SIZE];

const char* const known_log_files[] =
{
    "max31865.csv",
    "wsen_pads.csv",
    "wsen_hids.csv",
    "wsen_isds.csv",
    "airdos.csv"
};


const char* storage_name(LoggerStorage storage)
{
    return storage == LoggerStorage::INTERNAL ? "INTERNAL" : "BACKUP";
}


bool parse_storage(const char* text, LoggerStorage& storage)
{
    if (std::strcmp(text, "INTERNAL") == 0)
    {
        storage = LoggerStorage::INTERNAL;
        return true;
    }
    if (std::strcmp(text, "BACKUP") == 0)
    {
        storage = LoggerStorage::BACKUP;
        return true;
    }
    return false;
}


bool known_filename(const char* filename)
{
    for (const char* known : known_log_files)
    {
        if (std::strcmp(filename, known) == 0) return true;
    }
    return false;
}


void send_error(const char* detail)
{
    Serial.print("DOWNLOAD_ERROR,");
    Serial.println(detail);
}


uint32_t update_crc32(uint32_t crc, const uint8_t* data, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ (0xEDB88320UL & -(crc & 1));
        }
    }
    return crc;
}


void list_files(LoggerStorage storage)
{
    if (!logger_download_storage_available(storage))
    {
        send_error("STORAGE_UNAVAILABLE");
        return;
    }

    Serial.print("LIST_BEGIN,");
    Serial.println(storage_name(storage));

    for (const char* filename : known_log_files)
    {
        uint64_t size;
        if (!logger_download_file_size(storage, filename, size)) continue;

        char line[96];
        snprintf(
            line,
            sizeof(line),
            "FILE,%s,%llu",
            filename,
            static_cast<unsigned long long>(size)
        );
        Serial.println(line);
    }

    Serial.print("LIST_END,");
    Serial.println(storage_name(storage));
}


void start_transfer(LoggerStorage storage, const char* filename)
{
    if (!known_filename(filename))
    {
        send_error("INVALID_FILENAME");
        return;
    }

    uint64_t size;
    if (!logger_download_open(storage, filename, size))
    {
        send_error("FILE_NOT_FOUND");
        return;
    }

    transfer_storage = storage;
    std::strncpy(transfer_filename, filename, sizeof(transfer_filename) - 1);
    transfer_filename[sizeof(transfer_filename) - 1] = '\0';
    transfer_size = size;
    transfer_remaining = size;
    transfer_crc = 0xFFFFFFFF;
    transfer_active = true;

    char line[128];
    snprintf(
        line,
        sizeof(line),
        "FILE_BEGIN,%s,%s,%llu",
        storage_name(storage),
        transfer_filename,
        static_cast<unsigned long long>(transfer_size)
    );
    Serial.println(line);
}


void finish_transfer()
{
    logger_download_close();

    // The empty line separates raw file bytes from the final protocol line.
    Serial.println();

    char line[80];
    snprintf(
        line,
        sizeof(line),
        "FILE_END,%llu,%08lX",
        static_cast<unsigned long long>(transfer_size),
        static_cast<unsigned long>(transfer_crc ^ 0xFFFFFFFF)
    );
    Serial.println(line);
    transfer_active = false;
}


void continue_transfer()
{
    if (transfer_remaining == 0)
    {
        finish_transfer();
        return;
    }

    const int serial_space = Serial.availableForWrite();
    if (serial_space <= 0) return;

    size_t wanted = sizeof(transfer_buffer);
    if (wanted > transfer_remaining)
    {
        wanted = static_cast<size_t>(transfer_remaining);
    }
    if (wanted > static_cast<size_t>(serial_space))
    {
        wanted = static_cast<size_t>(serial_space);
    }

    const int32_t bytes_read = logger_download_read(transfer_buffer, wanted);
    if (bytes_read <= 0)
    {
        logger_download_close();
        transfer_active = false;
        send_error("READ_FAILED");
        return;
    }

    const size_t sent = Serial.write(
        transfer_buffer,
        static_cast<size_t>(bytes_read)
    );
    if (sent != static_cast<size_t>(bytes_read))
    {
        logger_download_close();
        transfer_active = false;
        send_error("USB_WRITE_FAILED");
        return;
    }
    transfer_crc = update_crc32(transfer_crc, transfer_buffer, sent);
    transfer_remaining -= sent;
}


void handle_command(char* command)
{
    if (std::strcmp(command, "DOWNLOAD,ENTER") == 0)
    {
        if (!logger_enter_download_mode())
        {
            send_error("NO_STORAGE");
            return;
        }

        download_mode = true;
        Serial.println("DOWNLOAD_READY,RESET_TO_RESUME");
        return;
    }

    if (!download_mode)
    {
        send_error("ENTER_REQUIRED");
        return;
    }

    constexpr const char* list_prefix = "DOWNLOAD,LIST,";
    if (std::strncmp(command, list_prefix, std::strlen(list_prefix)) == 0)
    {
        LoggerStorage storage;
        if (!parse_storage(command + std::strlen(list_prefix), storage))
        {
            send_error("INVALID_STORAGE");
            return;
        }
        list_files(storage);
        return;
    }

    constexpr const char* get_prefix = "DOWNLOAD,GET,";
    if (std::strncmp(command, get_prefix, std::strlen(get_prefix)) == 0)
    {
        char* storage_text = command + std::strlen(get_prefix);
        char* separator = std::strchr(storage_text, ',');
        if (!separator)
        {
            send_error("INVALID_COMMAND");
            return;
        }

        *separator = '\0';
        LoggerStorage storage;
        if (!parse_storage(storage_text, storage))
        {
            send_error("INVALID_STORAGE");
            return;
        }

        start_transfer(storage, separator + 1);
        return;
    }

    send_error("UNKNOWN_COMMAND");
}


void read_commands()
{
    while (Serial.available() > 0 && !transfer_active)
    {
        const char value = static_cast<char>(Serial.read());
        if (value == '\r') continue;

        if (value == '\n')
        {
            if (command_length > 0)
            {
                command_buffer[command_length] = '\0';
                handle_command(command_buffer);
                command_length = 0;
            }
            continue;
        }

        if (command_length < sizeof(command_buffer) - 1)
        {
            command_buffer[command_length++] = value;
        }
        else
        {
            command_length = 0;
            send_error("COMMAND_TOO_LONG");
        }
    }
}
} // namespace


void storage_download_update()
{
    if (transfer_active)
    {
        continue_transfer();
    }
    else
    {
        read_commands();
    }
}


bool storage_download_is_active()
{
    return download_mode;
}
