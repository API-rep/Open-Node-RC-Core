{ .infoName = "analog brake channel",  .value = 0u,          .layer = ChanLayer::REMOTE }, ///< L2 trigger input — written by input_manager, read by sim_brake.
{ .infoName = "sub-gear channel",      .value = 0u,          .layer = ChanLayer::LOCAL  }, ///< Active sub-gear index — written by sim_gear.
{ .infoName = "dump ramp channel",     .value = CbusNeutral, .layer = ChanLayer::LOCAL  }, ///< Inertia-smoothed dump position — written by sim_ramp, read by DUMP_ACTUATOR.
{ .infoName = "steering ramp channel", .value = CbusNeutral, .layer = ChanLayer::LOCAL  }, ///< Inertia-smoothed steering position — written by sim_ramp, read by STEERING.
{ .infoName = "throttle stick (raw)",  .value = CbusNeutral, .layer = ChanLayer::REMOTE }, ///< Raw throttle stick — written by input_manager, conditioned by INPUT_THROTTLE chain.
{ .infoName = "throttle bus (cond)",   .value = CbusNeutral, .layer = ChanLayer::LOCAL  }, ///< Conditioned throttle — INPUT_THROTTLE output, read by SIM_THROTTLE pipeline.