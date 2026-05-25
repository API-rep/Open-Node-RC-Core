/*!****************************************************************************
 * @file    ctrl_config.h
 * @brief   Volvo A60H Bruder — CtrlChain pipeline declaration.
 *
 * @details Exposes the CtrlChain array consumed by ctrl_update() in the
 *   machine main loop.
 *
 *   Channel pipelines:
 *     CTRL_DIRECT_DRIVE : read(DIRECT_DRIVE_BTN) → speed_gate → toggle → write(DIRECT_DRIVE)
 *****************************************************************************/
#pragma once

#include <struct/ctrl_struct.h>  // CtrlChain


// =============================================================================
// 1. CHANNEL COUNT
// =============================================================================

/**
 * @brief Indices into `kCtrlChains[]`.
 *
 * @details `CTRL_CH_COUNT` is the sentinel used as array size and loop bound.
 */
enum CtrlCh {
    CTRL_DIRECT_DRIVE = 0,  ///< read(DIRECT_DRIVE_BTN) → speed_gate → toggle → write(DIRECT_DRIVE)
    CTRL_CH_COUNT,
};


// =============================================================================
// 2. EXPORTED ARRAY
// =============================================================================

extern CtrlChain kCtrlChains[];
extern const uint8_t kCtrlChainCount;

// EOF ctrl_config.h
