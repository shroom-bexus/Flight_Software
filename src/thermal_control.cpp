// ███████╗██╗  ██╗██████╗  ██████╗  ██████╗ ███╗   ███╗
// ██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗████╗ ████║
// ███████╗███████║██████╔╝██║   ██║██║   ██║██╔████╔██║
// ╚════██║██╔══██║██╔══██╗██║   ██║██║   ██║██║╚██╔╝██║
// ███████║██║  ██║██║  ██║╚██████╔╝╚██████╔╝██║ ╚═╝ ██║
// ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝
//
// Stratospheric High-Altitude Radiation Observation of Organismic Mycology

#include "thermal_control.h"

#include <Arduino.h>

#include "config.h"
#include "heater.h"
#include "max31865.h"


namespace
{

// ============================================================================
// Runtime state
// ============================================================================

struct ControllerState
{
    float integral = 0.0f;

    float previous_temperature_k = NAN;

    uint32_t previous_time_ms = 0;

    float output_percent = 0.0f;

    bool initialized = false;
};


ControllerState controller_state;


// ============================================================================
// Helper functions
// ============================================================================

TempSensor get_control_sensor()
{
    return static_cast<TempSensor>(
        THERMAL_CONTROL_SENSOR
    );
}


void reset_controller()
{
    controller_state = {};

    heater_set_all_power(0.0f);
}

} // namespace


// ============================================================================
// Initialization
// ============================================================================

void thermal_control_init()
{
    reset_controller();
}


// ============================================================================
// PID controller
// ============================================================================

void thermal_control_update()
{
    const TempSensor sensor =
        get_control_sensor();


    // ------------------------------------------------------------------------
    // Sensor validation
    // ------------------------------------------------------------------------

    if (!max31865_is_enabled(sensor) ||
        !max31865_data_valid(sensor))
    {
        reset_controller();
        return;
    }


    const float temperature_k =
        max31865_get_temperature(sensor);


    // ------------------------------------------------------------------------
    // Overtemperature protection
    // ------------------------------------------------------------------------

    if (temperature_k >= THERMAL_MAX_TEMPERATURE_K)
    {
        reset_controller();
        return;
    }


    const uint32_t current_time_ms =
        millis();


    const float error =
        THERMAL_TARGET_K -
        temperature_k;


    // ------------------------------------------------------------------------
    // First controller update
    // ------------------------------------------------------------------------
    //
    // No derivative or integral calculation is possible yet because no
    // previous sample exists.
    //

    if (!controller_state.initialized)
    {
        controller_state.previous_temperature_k =
            temperature_k;

        controller_state.previous_time_ms =
            current_time_ms;

        controller_state.initialized = true;


        controller_state.output_percent =
            constrain(
                THERMAL_KP * error,
                0.0f,
                THERMAL_MAX_OUTPUT_PERCENT
            );


        heater_set_all_power(
            controller_state.output_percent
        );

        return;
    }


    // ------------------------------------------------------------------------
    // Time step
    // ------------------------------------------------------------------------

    const float dt =
        static_cast<float>(
            current_time_ms -
            controller_state.previous_time_ms
        ) / 1000.0f;


    if (dt <= 0.0f)
    {
        return;
    }


    // ------------------------------------------------------------------------
    // Proportional term
    // ------------------------------------------------------------------------

    const float p_term =
        THERMAL_KP * error;


    // ------------------------------------------------------------------------
    // Derivative term
    // ------------------------------------------------------------------------
    //
    // Derivative on measurement rather than error.
    //
    // This avoids a derivative kick when the target temperature changes.
    //

    const float temperature_rate =
        (
            temperature_k -
            controller_state.previous_temperature_k
        ) / dt;


    const float d_term =
        -THERMAL_KD *
        temperature_rate;


    // ------------------------------------------------------------------------
    // Integral candidate
    // ------------------------------------------------------------------------

    float integral_candidate =
        controller_state.integral +
        THERMAL_KI * error * dt;


    // Prevent unreasonable integral accumulation.
    integral_candidate =
        constrain(
            integral_candidate,
            -THERMAL_MAX_OUTPUT_PERCENT,
             THERMAL_MAX_OUTPUT_PERCENT
        );


    // ------------------------------------------------------------------------
    // Anti-windup
    // ------------------------------------------------------------------------

    const float candidate_output =
        p_term +
        integral_candidate +
        d_term;


    const bool saturated_high =
        candidate_output >
        THERMAL_MAX_OUTPUT_PERCENT;


    const bool saturated_low =
        candidate_output <
        0.0f;


    // Do not integrate further into saturation.
    // Integration in the opposite direction remains possible so that
    // the controller can leave saturation again.
    if (!(
            (saturated_high && error > 0.0f) ||
            (saturated_low  && error < 0.0f)
        ))
    {
        controller_state.integral =
            integral_candidate;
    }


    // ------------------------------------------------------------------------
    // PID output
    // ------------------------------------------------------------------------

    controller_state.output_percent =
        constrain(
            p_term +
            controller_state.integral +
            d_term,
            0.0f,
            THERMAL_MAX_OUTPUT_PERCENT
        );


    // All four heaters belong to the same thermal mass and therefore
    // receive the same commanded power.
    heater_set_all_power(
        controller_state.output_percent
    );


    // ------------------------------------------------------------------------
    // Store state
    // ------------------------------------------------------------------------

    controller_state.previous_temperature_k =
        temperature_k;

    controller_state.previous_time_ms =
        current_time_ms;
}


// ============================================================================
// Getter functions
// ============================================================================

float thermal_control_get_output()
{
    return controller_state.output_percent;
}


bool thermal_control_is_active()
{
    return controller_state.initialized;
}


float thermal_control_get_target()
{
    return THERMAL_TARGET_K;
}


float thermal_control_get_temperature()
{
    const TempSensor sensor =
        get_control_sensor();

    if (!max31865_is_enabled(sensor) ||
        !max31865_data_valid(sensor))
    {
        return NAN;
    }

    return max31865_get_temperature(sensor);
}