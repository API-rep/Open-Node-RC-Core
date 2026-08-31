/*!****************************************************************************
 * @file struct.h
 * @brief Global structure definition top file
 *******************************************************************************/// 
#pragma once

//#include "core_struct.h"
#include "machines_struct.h"
#include "simulation_struct.h"
#include "remotes_struct.h"
#include "vbat_struct.h"
#include "pin_struct.h"
#include "uart_struct.h"
#include "simulation_struct.h"

// NOTE: combus_struct.h and outputs_struct.h used to be aggregated here via
// local quoted includes.  Both files have been relocated as part of the
// combus-frame-handshake refactor:
//   - combus_struct.h     -> <core/system/combus/combus_defs.h>
//   - outputs_struct.h    -> <core/system/combus/frame/combus_frame_defs.h>
// Translation units that need them now include those paths directly.


// EOF struct.h