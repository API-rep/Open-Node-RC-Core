/*****************************************************************************
 * @file hw_init_debug.cpp
 * @brief Hardware configuration diagnostic and verbose output
 *****************************************************************************/

#include "hw_init.h"

#if defined(DEBUG_HW) || defined(DEBUG_ALL)

// =============================================================================
// 1. LOCAL HELPERS
// =============================================================================

/**
 * @brief Return readable name for a DC driver mode
 * @param mode DC driver mode enum value
 * @return Constant readable mode label
 */

static const char* dcDriverModeName(DcDrvMode mode) {
	if (mode == DcDrvMode::TWO_WAY_NEUTRAL_CENTER) {
		return "TWO_WAY_NEUTRAL_CENTER";
	}

	if (mode == DcDrvMode::ONE_WAY) {
		return "ONE_WAY";
	}

	return "UNDEFINED";
}

/**
 * @brief Emit ANSI gray style prefix for inherited values
 * @param isInherited True when the value matches parent-derived configuration
 */

static void printInheritedColorStart(bool isInherited) {
  #if DEBUG_SERIAL_ANSI
  if (isInherited) {
    Serial.printf("\033[3;37m");
  }
  #else
  (void)isInherited;
  #endif
}

/**
 * @brief Emit inherited value marker prefix
 * @param isInherited True when the value matches parent-derived configuration
 */

static void printInheritedPrefix(bool isInherited) {
  if (isInherited) {
    Serial.print("~");
  }
}

/**
 * @brief Emit ANSI reset style suffix for inherited values
 * @param isInherited True when the value matches parent-derived configuration
 */

static void printInheritedColorEnd(bool isInherited) {
  #if DEBUG_SERIAL_ANSI
  if (isInherited) {
    Serial.printf("\033[0m");
  }
  #else
  (void)isInherited;
  #endif
}

/**
 * @brief Resolve parent DC device from parent ID
 * @param child Child device descriptor
 * @return Parent pointer when resolvable, otherwise nullptr
 */

static const DcDevice* dcDevParentPtr(const DcDevice &child) {
	if (!child.parentID.has_value()) {
		return nullptr;
	}

	uint8_t parentIdx = child.parentID.value();
	if (parentIdx >= static_cast<uint8_t>(machine.dcDevCount)) {
		return nullptr;
	}

	return &machine.dcDev[parentIdx];
}


// =============================================================================
// 2. HARDWARE DEBUG OUTPUTS
// =============================================================================

/**
 * @brief Print hardware configuration for debugging
 */

