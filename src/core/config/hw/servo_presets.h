/******************************************************************************
 * @file servo_presets.h
 * @brief Named `SrvHwAngle` constants for common servo hardware.
 *
 * @details Each entry is a `constexpr SrvHwAngle` with the datasheet mechanical
 *   angle limits of a widely-used servo model.  Pass its address via `hwAngle`
 *   in a `SrvDevice` config declaration.
 *
 *   Use `nullptr` for `hwAngle` to disable the angular pipeline entirely.
 *
 *   To add a model: declare one `constexpr SrvHwAngle kXxx { min, max };` below,
 *   derive values from the datasheet "operating angle" specification.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_res.h>  // SrvHwAngle


// =============================================================================
// 1. BUILT-IN PRESETS
// =============================================================================

  /// Generic ±45° servo — 90° total swing, neutral-center.
static constexpr SrvHwAngle srvNc90 { -45.0f, +45.0f };

  /// Generic ±90° servo — 180° total swing, neutral-center.
static constexpr SrvHwAngle srvNc180 { -90.0f, +90.0f };

  /// Generic ±120° servo — 240° total swing, neutral-center.
static constexpr SrvHwAngle srvNc120 { -60.0f, +60.0f };

  /// Generic ±120° servo — 240° total swing, neutral-center.
static constexpr SrvHwAngle srvNc240 { -120.0f, +120.0f };


// EOF servo_presets.h
