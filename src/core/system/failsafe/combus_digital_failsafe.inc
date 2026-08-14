#ifdef HAS_FAILSAFE
{ .infoName = "failsafe channel",     .value = false, .layer = ChanLayer::REMOTE }, ///< Failsafe aggregator channel — set when any FAILSAFE_X contributor reports a fault.
#ifdef HAS_VBAT_FAILSAFE
  #include <core/config/vbat/combus_digital_vbat_failsafe.inc>
#endif
#endif
