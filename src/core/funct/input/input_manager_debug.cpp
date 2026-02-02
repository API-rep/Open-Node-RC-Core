/**
 * @file input_debug.cpp
 * @brief Debug functions for input mapping and bus monitoring
 */

#include "input_manager.h"
#include <core/config/combus/combus.h>


/**
 * @brief Verbose debug output for input mapping
 */

void debugInputMapping(ComBus &bus) {
  Serial.println(F("\n--- [DEBUG] INPUT TO COMBUS MAPPING ---"));
  
// ==========================================================
// 1. ANALOG CHANNELS DEBUG
// ==========================================================

  Serial.println(F("[ANALOG CHANNELS]"));
  for (uint8_t i = 0; i < InputAnalogMapCount; i++) {
    
      // --- Safe data access ---
    const InputAnalogMap m = InputAnalogMapArray[i];
    uint8_t ch = static_cast<uint8_t>(m.busChannel);
    
      // --- Console output ---
    Serial.printf("  Map #%d: [DevID:%d] -> [BusCh:%d] | Value: %5u | Drived: %s %s\n", 
                  i, 
                  static_cast<uint8_t>(m.devID), 
                  ch,
                  bus.analogBus[ch].value,
                  bus.analogBus[ch].isDrived ? "YES" : "NO ",
                  m.isInverted ? "(INV)" : "     ");
  }

// ==========================================================
// 2. DIGITAL CHANNELS DEBUG
// ==========================================================

  Serial.println(F("[DIGITAL CHANNELS]"));
  for (uint8_t i = 0; i < InputDigitalMapCount; i++) {
    
      // --- Safe data access ---
    const InputDigitalMap m = InputDigitalMapArray[i];
    uint8_t ch = static_cast<uint8_t>(m.busChannel);
    
      // --- Console output ---
    Serial.printf("  Map #%d: [DevID:%d] -> [BusCh:%d] | State: %5s | Drived: %s %s\n", 
                  i, 
                  static_cast<uint8_t>(m.devID), 
                  ch,
                  bus.digitalBus[ch].value ? "ON " : "OFF",
                  bus.digitalBus[ch].isDrived ? "YES" : "NO ",
                  m.isInverted ? "(INV)" : "     ");
  }
  Serial.println(F("---------------------------------------\n"));
}
