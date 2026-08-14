#ifdef HAS_FAILSAFE
FAILSAFE,           ///< Failsafe aggregator channel — set when any FAILSAFE_X contributor reports a fault (FAILSAFE itself is the aggregator, not a source).
#ifdef HAS_VBAT_FAILSAFE
  #include <core/config/vbat/combus_ids_digital_vbat_failsafe.inc>
#endif
FAILSAFE_END,        ///< Range marker — first index after Failsafe group.
#endif
