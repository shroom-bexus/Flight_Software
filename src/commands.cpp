// SHROOM Flight Software
// Ground-station command handling

#include "commands.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "config.h"

#if ENABLE_ETHERNET
#include "ethernet_link.h"
#include "heater.h"
#include "thermal_control.h"

namespace
{
using CommandHandler = void (*)(const char* args);

uint16_t active_command_id = 0;
uint16_t last_command_id = 0;
bool last_command_available = false;
char last_response[96];

enum class PidGain
{
    KP,
    KI,
    KD
};

// The table maps the received command name to its handler function.
struct CommandEntry
{
    const char* name;
    CommandHandler handler;
};

void send_reply(const char* type, const char* command, const char* detail = nullptr)
{
    // Every response starts with ACK, NACK, or WARN.
    if (detail)
    {
        snprintf(
            last_response,
            sizeof(last_response),
            "%s,%u,%s,%s",
            type,
            active_command_id,
            command,
            detail
        );
    }
    else
    {
        snprintf(
            last_response,
            sizeof(last_response),
            "%s,%u,%s",
            type,
            active_command_id,
            command
        );
    }

    last_command_id = active_command_id;
    last_command_available = true;
    ethernet_link_send_priority_line(last_response);
}

bool parse_float(const char* text, float& value)
{
    // Reject trailing characters instead of silently accepting part of a value.
    char* end = nullptr;
    value = strtof(text, &end);
    return text != end && *end == '\0';
}

bool parse_pid_values(const char* text, float& kp, float& ki, float& kd)
{
    char* end = nullptr;

    kp = strtof(text, &end);
    if (text == end || *end != ',') return false;

    const char* ki_text = end + 1;
    ki = strtof(ki_text, &end);
    if (ki_text == end || *end != ',') return false;

    const char* kd_text = end + 1;
    kd = strtof(kd_text, &end);
    return kd_text != end && *end == '\0';
}

void handle_ping(const char*)
{
    send_reply("ACK", "PING");
}

void handle_set_target(const char* args)
{
    float target_k;
    if (!parse_float(args, target_k))
    {
        send_reply("NACK", "SET_TARGET", "INVALID_VALUE");
        return;
    }
    if (!thermal_control_set_target(target_k))
    {
        send_reply("NACK", "SET_TARGET", "OUT_OF_RANGE");
        return;
    }

    char value[16];
    snprintf(value, sizeof(value), "%.2f", target_k);
    send_reply("ACK", "SET_TARGET", value);
}

void handle_thermal_on(const char*)
{
    thermal_control_set_enabled(true);
    send_reply("ACK", "THERMAL_ON");
}

void handle_thermal_off(const char*)
{
    thermal_control_set_enabled(false);
    send_reply("ACK", "THERMAL_OFF");
}

void handle_set_pid(const char* args)
{
    float kp;
    float ki;
    float kd;
    if (!parse_pid_values(args, kp, ki, kd))
    {
        send_reply("NACK", "SET_PID", "INVALID_VALUE");
        return;
    }
    if (!thermal_control_set_pid(kp, ki, kd))
    {
        send_reply("NACK", "SET_PID", "OUT_OF_RANGE");
        return;
    }

    char detail[48];
    snprintf(detail, sizeof(detail), "%.6g,%.6g,%.6g", kp, ki, kd);
    send_reply("ACK", "SET_PID", detail);
}

void handle_set_gain(const char* args, PidGain gain, const char* command)
{
    float value;
    if (!parse_float(args, value))
    {
        send_reply("NACK", command, "INVALID_VALUE");
        return;
    }

    float kp = thermal_control_get_kp();
    float ki = thermal_control_get_ki();
    float kd = thermal_control_get_kd();

    if (gain == PidGain::KP) kp = value;
    if (gain == PidGain::KI) ki = value;
    if (gain == PidGain::KD) kd = value;

    if (!thermal_control_set_pid(kp, ki, kd))
    {
        send_reply("NACK", command, "OUT_OF_RANGE");
        return;
    }

    char detail[16];
    snprintf(detail, sizeof(detail), "%.6g", value);
    send_reply("ACK", command, detail);
}

void handle_set_kp(const char* args)
{
    handle_set_gain(args, PidGain::KP, "SET_KP");
}

void handle_set_ki(const char* args)
{
    handle_set_gain(args, PidGain::KI, "SET_KI");
}

void handle_set_kd(const char* args)
{
    handle_set_gain(args, PidGain::KD, "SET_KD");
}

void handle_set_downlink_limit(const char* args)
{
    float limit_kbit_s;
    if (!parse_float(args, limit_kbit_s))
    {
        send_reply("NACK", "SET_DOWNLINK_LIMIT", "INVALID_VALUE");
        return;
    }

    if (!ethernet_link_set_downlink_limit(limit_kbit_s))
    {
        send_reply("NACK", "SET_DOWNLINK_LIMIT", "OUT_OF_RANGE");
        return;
    }

    char detail[16];
    snprintf(detail, sizeof(detail), "%.6g", limit_kbit_s);
    send_reply("ACK", "SET_DOWNLINK_LIMIT", detail);
}

void handle_set_heater(const char* args)
{
    // Manual heater commands must not compete with the PID controller.
    if (thermal_control_is_enabled())
    {
        send_reply("WARN", "SET_HEATER", "THERMAL_CONTROL_ACTIVE");
        return;
    }

    // Expected arguments: <heater number|ALL>,<power percent>.
    const char* separator = std::strchr(args, ',');
    if (!separator)
    {
        send_reply("NACK", "SET_HEATER", "INVALID_VALUE");
        return;
    }

    const bool all_heaters = std::strncmp(args, "ALL,", 4) == 0;
    long heater_number = 0;
    if (!all_heaters)
    {
        char* end = nullptr;
        heater_number = strtol(args, &end, 10);
        if (args == end || end != separator ||
            heater_number < 1 || heater_number > HEATER_CHANNEL_COUNT)
        {
            send_reply("NACK", "SET_HEATER", "INVALID_HEATER");
            return;
        }
    }

    float power_percent;
    if (!parse_float(separator + 1, power_percent))
    {
        send_reply("NACK", "SET_HEATER", "INVALID_VALUE");
        return;
    }
    if (!std::isfinite(power_percent) || power_percent < 0.0f || power_percent > 100.0f)
    {
        send_reply("NACK", "SET_HEATER", "OUT_OF_RANGE");
        return;
    }

    char detail[32];
    if (all_heaters)
    {
        heater_set_all_power(power_percent);
        // Persist the manual output so it survives a reset.
        thermal_control_save_heater_state();
        snprintf(detail, sizeof(detail), "ALL,%.1f", power_percent);
    }
    else
    {
        const Heater heater = static_cast<Heater>(heater_number - 1);
        if (!heater_is_enabled(heater))
        {
            send_reply("NACK", "SET_HEATER", "HEATER_DISABLED");
            return;
        }

        heater_set_power(heater, power_percent);
        thermal_control_save_heater_state();
        snprintf(
            detail,
            sizeof(detail),
            "%ld,%.1f",
            heater_number,
            heater_get_power(heater)
        );
    }

    send_reply("ACK", "SET_HEATER", detail);
}

// Add new commands by defining a handler and adding one table entry.
const CommandEntry command_table[] =
{
    {"PING", handle_ping},
    {"SET_TARGET", handle_set_target},
    {"THERMAL_ON", handle_thermal_on},
    {"THERMAL_OFF", handle_thermal_off},
    {"SET_PID", handle_set_pid},
    {"SET_KP", handle_set_kp},
    {"SET_KI", handle_set_ki},
    {"SET_KD", handle_set_kd},
    {"SET_DL_LIMIT", handle_set_downlink_limit},
    {"SET_DOWNLINK_LIMIT", handle_set_downlink_limit},
    {"SET_HEATER", handle_set_heater}
};
} // namespace

