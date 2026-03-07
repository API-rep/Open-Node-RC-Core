/******************************************************************************
 * @file sound_uart_rx.h
 * Sound module — sound-side UART receiver.
 *
 * @details Receives binary ComBus transport frames from the machine ESP32
 * over UART and exposes the latest valid snapshot for the sound HAL.
 *
 * Frame sync is byte-by-byte: the receiver scans for SOF (0xAA), then
 * accumulates bytes until a full validated frame is complete.
 *
 * Call sound_uart_rx_update() every loop iteration (non-blocking).
 * Then read sound_uart_rx_snapshot() to get the latest data in the HAL.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/transport/combus_transport.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the UART receiver port for the machine link.
 *
 * @param rxPin  GPIO pin connected to machine ESP32 UART TX.
 * @param txPin  TX pin (-1 for receive-only link).
 */
void sound_uart_rx_init(int rxPin, int txPin = -1);

/**
 * @brief Poll the UART and process any incoming bytes.
 *
 * @details Non-blocking. Accumulates bytes into an internal ring buffer,
 * searches for valid frames, and updates the internal snapshot on success.
 * Call every loop iteration.
 */
void sound_uart_rx_update();

/**
 * @brief Return a pointer to the latest valid decoded snapshot.
 *
 * @details Returns nullptr until at least one valid frame has been received.
 * The returned pointer is stable until the next valid frame arrives.
 *
 * @return Const pointer to the latest ComBusSnapshot, or nullptr.
 */
const ComBusSnapshot* sound_uart_rx_snapshot();

/**
 * @brief Milliseconds since the last valid frame was received.
 *
 * @details Used by the HAL to detect timeout / link loss.
 * Returns UINT32_MAX if no frame has ever been received.
 */
uint32_t sound_uart_rx_age_ms();

/**
 * @brief True if the link is considered alive (last frame within timeout).
 *
 * @param timeoutMs  Stale threshold in ms (default: 500 ms = ~25 missed frames).
 */
bool sound_uart_rx_is_alive(uint32_t timeoutMs = 500u);

// EOF sound_uart_rx.h
