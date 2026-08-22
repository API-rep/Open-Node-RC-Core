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

// RAW INPUT acquisition now comes from input.cb via the combus_builder hook.
// The 4 input_*_*.inc fragments are removed 2026-08-22 \u2014 see combus_v2 migration.
#include "combus_ids.h"
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