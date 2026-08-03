// --- Wire channels (0 .. WIRE_END-1) --- transmitted over UART ---
{ .infoName = "steering channel",        .value = CbusNeutral, .layer = ChanLayer::REMOTE  },
{ .infoName = "rpm channel",             .value = 0u,          .layer = ChanLayer::LOCAL }, ///< Engine RPM — written by SIM_RPM pipeline, transmitted to sound node.
{ .infoName = "dump actuators channel",  .value = CbusNeutral, .layer = ChanLayer::REMOTE  },
{ .infoName = "esc speed channel",       .value = CbusNeutral, .layer = ChanLayer::LOCAL },
{ .infoName = "gear channel",            .value = 1u,          .layer = ChanLayer::LOCAL }, ///< Virtual gear (1–3) from motion pipeline; direction in ESC_REVERSE.
{ .infoName = "drive state channel",     .value = 0u,          .layer = ChanLayer::LOCAL }, ///< DriveState 3-bit bitmask — see DriveStateBus::encode/decode in simulation_struct.h.