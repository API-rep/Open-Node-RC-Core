/*!****************************************************************************
 * @file    ctrl_config.h
 * @brief   Volvo A60H Bruder — CtrlChannel pipeline declaration.
 *
 * @details Exposes the CtrlChannel array consumed by ctrl_update() in the
 *   machine main loop.
 *
 *   Channel pipelines:
 *     CTRL_DIRECT_DRIVE : DIRECT_DRIVE_BTN → [toggle (speed-gated)] → DIRECT_DRIVE
 *****************************************************************************/
#pragma once

#include <struct/ctrl_struct.h>  // CtrlChannel


// =============================================================================
// 1. CHANNEL COUNT
// =============================================================================

/**
 * @brief Indices into `kCtrlChannels[]`.
 *
 * @details `CTRL_CH_COUNT` is the sentinel used as array size and loop bound.
 */
enum CtrlCh {
    CTRL_DIRECT_DRIVE = 0,  ///< DIRECT_DRIVE_BTN → [toggle] → DIRECT_DRIVE
    CTRL_CH_COUNT,
};


// =============================================================================
// 2. EXPORTED ARRAY
// =============================================================================

extern CtrlChannel kCtrlChannels[];
extern const uint8_t kCtrlChannelCount;

// EOF ctrl_config.h
