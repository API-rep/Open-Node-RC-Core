/*!****************************************************************************
 * @file input_init.h
 * @brief Input system initialisation script
 * This section contain scripts an routines used to initialize the input system.
 * By this it :
 * - Parse remote config file to rich C code (const, var, struct)
 * - Parse input config file to rich C code (const, var, struct)
 * - Do some sanitary check
 * - Compute hardware related environement value
 * - Create harware device object
 * 
 * This script MUST be include in init.h top file
 *******************************************************************************/// 
#pragma once

#include "config.h"

#include <const.h>
#include <struct/struct.h>  // STAGE 1 :
#include <defs/defs.h>

// 2. ENSUITE, on inclut le sélecteur de machine qui définit les enums (AnalogComCh)
// C'est ici que combus.h intervient.
#include <core/config/combus/combus.h>

// 3. ENFIN, on peut inclure le mapping, car AnalogComCh est maintenant connu
#include <struct/remotes_map_struct.h>



// EOF input_init.h