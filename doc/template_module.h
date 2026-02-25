/******************************************************************************
 * @file template_module.h
 * [Short module purpose]
 *
 * @details [Optional: architecture role, constraints, usage scope]
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. INCLUDES
// =============================================================================

#include <stdint.h>



// =============================================================================
// 2. CONSTANTS AND ENUMS
// =============================================================================

	/// Operating mode for this module.
enum class TemplateMode : uint8_t {
  MODE_A = 0,
  MODE_B
};



// =============================================================================
// 3. STRUCTURES
// =============================================================================

	/// Configuration payload for module initialization.
struct TemplateConfig {
  uint32_t frequencyHz;   ///< Working frequency in Hz.
  bool invertPolarity;    ///< Set true to invert output polarity.
};



// =============================================================================
// 4. CLASS DEFINITION
// =============================================================================

/**
 * @class TemplateModule
 * [One-line API purpose]
 *
 * @details [Optional short class behavior overview]
 */

class TemplateModule {

public:

    /// Build a module with default internal state.
  TemplateModule();

    /// Destroy module resources safely.
  ~TemplateModule();

    /// Initialize module runtime behavior.
  bool begin(const TemplateConfig &config);

    /// Execute one control/update cycle.
  void update();

    /// Enable runtime output.
  void enable();

    /// Disable runtime output.
  void disable();

    /// Return true if module is enabled.
  bool isEnabled() const;


private:

	bool _isEnabled;              ///< Current enabled state.
	uint32_t _frequencyHz;        ///< Runtime frequency in Hz.
	bool _invertPolarity;         ///< Runtime polarity inversion flag.
};

// EOF template_module.h
