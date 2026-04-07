/******************************************************************************
 * @file combus_res.h
 * @brief ComBus compile-time range constants and conversion helpers.
 *
 * @details Provides the three full-scale constants (`CbusMaxVal`, `CbusMinVal`,
 *   `CbusNeutral`) derived algebraically from `combus_t` (defined in
 *   `include/const.h`), and two `constexpr` helpers for expressing motion
 *   config values in human-readable units:
 *
 *   `pctToCbus(pct)`                  — % of half-range → `combus_t` offset
 *   `angleToCbus(angleDeg, hw)`       — degrees → absolute `combus_t` value
 *
 *   For servo presets, use a named `constexpr` from `servo_presets.h` by value:
 *   @code
 *     .hwAngle = srvNc180,  // ±90° range, neutral-centered
 *     .maxHwVal = angleToCbus(+45.0f, srvNc180),;
 *   @endcode
 *
 *   All motion config fields (`MotionHw`, presets, etc.) must be expressed
 *   using these constants and helpers — no bare numeric literals.
 *
 *   **Inherent 1-lsb asymmetry:** `CbusNeutral = CbusMaxVal >> 1`, giving
 *   32767 for uint16_t.  The forward half-range is 1 unit wider than reverse.
 *   This is negligible (~0.0015 %) and intentional (matches legacy behaviour).
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <cassert>
#include <const.h>                  // combus_t


// =============================================================================
// 1. COMPILE-TIME RANGE CONSTANTS
// =============================================================================

  /// Combus maximum value — all bits set (width-agnostic).
static constexpr combus_t CbusMaxVal = static_cast<combus_t>(~combus_t{0});

  /// Combus minimum value — all bits cleared (width-agnostic).
static constexpr combus_t CbusMinVal = combus_t{0};

  /// Combus neutral value — midpoint, rounded down (inherent 1-lsb asymmetry).
static constexpr combus_t CbusNeutral = static_cast<combus_t>(CbusMaxVal >> 1u);



// =============================================================================
// 2. CONVERSION HELPERS
// =============================================================================

/**
 * @brief Convert a motor speed percentage to a `combus_t` offset from neutral.
 *
 * @details Maps pct [0, 100] to [0, CbusNeutral].  Returns an offset
 *   from neutral — always combine with `CbusNeutral` for an absolute value.
 *
 *   Uses `uint64_t` intermediate to avoid overflow for any `combus_t` width
 *   up to 32-bit.  Values above 100 are a logic error (`assert`).
 *
 * @param pct  Speed percentage 0–100.
 * @return     Offset from neutral in `combus_t` units.
 */

constexpr combus_t pctToCbus(uint8_t pct)
{
    assert(pct <= 100u);
    return static_cast<combus_t>(
        static_cast<uint64_t>(CbusNeutral) * (pct > 100u ? 100u : pct) / 100u
    );
}



// =============================================================================
// 3. SERVO ANGLE CONFIG
// =============================================================================

/**
 * @brief Servo hardware angle range — datasheet constants, set once at build time.
 *
 * @details Mandatory fields — no defaults.  Use a named preset from
 *   `servo_presets.h` and assign by value into `SrvDevice::hwAngle`.
 */

struct SrvHwAngle {
    float minHwAngle;  ///< Minimum mechanical angle (degrees).
    float maxHwAngle;  ///< Maximum mechanical angle (degrees).
    constexpr float totalRange() const { return maxHwAngle - minHwAngle; }  ///< Total swing (degrees).
};



/**
 * @brief Convert a servo angle to an absolute `combus_t` value.
 *
 * @details Linear interpolation over [`minHwAngle`, `maxHwAngle`].
 *   No symmetry assumption — correct for asymmetric servos.
 *   Use a named `constexpr` preset from `servo_presets.h`:
 *
 * @param angleDeg  Target angle in degrees.
 * @param hw        `SrvHwAngle` with `totalRange() > 0` (asserted).
 * 
 * @return          Absolute `combus_t` value in [CbusMinVal..CbusMaxVal].
 */

constexpr combus_t angleToCbus(float angleDeg, const SrvHwAngle& hw)
{
        // --- 1. Range validity and angle within bounds ---
    assert(hw.totalRange() > 0.0f);
    assert(angleDeg >= hw.minHwAngle && angleDeg <= hw.maxHwAngle);

        // --- 2. Lerp over [minHwAngle, maxHwAngle] -> [CbusMinVal, CbusMaxVal] ---
    const float result = static_cast<float>(CbusMinVal)
                       + (static_cast<float>(CbusMaxVal) - static_cast<float>(CbusMinVal))
                         * (angleDeg - hw.minHwAngle) / hw.totalRange();

    return static_cast<combus_t>(result);
}


// EOF combus_res.h
