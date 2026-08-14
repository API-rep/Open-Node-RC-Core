// --- Wire channels (0 .. WIRE_END-1) — transmitted over UART ---
{ .infoName = "horn channel",            .value = false, .layer = ChanLayer::REMOTE },
{ .infoName = "lights channel",          .value = false, .layer = ChanLayer::REMOTE },
{ .infoName = "key channel",             .value = false, .layer = ChanLayer::REMOTE },
{ .infoName = "battery low channel",     .value = false, .layer = ChanLayer::LOCAL  },
{ .infoName = "indicator left channel",  .value = false, .layer = ChanLayer::REMOTE },
{ .infoName = "indicator right channel", .value = false, .layer = ChanLayer::REMOTE },
{ .infoName = "hazards channel",         .value = false, .layer = ChanLayer::REMOTE },
{ .infoName = "high beam channel",       .value = false, .layer = ChanLayer::REMOTE },
{ .infoName = "roof light channel",      .value = false, .layer = ChanLayer::REMOTE },
{ .infoName = "low beam channel",        .value = false, .layer = ChanLayer::REMOTE },
{ .infoName = "braking channel",         .value = false, .layer = ChanLayer::LOCAL  },
{ .infoName = "key active channel",      .value = false, .layer = ChanLayer::LOCAL  }, ///< Processed ignition key state — written by key_runlevel chain, transmitted to sound node.

// ---- Common umbrella ------------------------------------------
// Cross-machine-type Failsafe Digital REMOTE channel init values.
// Pulled in only when HAS_FAILSAFE is defined. Currently empty.
#include <core/config/machines/common/combus/combus_remote_digital_common.inc>
