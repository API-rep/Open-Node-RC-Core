/**
 * @file config.h
 * @brief Minimal stub for the combus loopback test environment.
 *
 * Provides the board-level constants required by uart_com.cpp without pulling
 * in the full machine configuration chain. The test runner adds the test suite
 * directory to CPPPATH, so <config/config.h> resolves here instead of the real
 * machines config.
 */
#pragma once

#include <stdint.h>

// Maximum number of UART ports in the pool (matches ESP32_8M_6S.h default).
static constexpr uint8_t UartComMaxPorts = 3u;
