// input_channels_digital.inc — Raw input acquisition DIGITAL channels (LOCAL, direction=none)
// =============================================================================
// LEGACY HAND-WRITTEN MIRROR of input.cb (digital channel descriptors).
//
// Consumed by combus.cpp arrays:
//   DigitalComBusArray[DigitalComBusID::CH_COUNT] = {
//     #include <core/config/machines/dumper_truck/combus/combus_channels_remote_digital.inc>
//     #include <machines/.../combus_channels_local_digital.inc>
//     #include <machines/.../combus_channels_system_digital.inc>
//     #include <core/config/machines/dumper_truck/inputs/input_channels_digital.inc>
//   };
//
// Each entry matches { infoName, value=false, layer=ChanLayer::LOCAL, direction=Direction::None }.
//
// Source chain: input_update() fills these every loop → consumed by INPUT/SIM processors.
// =============================================================================

{ .infoName = "cruise toggle button (raw)",   .value = false, .layer = ChanLayer::LOCAL, .direction = Direction::None },  ///< CRUISE_TOGGLE_BTN
{ .infoName = "cruise update button (raw)",   .value = false, .layer = ChanLayer::LOCAL, .direction = Direction::None },  ///< CRUISE_UPDATE_BTN
{ .infoName = "direct-drive toggle button (raw)", .value = false, .layer = ChanLayer::LOCAL, .direction = Direction::None },  ///< DIRECT_DRIVE_BTN
{ .infoName = "gear down button (raw)",       .value = false, .layer = ChanLayer::LOCAL, .direction = Direction::None },  ///< GEAR_DOWN_BTN
{ .infoName = "gear up button (raw)",         .value = false, .layer = ChanLayer::LOCAL, .direction = Direction::None },  ///< GEAR_UP_BTN
{ .infoName = "sub-gear toggle button (raw)", .value = false, .layer = ChanLayer::LOCAL, .direction = Direction::None },  ///< SUBGEAR_SET_BTN