{ .infoName = "direct drive (local)",     .value = false, .layer = ChanLayer::LOCAL  }, ///< Toggle state — written by main.cpp toggle logic, read by cb_bypass_fn.
{ .infoName = "direct drive btn (local)", .value = false, .layer = ChanLayer::REMOTE }, ///< Raw OPTIONS button — written by input_update every cycle.
{ .infoName = "manual gear set (local)",  .value = false, .layer = ChanLayer::LOCAL  }, ///< Manual gear mode active — written by INPUT chain, read by cb_bypass_fn (GEAR claim guard).
{ .infoName = "subgear set btn (local)",  .value = false, .layer = ChanLayer::REMOTE }, ///< Raw sub-gear toggle button — written by input_update; processed by ctrl chain.
{ .infoName = "gear up btn (local)",      .value = false, .layer = ChanLayer::REMOTE }, ///< Gear up button — written by input_update; read by INPUT chain.
{ .infoName = "gear down btn (local)",    .value = false, .layer = ChanLayer::REMOTE }, ///< Gear down button — written by input_update; read by INPUT chain.
{ .infoName = "gear shifting (local)",    .value = false, .layer = ChanLayer::LOCAL  }, ///< Upshift damp window — set/cleared by gear_upshift_damp_fn.
{ .infoName = "cruise toggle btn (local)",.value = false, .layer = ChanLayer::REMOTE }, ///< Raw □ button — written by input_manager every cycle.
{ .infoName = "cruise update btn (local)",.value = false, .layer = ChanLayer::REMOTE }, ///< Raw L3 button — written by input_manager every cycle.
{ .infoName = "cruise active (local)",    .value = false, .layer = ChanLayer::LOCAL  }, ///< Normal cruise active — maintained by INPUT cruise chain.