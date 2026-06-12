/******************************************************************************
 * @file module.h
 * Template module header.
 *
 * @details Exposes the public API and configuration types for TemplateModule.
 *   The module can be enabled or disabled at runtime, and its output polarity
 *   can be inverted through `TemplateConfig`.
 *
 *   Typical lifecycle: construct -> begin() -> enable() -> update() in loop.
 *****************************************************************************/
#pragma once


// =============================================================================
// 1. INCLUDES
// =============================================================================

#include <stdint.h>



// =============================================================================
// 2. CONSTANTS AND ENUMS
// =============================================================================

	/// Default working frequency in Hz.
static constexpr uint32_t kDefaultFrequencyHz = 1000;

	/// Operating mode for this module.
enum class TemplateMode : uint8_t {
  MODE_A = 0,   ///< Standard two-quadrant operation.
  MODE_B        ///< Four-quadrant operation with inversion.
};



// =============================================================================
// 3. STRUCTURES
// =============================================================================

	/// Configuration payload for module initialization.
struct TemplateConfig {
  uint32_t frequencyHz;   ///< Working frequency in Hz.
  bool invertPolarity;    ///< Set true to invert output polarity.
  TemplateMode mode;      ///< Selected operating mode.
};



// =============================================================================
// 4. CLASS DEFINITION
// =============================================================================

/**
 * @class TemplateModule
 * Minimal runtime module template.
 *
 * @details Demonstrates sectioned layout, Doxygen one-line API docs, member
 *   inline docs, and private state kept in lowerCamelCase with leading `_`.
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

    /// Return the configured operating mode.
  TemplateMode mode() const;


private:

	bool _isEnabled;              ///< Current enabled state.
	uint32_t _frequencyHz;        ///< Runtime frequency in Hz.
	bool _invertPolarity;         ///< Runtime polarity inversion flag.
	TemplateMode _mode;           ///< Current operating mode.
};

// EOF module.h
