{ .infoName = "always on (local)",      .value = true,  .layer = ChanLayer::SYSTEM }, ///< Stays true: continuous sources driven by volMod.
{ .infoName = "siren (local)",          .value = false, .layer = ChanLayer::SYSTEM }, ///< Siren/cannon mode toggle.
{ .infoName = "indicator tick (local)", .value = false, .layer = ChanLayer::SYSTEM }, ///< Combined indicator gate.
{ .infoName = "esc reverse (local)",    .value = false, .layer = ChanLayer::SYSTEM }, ///< ESC reverse state.
{ .infoName = "sound1 (local)",         .value = false, .layer = ChanLayer::SYSTEM }, ///< One-shot, cleared by EffectsGen.
{ .infoName = "wastegate (local)",      .value = false, .layer = ChanLayer::SYSTEM }, ///< One-shot, cleared by EffectsGen.
{ .infoName = "brake (local)",          .value = false, .layer = ChanLayer::SYSTEM }, ///< Resolved by EffectsGen (EngineGen||sim).
{ .infoName = "parking brake (local)",  .value = false, .layer = ChanLayer::SYSTEM }, ///< EngineGen level flag.
{ .infoName = "shifting (local)",       .value = false, .layer = ChanLayer::SYSTEM }, ///< One-shot, cleared by EffectsGen.
{ .infoName = "coupling (local)",       .value = false, .layer = ChanLayer::SYSTEM }, ///< One-shot, cleared by EffectsGen.
{ .infoName = "uncoupling (local)",     .value = false, .layer = ChanLayer::SYSTEM }, ///< One-shot, cleared by EffectsGen.
{ .infoName = "bucket rattle (local)",  .value = false, .layer = ChanLayer::SYSTEM }, ///< One-shot, cleared by EffectsGen.
{ .infoName = "out of fuel (local)",    .value = false, .layer = ChanLayer::SYSTEM }, ///< One-shot, cleared by EffectsGen.