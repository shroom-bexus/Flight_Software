// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
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
    {"PING",       handle_ping},
    {"SET_TARGET", handle_set_target},

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