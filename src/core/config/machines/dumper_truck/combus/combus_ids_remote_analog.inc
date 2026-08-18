ESC_RPM_BUS = 0,         ///< Engine RPM magnitude [0..maxRpm], pre-inertia, post-ratio — written by SIM_THROTTLE. Transmitted to sound node. (ID 0 — STEERING_BUS migrated to steering_bus.cb, A9.1)
DUMP_BUS,
ESC_SPEED_BUS,       ///< Inertia-smoothed speed — drives ESC output + currentSpeed (owner: SYSTEM_EXT)
GEAR,              ///< Active virtual gear (1–3) — derived from ESC_SPEED_BUS deviation by motion pipeline. Never 0; direction encoded separately in ESC_REVERSE.
DRIVE_STATE_BUS,   ///< DriveState 3-bit bitmask — see DriveStateBus::encode/decode in simulation_struct.h.
                     ///<   bit0 ACTIVE | bit1 FWD | bit2 BRAKE  (0 = standing)