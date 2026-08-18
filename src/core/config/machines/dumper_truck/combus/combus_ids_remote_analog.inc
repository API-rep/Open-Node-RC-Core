ESC_RPM_BUS = 0,         ///< Engine RPM magnitude [0..maxRpm], pre-inertia, post-ratio — written by SIM_THROTTLE. Transmitted to sound node. (ID 0 — STEERING_BUS migrated to steering_bus.cb A9.1, DUMP_BUS migrated to dump_bus.cb A9.2, ESC_SPEED_BUS migrated to esc_speed_bus.cb A9.5)
GEAR,              ///< Active virtual gear (1–3) — derived from ESC_SPEED_BUS deviation by motion pipeline. Never 0; direction encoded separately in ESC_REVERSE. (ID 1 — was 2 before A9.2, was 3 before A9.5)
DRIVE_STATE_BUS,   ///< DriveState 3-bit bitmask — see DriveStateBus::encode/decode in simulation_struct.h. (ID 2 — was 3 before A9.2, was 4 before A9.5)
                     ///<   bit0 ACTIVE | bit1 FWD | bit2 BRAKE  (0 = standing)
