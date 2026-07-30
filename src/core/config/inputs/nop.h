/*!****************************************************************************
 * @file  nop.h
 * @brief No-input backend — no physical input device.
 *
 * @details Provides the complete input subsystem interface when no physical
 *   remote is connected. Used for autonomous / headless machine builds.
 *
 *   Exposes exactly the same public symbols as other input backends
 *   (e.g. PS4_dualshock.h), so that consumers never need conditional
 *   logic based on -D INPUT_* flags.
 *******************************************************************************/
#pragma once

#if defined(INPUT_NONE)

    #include <defs/remotes_defs.h>  // RemoteProtocol
    #include <struct/remotes_struct.h>  // InputDev, AnalogInputDev, DigitalInputDev

    // =============================================================================
    // ANALOG DEVICE ENUMS AND DESCRIPTOR ARRAYS — EMPTY (COUNT = 0)
    // =============================================================================

    /** @brief No analog input channels — COUNT = 0. */
    enum class AnalogInputDevID : uint8_t {
        ANALOG_DEV_COUNT = 0
    };

    extern AnalogInputDev AnalogInputDevArray[static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT)];


    // =============================================================================
    // DIGITAL DEVICE ENUMS AND DESCRIPTOR ARRAYS — EMPTY (COUNT = 0)
    // =============================================================================

    /** @brief No digital input channels — COUNT = 0. */
    enum class DigitalInputDevID : uint8_t {
        DIGITAL_DEV_COUNT = 0
    };

    extern DigitalInputDev digitalInputDevArray[static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT)];


    // =============================================================================
    // TOP-LEVEL DESCRIPTOR — NO DEVICE
    // =============================================================================

    inline constexpr InputDev inputDev {
        .infoName             = "No input device",
        .protocol             = RemoteProtocol::UNDEFINED,
        .analogInputDev       = AnalogInputDevArray,
        .digitalInputDev      = digitalInputDevArray,
        .analogInputDevCount  = static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT),
        .digitalInputDevCount = static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT)
    };


    // =============================================================================
    // COMPILE-TIME SANITY CHECKS
    // =============================================================================

    static_assert(static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT) == 0u,
                  "nop: ANALOG_DEV_COUNT must be 0");
    static_assert(static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT) == 0u,
                  "nop: DIGITAL_DEV_COUNT must be 0");

#endif

// EOF nop.h
