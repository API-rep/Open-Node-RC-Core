/*!****************************************************************************
 * @file    volvo_A60H_bruder.cpp
 * @brief   Volvo A60H Bruder — vehicle-level device configuration.
 *
 * @details Defines the signal device array for the Volvo A60H.
 *   Simulation device configuration moved to sim_config.cpp.bak pending
 *   the CbChain refactor.
 *******************************************************************************
 */

#include "volvo_A60H_bruder.h"


// =============================================================================
// 1. SIGNAL DEVICES
// =============================================================================

/**
 * @brief Signal device table for the Volvo A60H Bruder.
 *
 * @details Maps each discrete signal source to its ComBus digital channel
 *   and functional role (DevUsage).  Output modules (sound, lighting) iterate
 *   this table at init to build their channel binding tables automatically —
 *   no hard-coded channel indices in those modules.
 *
 *   Entries with DevUsage::UNDEFINED have no standard sound binding; they are
 *   handled by dedicated interpreter logic (engine key FSM, indicator mux…).
 */
SigDevice sigDevArray[SIG_COUNT] = {
  {
    .ID             = HORN_SIG,
    .infoName       = "horn",
    .usage          = DevUsage::SIG_HORN,
    .digitalChannel = DigitalComBusID::HORN_BTN
  },
  {
    .ID             = LIGHTS_SIG,
    .infoName       = "lights",
    .usage          = DevUsage::SIG_LIGHT,
    .digitalChannel = DigitalComBusID::LIGHTS
  },
  {
    .ID             = KEY_SIG,
    .infoName       = "ignition key",   ///< interpreted by engine on/off FSM in sound_interpreter
    .usage          = DevUsage::UNDEFINED,
    .digitalChannel = DigitalComBusID::KEY_BTN
  },
  {
    .ID             = INDIC_L_SIG,
    .infoName       = "indicator left", ///< encoded into FUNCTION_L mux by sound_core
    .usage          = DevUsage::UNDEFINED,
    .digitalChannel = DigitalComBusID::INDICATOR_LEFT
  },
  {
    .ID             = INDIC_R_SIG,
    .infoName       = "indicator right",
    .usage          = DevUsage::UNDEFINED,
    .digitalChannel = DigitalComBusID::INDICATOR_RIGHT
  },
  {
    .ID             = HAZARDS_SIG,
    .infoName       = "hazards",
    .usage          = DevUsage::UNDEFINED,
    .digitalChannel = DigitalComBusID::HAZARDS
  },
};


// EOF volvo_A60H_bruder.cpp
