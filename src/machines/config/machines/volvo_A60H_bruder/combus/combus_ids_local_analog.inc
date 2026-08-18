BRAKE_BUS,               ///< Brake pedal input (L2, ComBus domain) — written by input_manager, read by sim_brake.
SUBGEAR_BUS,              ///< Active sub-gear index (1..N, 0 = inactive) — written by sim_gear.
DUMP_RAMPED_BUS,          ///< Inertia-smoothed dump position — written by sim_ramp, read by DUMP_ACTUATOR.
STEERING_RAMPED_BUS,      ///< Inertia-smoothed steering position — written by sim_ramp, read by STEERING.
THROTTLE_STICK,           ///< Raw throttle stick — written by input_manager, conditioned by INPUT_THROTTLE chain. (THROTTLE_BUS migrated to throttle_bus.cb, A9.4)
