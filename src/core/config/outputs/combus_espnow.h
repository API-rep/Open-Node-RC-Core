/******************************************************************************
 * @file combus_espnow.h
 * @brief ComBus ESP-Now transport — physical payload cap.
 *
 * @details Physical constraint for the ESP-Now broadcast transport layer.
 * Include this file from any output config that uses ESP-Now to verify
 * frame sizes against the Espressif hard limit.
 *
 * @note ESP-Now transport is not yet implemented. This file is a placeholder
 *       that centralises the physical cap so future configs have a single
 *       authoritative source.
 *****************************************************************************/
#pragma once

#include <stdint.h>

/// ESP-Now hard payload limit — Espressif specification, non-negotiable.
static constexpr uint8_t CombusPhysEspNowMax = 250u;

// EOF combus_espnow.h
