/*!****************************************************************************
 * @file  sim_math.cpp
 * @brief SimProc functions — generic arithmetic transforms.
 *****************************************************************************/

#include "sim_math.h"

#include <struct/simulation_struct.h>  // DriveStateBus, DriveState, SimDriveStateCfg


// =============================================================================
// 1. SIMPROC FUNCTIONS
// =============================================================================

/** @brief Signed center deviation — see sim_math.h for contract. */
void sim_center_fn(SimProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const SimCenterCfg* cfg = static_cast<const SimCenterCfg*>(proc->cfg);
    value = static_cast<uint16_t>(static_cast<int16_t>(value)
                                - static_cast<int16_t>(cfg->neutral));
}

/** @brief Absolute value with sign side effect — see sim_math.h for contract. */
void sim_abs_fn(SimProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const int16_t sv   = static_cast<int16_t>(value);
    proc->secOutValue  = static_cast<uint16_t>(sv >= 0 ? 1u : 0u);  // runner commits to optSecOutCh
    value              = static_cast<uint16_t>(sv >= 0 ? sv : -sv);
}

/** @brief Linear scale — see sim_math.h for contract. */
void sim_scale_fn(SimProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const SimScaleCfg* cfg = static_cast<const SimScaleCfg*>(proc->cfg);
    value = static_cast<uint16_t>(
        static_cast<int32_t>(value)       * static_cast<int32_t>(cfg->outMax)
      / static_cast<int32_t>(cfg->inMax));
}

/** @brief Drive-state observer — see sim_math.h for contract. */
void sim_drive_state_fn(SimProc* proc, uint16_t& value, bool& /*claimed*/, ChanOwner /*chainOwner*/)
{
    const SimDriveStateCfg* cfg = static_cast<const SimDriveStateCfg*>(proc->cfg);

    const int8_t ds = (value > cfg->neutral) ? DriveState::kDriveFwd
                    : (value < cfg->neutral) ? DriveState::kDriveRev
                    :                          DriveState::kStanding;

    proc->secOutValue = DriveStateBus::encode(ds);  // runner commits to proc->optSecOutCh
    (void)value;  // observer — does not modify value
}

// EOF sim_math.cpp
