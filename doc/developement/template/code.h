/******************************************************************************
 * @file code.h
 * Comprehensive template demonstrating all project coding conventions.
 *
 * @details This header file serves as a complete reference for:
 *   - File header format with Doxygen documentation
 *   - Section separators and organization
 *   - Include ordering and #pragma once usage
 *   - Constant and enum definitions with proper naming
 *   - Structure declarations with member documentation
 *   - Class design with access sections
 *   - Function prototypes with complete Doxygen documentation
 *   - Inline documentation style using ///<
 *
 *   The file demonstrates the caller perspective: what the API provides
 *   and why each element exists, without implementation details.
 ******************************************************************************/

#pragma once

// =============================================================================
// 1. INCLUDES
// =============================================================================

	// Standard headers last, after project headers
#include <cstdint>


// =============================================================================
// 2. CONSTANTS AND ENUMS
// =============================================================================

	/// Default system tick frequency in Hz.
static constexpr uint32_t kDefaultTickFrequency = 1000;

	/// Maximum number of channels supported by this module.
static constexpr uint8_t kMaxChannelCount = 8;

	/// Operating mode selector for the processing engine.
enum class ProcessingMode : uint8_t {
	MODE_STANDBY = 0,   ///< Low-power idle state, no processing.
	MODE_NORMAL,        ///< Standard operation with full features.
	MODE_FAST           ///< High-speed mode with reduced safety checks.
};


// =============================================================================
// 3. FORWARD DECLARATIONS
// =============================================================================

	// Forward declaration to avoid circular includes in complex modules.
class ProcessingEngine;


// =============================================================================
// 4. DATA STRUCTURES
// =============================================================================

/**
 * @brief Configuration parameters for channel initialization.
 *
 * @details Holds all parameters needed to configure a single processing channel.
 *   Used during setup to define channel behavior before the processing loop begins.
 */

struct ChannelConfig {
	uint8_t channelId;        ///< Unique identifier for this channel (0 to kMaxChannelCount-1).
	uint32_t sampleRate;      ///< Sampling frequency in Hz. Must be > 0.
	ProcessingMode mode;      ///< Selected operating mode for this channel.
	bool enableFiltering;     ///< Set true to enable input signal filtering.
};


/**
 * @brief Runtime statistics for monitoring channel health.
 *
 * @details Provides visibility into channel operation for debugging and monitoring.
 *   Updated by the processing engine after each cycle.
 */


struct ChannelStats {
	uint32_t cycleCount;      ///< Total number of processing cycles completed.
	uint32_t errorCount;      ///< Cumulative error events since initialization.
	float averageLoad;        ///< Average processing load as percentage (0.0 to 100.0).
};


// =============================================================================
// 5. CLASS DEFINITION
// =============================================================================

/**
 * @class ProcessingEngine
 * Core processing unit for signal transformation and output generation.
 *
 * @details Manages multiple processing channels with configurable modes.
 *   Handles initialization, runtime control, and health monitoring.
 *
 *   Typical lifecycle:
 *   1. Construct with default state.
 *   2. Configure channels using begin().
 *   3. Enable processing with enable().
 *   4. Call update() periodically in the main loop.
 *   5. Monitor health via getStats().
 */

class ProcessingEngine {

public:

	/**
	 * @brief Default constructor. Creates engine in safe initial state.
	 *
	 * @details All channels disabled, counters zeroed, mode set to standby.
	 */
	ProcessingEngine();

	/**
	 * @brief Destructor. Performs safe shutdown sequence.
	 *
	 * @details Disables all channels and releases resources.
	 */


	~ProcessingEngine();

	/**
	 * @brief Initialize the engine with channel configuration.
	 *
	 * @details Validates configuration and prepares internal state.
	 *   Must be called before enable().
	 *
	 * @param config Channel configuration array. Must not be null.
	 * @param count Number of channels to configure (max kMaxChannelCount).
	 * @return True when configuration is valid and applied, false otherwise.
	 */
	bool begin(const ChannelConfig* config, uint8_t count);

	/**
	 * @brief Execute one processing cycle for all enabled channels.
	 *
	 * @details Non-blocking operation. Processes each channel according to
	 *   its configured mode and updates runtime statistics.
	 */
	void update();

	/**
	 * @brief Enable the processing engine.
	 *
	 * @details Activates all configured channels. update() will process
	 *   channels when enabled.
	 */
	void enable();

	/**
	 * @brief Disable the processing engine.
	 *
	 * @details Stops processing and puts channels in safe state.
	 */
	void disable();

	/**
	 * @brief Check if the engine is currently enabled.
	 *
	 * @return True when enabled and processing, false when disabled.
	 */
	bool isEnabled() const;

	/**
	 * @brief Retrieve current runtime statistics.
	 *
	 * @details Provides snapshot of engine health and performance.
	 *
	 * @param channelId Channel to query (0 to kMaxChannelCount-1).
	 * @return Statistics structure for the specified channel.
	 */
	ChannelStats getStats(uint8_t channelId) const;

	/**
	 * @brief Get the number of currently configured channels.
	 *
	 * @return Channel count set during begin(), or 0 if not initialized.
	 */
	uint8_t getChannelCount() const;


private:

	bool _isEnabled;                    ///< True when engine is actively processing.
	uint8_t _channelCount;              ///< Number of channels configured (0 to kMaxChannelCount).
	ChannelConfig _channelConfig[kMaxChannelCount];  ///< Cached configuration for each channel.
	ChannelStats _channelStats[kMaxChannelCount];    ///< Runtime statistics for each channel.
};

// =============================================================================
// 6. STANDALONE FUNCTIONS
// =============================================================================

/**
 * @brief System-wide initialization helper.
 *
 * @details Performs global setup required before any ProcessingEngine instance.
 *   Configures shared resources and validates hardware presence.
 *
 * @return True when system is ready for engine initialization.
 */
bool systemInit();

/**
 * @brief Convert ProcessingMode to human-readable string.
 *
 * @details Utility for logging and debugging. Returns pointer to static string.
 *
 * @param mode The mode to convert.
 * @return String representation of the mode (e.g., "MODE_NORMAL").
 */
const char* modeToString(ProcessingMode mode);

// EOF code.h