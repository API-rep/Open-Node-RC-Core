/*!****************************************************************************
 * @file  sim_math.cpp
 * @brief SimProc functions — generic arithmetic transforms.
 *****************************************************************************/

#include "sim_math.h"

#include <core/system/combus/combus_access.h>  // combus_set_analog (sim_drive_state_fn)


// =============================================================================
// 1. SIMPROC FUNCTIONS
// =============================================================================

/** @brief Signed center deviation \u2014 see sim_math.h for contract. */
void sim_center_fn(SimProc* proc, uint16_t& value, ComBus& /*bus*/, bool& /*claimed*/, ChanOwner /*chanOwner*/)
{
    const SimCenterCfg* cfg = static_cast<const SimCenterCfg*>(proc->cfg);

    // Reinterpret as signed: (int16_t)(value - cfg->neutral), packed back in uint16_t.
    // Positive side (FWD): value > neutral  → small positive int16.
    // Negative side (REV): value < neutral  → two's complement (e.g. −300 → 0xFED4).
    value = static_cast<uint16_t>(static_cast<int16_t>(value)
                                - static_cast<int16_t>(cfg->neutral));
}

/** @brief Absolute value with optional digital side effect — see sim_math.h for contract. */
void sim_abs_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& /*claimed*/, ChanOwner /*chanOwner*/)
{
    const int16_t sv = static_cast<int16_t>(value);

    // --- Optional digital side effect (sign: HIGH = FWD, LOW = REV) -----------
    if (proc->optOutDCh.has_value()) {
        bus.digitalBus[static_cast<uint8_t>(*proc->optOutDCh)].value = (sv >= 0);
    }

    // --- Absolute value --------------------------------------------------------
    value = static_cast<uint16_t>(sv >= 0 ? sv : -sv);
}

/** @brief Linear scale \u2014 see sim_math.h for contract. */
void sim_scale_fn(SimProc* proc, uint16_t& value, ComBus& /*bus*/, bool& /*claimed*/, ChanOwner /*chanOwner*/)
{
    const SimScaleCfg* cfg = static_cast<const SimScaleCfg*>(proc->cfg);

    value = static_cast<uint16_t>(
        static_cast<int32_t>(value)       * static_cast<int32_t>(cfg->outMax)
      / static_cast<int32_t>(cfg->inMax));
}
/** @brief Drive-state observer — see sim_math.h for contract. */
void sim_drive_state_fn(SimProc* proc, uint16_t& value, ComBus& bus, bool& /*claimed*/, ChanOwner chanOwner)
{
    const SimDriveStateCfg* cfg = static_cast<const SimDriveStateCfg*>(proc->cfg);

    // Determine direction from post-ramp bipolaire value.
    const int8_t ds = (value > cfg->neutral) ? DriveState::kDriveFwd
                    : (value < cfg->neutral) ? DriveState::kDriveRev
                    :                          DriveState::kStanding;

    // Write encoded state to optional output channel (e.g. DRIVE_STATE_BUS).
    if (proc->optOutCh.has_value()) {
        combus_set_analog(bus, proc->optOutCh.value(), DriveStateBus::encode(ds), chanOwner);
    }
    // value is NOT modified — this proc is a side-effect-only observer.
    (void)value;
}

// EOF sim_math.cpp
