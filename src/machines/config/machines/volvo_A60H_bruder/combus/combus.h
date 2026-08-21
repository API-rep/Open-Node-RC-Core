/*!****************************************************************************
 * @file  combus.h
 * @brief ComBus — Volvo A60H Bruder — externs runtime + input/light mapping
 *
 * Point d'entrée runtime du sous-projet machine. Ce fichier n'est compilé
 * QUE par l'environnement machine Volvo A60H Bruder — pas de garde
 * IS_MACHINE/IS_REMOTE nécessaire ici, la séparation de sous-projet fait
 * déjà ce filtrage.
 *******************************************************************************///
#pragma once

#include "combus_ids.h"

#include <core/config/machines/dumper_truck/inputs/input_channels_analog.inc>  // A9.9: raw INPUT acquisition (SYSTEM) \u2014 lives in core/, injected into the array initializer in combus.cpp
#include <core/config/machines/dumper_truck/inputs/input_channels_digital.inc>  // A9.9: raw INPUT acquisition (SYSTEM) \u2014 lives in core/, injected into the array initializer in combus.cpp

#include <core/config/machines/dumper_truck/inputs/input_ids_analog.inc>        // A9.9: raw INPUT acquisition (SYSTEM) \u2014 lives in core/, injected into the enum tail in combus_ids.h

#include <core/config/machines/dumper_truck/inputs/input_ids_digital.inc>       // A9.9: raw INPUT acquisition (SYSTEM) \u2014 lives in core/, injected into the enum tail in combus_ids.h

#include <struct/combus_struct.h>

/// @brief Com-bus analog channels configuration array
extern AnalogComBus  AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)];

/// @brief Com-bus digital channels configuration array
extern DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)];

/// @brief Communication bus structure
extern ComBus comBus;

// NOTE: les blocs LIGHT_ENABLE / INPUT_MODULE de l'ancien combus.h ne sont
// PAS repris ici pour l'instant — traités dans une étape ultérieure une
// fois confirmé si dumper_truck_lights.h / inputs_map.h sont génériques
// au type (restent en core) ou spécifiques à cette instance (déménagent
// ici). Ne pas les ajouter de ta propre initiative à cette étape.

// EOF combus.h