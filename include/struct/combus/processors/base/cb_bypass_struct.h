/******************************************************************************
 * @file  cb_bypass_struct.h
 * @brief Config struct for `cb_bypass_fn` — conditional bypass gate.
 *
 * @details Retained for type safety and future extension.
 *   In practice `CbProc::cfg` is `nullptr` for this processor — the gate
 *   condition is fully described by `CbProc::inCh[0]`.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. CONFIG STRUCT
// =============================================================================

/**
 * @brief Config for `cb_bypass_fn` — conditional bypass gate.
 *
 * @details Empty struct — `CbProc::cfg` must be `nullptr` for this processor.
 *   The condition channel is declared in `CbProc::inCh[0]`.
 *   Kept as a named type for documentation and future extension.
 */
struct CbBypassCfg {};


// EOF cb_bypass_struct.h
