/*!****************************************************************************
 * @file  brake_input_config.h
 * @brief Dumper truck — INPUT brake chain: cfg, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbProc, cb_center_fn, cb_abs_fn, cb_scale_fn, CbCenterCfg, CbScaleCfg.
 *
 *   Chain function: BRAKE_STICK → [center, abs, scale] → BRAKE_BUS.
 *   In:  BRAKE_STICK (input of the type: released at neutral, full at max; L2 on a PS4).
 *   Out: BRAKE_BUS (0 = released, max = full brake), read by the brake of the
 *        throttle chain (cb_brake).
 *
 *   The input is safe at neutral, like any analog channel; BRAKE_BUS is released at
 *   0.  A stick below neutral brakes as much as above it (abs): a trigger never goes
 *   there.  Stateless processors only.
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

	///  Deviation from the neutral: released = 0.
static constexpr CbCenterCfg kBrakeInputCenterCfg {};

	///  Magnitude [0..32768] to the range of BRAKE_BUS [0..65535].
static constexpr CbScaleCfg kBrakeInputScaleCfg {
	.inMax  = 32768u,   ///< largest magnitude: max - neutral
	.outMax = 65535u,   ///< full brake
};


// =============================================================================
// 2. PROC ARRAY
// =============================================================================

/**
 * @brief  Brake input chain proc array.
 * @details Steps:
 *   1  center : BRAKE_STICK - neutral, signed (released = 0).
 *   2  abs    : magnitude [0..32768].
 *   3  scale  : to [0..65535], the range of BRAKE_BUS.
 */
static CbProc kBrakeInputProcs[] = {
		// 1. center — released at 0
	{ .name = "brake_center",
	  .fn   = cb_center_fn,
	  .cfg  = &kBrakeInputCenterCfg,
	},
		// 2. abs — magnitude
	{ .name = "brake_abs",
	  .fn   = cb_abs_fn,
	},
		// 3. scale — to the range of BRAKE_BUS
	{ .name = "brake_scale",
	  .fn   = cb_scale_fn,
	  .cfg  = &kBrakeInputScaleCfg,
	},
};

// EOF brake_input_config.h
