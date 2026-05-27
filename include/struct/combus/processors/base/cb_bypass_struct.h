/******************************************************************************
 * @file  cb_bypass_struct.h
 * @brief Config struct for `cb_bypass_fn` — conditional bypass gate.
 *
 * @details Optional force value support — when `forceValue` is set, the
 *   processor overwrites `value` on claim (e.g. force gear=1 in direct-drive).
 *   When `forceValue` is nullopt, `value` passes through unchanged.
 *****************************************************************************/
#pragma once

#include <optional>   // std::optional
#include <stdint.h>


// =============================================================================
// 1. CONFIG STRUCT
// =============================================================================

/**
 * @brief Config for `cb_bypass_fn` — conditional bypass gate.
 *
 * @details `cfg` can be `nullptr` (passthrough mode) or point to this struct.
 *   When `forceValue` is set, `value` is overwritten on claim.
 *   When `forceValue` is nullopt, `value` passes through unchanged.
 *
 *   Examples:
 *     - `cfg = nullptr` : pure claim, value unchanged (STEERING/DUMP bypass).
 *     - `cfg = &{ .forceValue = 1u }` : claim + force value=1 (GEAR direct-drive).
 */
struct CbBypassCfg {
    std::optional<uint16_t> forceValue;  ///< If set, value is overwritten on claim (nullopt = passthrough).
};


// EOF cb_bypass_struct.h
