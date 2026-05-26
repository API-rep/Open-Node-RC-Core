/******************************************************************************
 * @file  cb_dir.cpp
 * @brief CbProc function — direction detector (bipolaire value → DRIVE_STATE_BUS).
 *****************************************************************************/

#include "cb_dir.h"

#include <struct/simulation_struct.h>   // DriveState, DriveStateBus


// =============================================================================
// 1. PROC FUNCTION
// =============================================================================

/** @brief Direction detector — see cb_dir.h for full contract. */
void cb_dir_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const CbDirCfg* cfg = static_cast<const CbDirCfg*>(proc->cfg);

    const int8_t ds = (value > cfg->neutral) ? DriveState::kDriveFwd
                    : (value < cfg->neutral) ? DriveState::kDriveRev
                    :                          DriveState::kStanding;

    proc->outValue = DriveStateBus::encode(ds);  // runner commits to proc->outCh
    (void)value;  // observer — does not modify value
}

// EOF cb_dir.cpp
