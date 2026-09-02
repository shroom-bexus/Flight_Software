// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// fun
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

/**
 * @file commands.cpp
 * @brief Ground station command handling.
 */

#include "commands.h"

#include <cstring>
#include <cstdlib>

#include "config.h"


#if ENABLE_ETHERNET

#include "ethernet_link.h"
#include "thermal_control.h"
#include "heater.h"


namespace
{
    // Every command handler receives the text following the command name.
    //
    // Example:
    //
    // CMD,SET_TARGET,298.15
    //
    // args = "298.15"
    using CommandHandler = void (*)(const char* args);


    struct CommandEntry
    {
        const char* name;
        CommandHandler handler;
    };


    // ============================================================================
    // Command handlers
    // ============================================================================

    void handle_ping(const char* args)
    {
        (void)args;

        ethernet_link_send_line(
            "ACK,PING"
        );
    }


    void handle_set_target(const char* args)
    {
        char* end = nullptr;

        const float target_k =
            strtof(args, &end);


        // Argument must contain exactly one valid number.
        if (args == end ||
            *end != '\0')
        {
            ethernet_link_send_line(
                "NACK,SET_TARGET,INVALID_VALUE"
            );

            return;
        }


        // Reject unsafe or otherwise invalid target temperatures.
        if (!thermal_control_set_target(target_k))
        {
            ethernet_link_send_line(
                "NACK,SET_TARGET,OUT_OF_RANGE"
            );

            return;
        }


        char response[48];

        snprintf(
            response,
            sizeof(response),
            "ACK,SET_TARGET,%.2f",
            target_k
        );

        ethernet_link_send_line(
            response
        );
    }


    void handle_thermal_on(const char* args)
    {
        (void)args;

        thermal_control_set_enabled(
            true
        );

        ethernet_link_send_line(
            "ACK,THERMAL_ON"
        );
    }


    void handle_thermal_off(const char* args)
    {
        (void)args;

        thermal_control_set_enabled(
            false
        );

        ethernet_link_send_line(
            "ACK,THERMAL_OFF"
        );
    }


    void handle_set_heater(const char* args)
    {
        // Manual heater control is only allowed when
        // thermal control is disabled.
        if (thermal_control_is_enabled())
        {
            ethernet_link_send_line(
                "WARN,SET_HEATER,THERMAL_CONTROL_ACTIVE"
            );

            return;
        }


        // ------------------------------------------------------------------------
        // Find separator between heater and power
        // ------------------------------------------------------------------------

        const char* separator =
            std::strchr(args, ',');

        if (separator == nullptr)
        {
            ethernet_link_send_line(
                "NACK,SET_HEATER,INVALID_VALUE"
            );

            return;
        }


        // ------------------------------------------------------------------------
        // Heater selection
        // ------------------------------------------------------------------------

        bool all_heaters = false;
        long heater_number = 0;


        if (std::strncmp(args, "ALL,", 4) == 0)
        {
            all_heaters = true;
        }
        else
        {
            char* end = nullptr;

            heater_number =
                strtol(
                    args,
                    &end,
                    10
                );

            if (args == end ||
                end != separator)
            {
                ethernet_link_send_line(
                    "NACK,SET_HEATER,INVALID_HEATER"
                );

                return;
            }


            if (heater_number < 1 ||
                heater_number > HEATER_CHANNEL_COUNT)
            {
                ethernet_link_send_line(
                    "NACK,SET_HEATER,INVALID_HEATER"
                );

                return;
            }
        }


        // ------------------------------------------------------------------------
        // Heater power
        // ------------------------------------------------------------------------

        const char* power_argument =
            separator + 1;

        char* power_end = nullptr;

        const float power_percent =
            strtof(
                power_argument,
                &power_end
            );


        if (power_argument == power_end ||
            *power_end != '\0')
        {
            ethernet_link_send_line(
                "NACK,SET_HEATER,INVALID_VALUE"
            );

            return;
        }


        if (!isfinite(power_percent) ||
            power_percent < 0.0f ||
            power_percent > 100.0f)
        {
            ethernet_link_send_line(
                "NACK,SET_HEATER,OUT_OF_RANGE"
            );

            return;
        }


        // ------------------------------------------------------------------------
        // Apply heater power
        // ------------------------------------------------------------------------

        if (all_heaters)
        {
            heater_set_all_power(
                power_percent
            );

            thermal_control_save_heater_state();


            char response[48];

            snprintf(
                response,
                sizeof(response),
                "ACK,SET_HEATER,ALL,%.1f",
                power_percent
            );

            ethernet_link_send_line(
                response
            );

            return;
        }


        const Heater heater =
            static_cast<Heater>(
                heater_number - 1
            );


        if (!heater_is_enabled(heater))
        {
            ethernet_link_send_line(
                "NACK,SET_HEATER,HEATER_DISABLED"
            );

            return;
        }


        heater_set_power(
            heater,
            power_percent
        );

        thermal_control_save_heater_state();


        char response[48];

        snprintf(
            response,
            sizeof(response),
            "ACK,SET_HEATER,%ld,%.1f",
            heater_number,
            heater_get_power(heater)
        );

        ethernet_link_send_line(
            response
        );
    }


    // ============================================================================
    // Command table
    // ============================================================================
    //
    // To add a new command:
    //
    // 1. Add its handler above.
    // 2. Add one entry to this table.
    //

    const CommandEntry command_table[] =
    {
        {"PING", handle_ping},
        {"SET_TARGET", handle_set_target},
        {"THERMAL_ON", handle_thermal_on},
        {"THERMAL_OFF", handle_thermal_off},
        {"SET_HEATER", handle_set_heater},

        // Future examples:
        // {"SET_PID", handle_set_pid},
    };


    constexpr size_t COMMAND_COUNT =
        sizeof(command_table) /
        sizeof(command_table[0]);
} // namespace


// ============================================================================
// Command processing
// ============================================================================

void commands_handle(const char* message)
{
    // All ground station commands start with "CMD,".
    if (std::strncmp(
        message,
        "CMD,",
        4) != 0)
    {
        return;
    }


    // Skip "CMD,".
    const char* command =
        message + 4;


    // Search the command table.
    for (size_t i = 0; i < COMMAND_COUNT; ++i)
    {
        const char* name =
            command_table[i].name;

        const size_t name_length =
            std::strlen(name);


        // Command name must match.
        if (std::strncmp(
            command,
            name,
            name_length) != 0)
        {
            continue;
        }


        // After the command name there must either be:
        //
        //   end of message
        //
        // or:
        //
        //   comma followed by arguments
        //
        if (command[name_length] != '\0' &&
            command[name_length] != ',')
        {
            continue;
        }


        // Find command arguments.
        const char* args =
            command[name_length] == ','
                ? command + name_length + 1
                : "";


        command_table[i].handler(
            args
        );

        return;
    }


    // Correct CMD format, but unknown command.
    ethernet_link_send_line(
        "NACK,UNKNOWN_COMMAND"
    );
}

#endif // ENABLE_ETHERNET
