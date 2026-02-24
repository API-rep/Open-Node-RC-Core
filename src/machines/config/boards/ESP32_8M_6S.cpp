#include "ESP32_8M_6S.h"


DriverPort drvPortArray[DRV_PORT_COUNT] = {

  { 
    .ID = DRV_PORT_1A,
    .infoName = "M-1A", 
    .driverModel = &DC_DRIVER_MODEL,
    .pwmPin = 32,
    .brkPin = DECAY_PIN[CH_A],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_2A,
    .infoName = "M-2A", 
    .driverModel = &DC_DRIVER_MODEL,
    .pwmPin = 27,
    .brkPin = DECAY_PIN[CH_A],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_3A,
    .infoName = "M-3A", 
    .driverModel = &DC_DRIVER_MODEL,
    .pwmPin = 14,
    .brkPin = DECAY_PIN[CH_A],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_4A,
    .infoName = "M-4A", 
    .driverModel = &DC_DRIVER_MODEL,
    .pwmPin = 12,
    .brkPin = DECAY_PIN[CH_A],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_1B,
    .infoName = "M-1B", 
    .driverModel = &DC_DRIVER_MODEL,
    .pwmPin = 21,
    .brkPin = DECAY_PIN[CH_B],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },
  
  { 
    .ID = DRV_PORT_2B,
    .infoName = "M-2B", 
    .driverModel = &DC_DRIVER_MODEL,
    .pwmPin = 5,
    .brkPin = DECAY_PIN[CH_B],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_3B,
    .infoName = "M-3B", 
    .driverModel = &DC_DRIVER_MODEL,
    .pwmPin = 0,
    .brkPin = DECAY_PIN[CH_B],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_4B,
    .infoName = "M-4B", 
    .driverModel = &DC_DRIVER_MODEL,
    .pwmPin = 2,
    .brkPin = DECAY_PIN[CH_B],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },
};



ServoPort srvPortArray[SRV_PORT_COUNT]= {
  { 
    .ID = SRV_PORT_1A,
    .infoName = "SRV-1A", 
    .pwmPin = 19,
    .sensePin = SRV_A_SENSE_PIN
  },

  { 
    .ID = SRV_PORT_2A,
    .infoName = "SRV-2A", 
    .pwmPin = 22,
    .sensePin = SRV_A_SENSE_PIN
  },

  { 
    .ID = SRV_PORT_3A,
    .infoName = "SRV-3A", 
    .pwmPin = 23,
    .sensePin = SRV_A_SENSE_PIN
  },

  { 
    .ID = SRV_PORT_1B,
    .infoName = "SRV-1B", 
    .pwmPin = 4,
    .sensePin = SRV_B_SENSE_PIN
  },

  { 
    .ID = SRV_PORT_2B,
    .infoName = "SRV-2B", 
    .pwmPin = 16,
    .sensePin = SRV_B_SENSE_PIN
  },

  { 
    .ID = SRV_PORT_3B,
    .infoName = "SRV-3B", 
    .pwmPin = 17,
    .sensePin = SRV_B_SENSE_PIN
  },
};