/*!****************************************************************************
 * @file  PS4_dualshock_map.h
 * @brief PS4 DualShock to dumper_truck defaults — input device → ComBus mapping.
 *
 * @details Default mapping for a dumper_truck using a PS4 DualShock controller
 *   over Bluetooth. Lives in core/ — any dumper_truck instance (Volvo A60H,
 *   Cat 770, Bell B45, Komatsu HD785, ...) inherits this default.
 *
 *   Override mechanism (FUTURE): a per-instance header at
 *   `<machines/.../inputs_map/PS4_dualshock_map.h>` that re-declares the same
 *   constants. Not implemented in A9.9.
 *
 *   See input.cb (YAML) + input_ids_*.inc (legacy enum fragments) for the
 *   full contract on the ComBus channel side.
 *******************************************************************************///
#pragma once

#include <core/config/inputs/inputs.h>              // InputAnalogMap / InputDigitalMap structs + device vocab (AnalogInputDevID, DigitalInputDevID)
#include <machines/config/machines/volvo_A60H_bruder/combus/combus.h>  // AnalogComBusID / DigitalComBusID (instance umbrella — provides ALL channels including SYSTEM raw)

  // input → ComBus analog channel mapping
extern const InputAnalogMap InputAnalogMapArray[];

  // number of analog mappings in InputAnalogMap array
extern const uint8_t InputAnalogMapCount;

  // input → ComBus digital channel mapping
extern const InputDigitalMap InputDigitalMapArray[];

  // number of digital mappings in InputDigitalMap array
extern const uint8_t InputDigitalMapCount;


// EOF PS4_dualshock_map.h