void commands_handle(const char* message)
{
    // A new GS process starts a new ID sequence with a short session token.
    if (std::strncmp(message, "HELLO,", 6) == 0)
    {
        const char* session_id = message + 6;
        const size_t length = std::strlen(session_id);
        if (length == 0 || length > 8) return;

        last_command_available = false;
        char response[32];
        snprintf(response, sizeof(response), "ACK_SESSION,%s", session_id);
        ethernet_link_send_priority_line(response);
        return;
    }

    // Ignore normal telemetry or malformed input without the CMD prefix.
    if (std::strncmp(message, "CMD,", 4) != 0) return;

    // UDP commands carry an ID so retries cannot execute a change twice.
    const char* id_text = message + 4;
    char* id_end = nullptr;
    const unsigned long parsed_id = strtoul(id_text, &id_end, 10);
    if (id_text == id_end || *id_end != ',' || parsed_id > UINT16_MAX) return;
    active_command_id = static_cast<uint16_t>(parsed_id);

    if (last_command_available)
    {
        const int16_t difference = static_cast<int16_t>(
            active_command_id - last_command_id
        );
        if (difference == 0)
        {
            ethernet_link_send_priority_line(last_response);
            return;
        }
        if (difference < 0)
        {
            char response[64];
            snprintf(
                response,
                sizeof(response),
                "NACK,%u,STALE_COMMAND",
                active_command_id
            );
            ethernet_link_send_priority_line(response);
            return;
        }
    }

    const char* command = id_end + 1;

    // Find an exact command-name match and pass on the remaining arguments.
    for (const CommandEntry& entry : command_table)
    {
        const size_t name_length = std::strlen(entry.name);
        if (std::strncmp(command, entry.name, name_length) != 0) continue;
        if (command[name_length] != '\0' && command[name_length] != ',') continue;

        const char* args = command[name_length] == ','
            ? command + name_length + 1
            : "";
        entry.handler(args);
        return;
    }

    send_reply("NACK", "UNKNOWN_COMMAND");
}

#endif // ENABLE_ETHERNET
