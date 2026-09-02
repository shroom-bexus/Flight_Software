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

// The table maps the received command name to its handler function.
struct CommandEntry
{
    const char* name;
    CommandHandler handler;
};

void send_reply(const char* type, const char* command, const char* detail = nullptr)
{
    // Every response starts with ACK, NACK, or WARN.
    char response[64];
    if (detail)
    {
        snprintf(response, sizeof(response), "%s,%s,%s", type, command, detail);
        ethernet_link_send_line(response);
        return;
    }

    snprintf(response, sizeof(response), "%s,%s", type, command);
    ethernet_link_send_line(response);
}

bool parse_float(const char* text, float& value)
{
    // Reject trailing characters instead of silently accepting part of a value.
    char* end = nullptr;
    value = strtof(text, &end);
    return text != end && *end == '\0';
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
    {"SET_HEATER", handle_set_heater}
};
} // namespace

void commands_handle(const char* message)
{
    // Ignore normal telemetry or malformed input without the CMD prefix.
    if (std::strncmp(message, "CMD,", 4) != 0) return;
    const char* command = message + 4;

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

    ethernet_link_send_line("NACK,UNKNOWN_COMMAND");
}

#endif // ENABLE_ETHERNET
