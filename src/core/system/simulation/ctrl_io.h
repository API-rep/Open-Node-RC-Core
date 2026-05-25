/*!****************************************************************************
 * @file  ctrl_io.h
 * @brief Bus I/O CtrlProcFns — dedicated read and write procs for CtrlChannel.
 *
 * @details These two procs are the first and last entries in every CtrlChannel
 *   proc array.  They decouple the runner from bus channel knowledge: the runner
 *   never accesses `inCh`/`outCh` directly — all bus I/O goes through these procs.
 *
 *   `ctrl_read_fn`:
 *   - Reads `isDrived && value` from `inCh`.
 *   - When `!isDrived`, sets `value = false` so downstream procs see a released
 *     button and update their state accordingly (toggle retains active, no rising edge).
 *
 *   `ctrl_write_fn`:
 *   - Writes `value` to `outCh` via `combus_set_digital()`.
 *
 *   @code
 *     static constexpr CtrlReadCfg  kReadCfg  { .inCh  = DigitalComBusID::MY_BTN   };
 *     static constexpr CtrlWriteCfg kWriteCfg { .outCh = DigitalComBusID::MY_STATE };
 *     // First and last entries in the proc array:
 *     { "read",  ctrl_read_fn,  &kReadCfg,  nullptr },
 *     { "write", ctrl_write_fn, &kWriteCfg, nullptr },
 *   @endcode
 *****************************************************************************/
#pragma once

#include <struct/ctrl_struct.h>  // CtrlProc, CtrlReadCfg, CtrlWriteCfg, CtrlProcFn


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Read CtrlProcFn — reads one digital channel from the bus into `value`.
 *
 * @param proc    Proc descriptor: `cfg → const CtrlReadCfg*`, `state → nullptr`.
 * @param value   Set to `isDrived && value` of the configured input channel.
 * @param bus     Full ComBus — source of the read.
 * @param claimed Unused.
 * @param owner   Unused.
 */
void ctrl_read_fn(CtrlProc* proc, bool& value, ComBus& bus,
                  bool& claimed, ChanOwner owner);

/**
 * @brief Write CtrlProcFn — writes `value` to one digital channel in the bus.
 *
 * @param proc    Proc descriptor: `cfg → const CtrlWriteCfg*`, `state → nullptr`.
 * @param value   State to write to the configured output channel.
 * @param bus     Full ComBus — destination of the write.
 * @param claimed Unused.
 * @param owner   ChanOwner used for the `combus_set_digital()` call.
 */
void ctrl_write_fn(CtrlProc* proc, bool& value, ComBus& bus,
                   bool& claimed, ChanOwner owner);

// EOF ctrl_io.h