void debugVehicleConfig() {
  Serial.println(F("\n========================================"));
  Serial.println(F("       VEHICLE CONFIGURATION DEBUG      "));
  Serial.println(F("========================================"));

  Serial.printf("Machine Name = %s\n", machine.infoName);
  Serial.printf("Global Speed = FW=%.1f%% | BK=%.1f%%\n", machine.maxFwSpeed, machine.maxBackSpeed);
  Serial.printf("DC Devices   = %d\n", machine.dcDevCount);
  Serial.printf("Servo Devices= %d\n", machine.srvDevCount);
  Serial.println(F("----------------------------------------"));
  Serial.flush();

		// --- 1. DC devices hybrid output (normal mode) ---
  if (machine.dcDev != nullptr && machine.dcDevCount > 0) {
    for (int i = 0; i < machine.dcDevCount; i++) {
      const DcDevice* d = &machine.dcDev[i];
      const DcDevice* parent = dcDevParentPtr(*d);
      bool hasParent = (parent != nullptr);

      bool freqInherited = hasParent && d->pwmFreq.has_value() && parent->pwmFreq.has_value() &&
                           (d->pwmFreq.value() == parent->pwmFreq.value());
      bool polInherited = hasParent && (d->polInv == parent->polInv);
      bool modeInherited = hasParent && (d->mode == parent->mode);

      float fwSpeed = d->maxFwSpeed.value_or(100.0f);
      float bkSpeed = d->maxBackSpeed.value_or(100.0f);
      bool fwInherited = hasParent && d->maxFwSpeed.has_value() && parent->maxFwSpeed.has_value() &&
                         (fwSpeed == parent->maxFwSpeed.value());
      bool bkInherited = hasParent && d->maxBackSpeed.has_value() && parent->maxBackSpeed.has_value() &&
                         (bkSpeed == parent->maxBackSpeed.value());

      Serial.printf("[HW][DRV] DC_DEV #%d - %s\n", d->ID, d->infoName ? d->infoName : "UNNAMED");

      if (d->drvPort != nullptr) {
        Serial.printf("  > Board Port: %s (ID=%d)", d->drvPort->infoName ? d->drvPort->infoName : "N/A", d->drvPort->ID);
      } else {
        Serial.print(F("  > Board Port: N/A"));
      }

      if (hasParent) {
        Serial.printf(" | Parent=DC_DEV #%d (CLONE)", parent->ID);
      }
      Serial.println();

      if (d->drvPort != nullptr) {
        Serial.printf("  > Pins: PWM=%d BRK=%d EN=%d SLP=%d FLT=%d\n",
                      d->drvPort->pwmPin.value_or(-1),
                      d->drvPort->brkPin.value_or(-1),
                      d->drvPort->enPin.value_or(-1),
                      d->drvPort->slpPin.value_or(-1),
                      d->drvPort->fltPin.value_or(-1));
      } else {
        Serial.println(F("  > Pins: PWM=-1 BRK=-1 EN=-1 SLP=-1 FLT=-1"));
      }

      Serial.print(F("  > Config:"));
      if (d->pwmFreq.has_value()) {
        Serial.print(F(" Freq="));
        printInheritedColorStart(freqInherited);
        printInheritedPrefix(freqInherited);
        Serial.print(d->pwmFreq.value());
        Serial.print(F(" Hz"));
        printInheritedColorEnd(freqInherited);
      } else {
        Serial.print(F(" Freq=N/A"));
      }
      Serial.print(F(" | PolInv="));
      printInheritedColorStart(polInherited);
      printInheritedPrefix(polInherited);
      Serial.print(d->polInv ? "YES" : "NO");
      printInheritedColorEnd(polInherited);

      Serial.print(F(" | Mode="));
      printInheritedColorStart(modeInherited);
      printInheritedPrefix(modeInherited);
      Serial.print(dcDriverModeName(d->mode));
      Serial.print(F(" ("));
      Serial.print(static_cast<int>(d->mode));
      Serial.print(F(")"));
      printInheritedColorEnd(modeInherited);
      Serial.println();

      if (d->comChannel.has_value()) {
        uint8_t chId = static_cast<uint8_t>(d->comChannel.value());
        const char* chName = (comBus.analogBus[chId].infoName != nullptr) ? comBus.analogBus[chId].infoName : "AN_CH";
        Serial.printf("  > Com Ch: %s (ID=%d)\n", chName, chId);
      } else {
        Serial.println(F("  > Com Ch: N/A"));
      }

      Serial.print(F("  > Max Speed: FW="));
      printInheritedColorStart(fwInherited);
      printInheritedPrefix(fwInherited);
      Serial.print(fwSpeed, 1);
      Serial.print(F("%"));
      printInheritedColorEnd(fwInherited);
      Serial.print(F(" | BK="));
      printInheritedColorStart(bkInherited);
      printInheritedPrefix(bkInherited);
      Serial.print(bkSpeed, 1);
      Serial.print(F("%"));
      printInheritedColorEnd(bkInherited);
      Serial.println();

      Serial.println();
      Serial.flush();
    }
  }

		// --- 2. Servo devices summary ---
  if (machine.srvDev != nullptr && machine.srvDevCount > 0) {
    Serial.println(F("[HW][SRV] Servo devices summary"));
    for (int i = 0; i < machine.srvDevCount; i++) {
      const SrvDevice* s = &machine.srvDev[i];
      Serial.printf("  > SRV_DEV #%d - %s\n", s->ID, s->infoName ? s->infoName : "UNNAMED");
    }
  }

  Serial.println(F("========================================\n"));
}

#endif // HW_INIT_DEBUG_ENABLED

// EOF hw_init_debug.cpp
