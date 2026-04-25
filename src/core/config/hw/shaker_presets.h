/******************************************************************************
 * @file shaker_presets.h
 * @brief Ready-made ShakerCfg presets — generic shaker hardware library.
 *
 * @details Each entry is a `constexpr ShakerCfg` value (0–100 % motor power).
 *   Machine sound config files include this header and select the preset that
 *   matches their physical shaker hardware.
 *
 *   Preset naming convention: `kShaker_<HardwareModel>`.
 *****************************************************************************/
#pragma once

#include <core/system/hw/shaker.h>


// =============================================================================
// 1. GT-POWER SHAKERS  (vibration motor + offset weight)
// =============================================================================

  /// @brief GT-Power shaker with stock brass weight.
  /// @details Used in: TAMIYA King Hauler, Volvo A60H Bruder.
static constexpr ShakerCfg kShaker_GtPowerStock = {
    .idle         = 49u,  ///< Motor power at engine idle.
    .start        = 100u, ///< Motor power during engine start.
    .fullThrottle = 40u,  ///< Motor power at full throttle (smoother at speed).
    .stop         = 60u,  ///< Motor power during engine stop.
};

  /// @brief GT-Power shaker with 3D-printed plastic weight.
  /// @details Used in: Hercules Hobby Actros.
static constexpr ShakerCfg kShaker_GtPowerPlastic = {
    .idle         = 49u,
    .start        = 100u,
    .fullThrottle = 40u,
    .stop         = 60u,
};

// EOF shaker_presets.h
