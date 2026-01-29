#include "config/config.h"

#include "ESP32_8M_6S.h"


DriverPort drvPortArray[DRV_PORT_COUNT] = {

  { 
    .ID = DRV_PORT_1A,
    .infoName = "M-1A", 
    .pwmPin = 32,
    .brkPin = BRK_PIN[CH_A],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_2A,
    .infoName = "M-2A", 
    .pwmPin = 27,
    .brkPin = BRK_PIN[CH_A],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_3A,
    .infoName = "M-3A", 
    .pwmPin = 14,
    .brkPin = BRK_PIN[CH_A],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_4A,
    .infoName = "M-4A", 
    .pwmPin = 12,
    .brkPin = BRK_PIN[CH_A],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_1B,
    .infoName = "M-1B", 
    .pwmPin = 21,
    .brkPin = BRK_PIN[CH_B],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },
  
  { 
    .ID = DRV_PORT_2B,
    .infoName = "M-2B", 
    .pwmPin = 5,
    .brkPin = BRK_PIN[CH_B],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_3B,
    .infoName = "M-3B", 
    .pwmPin = 0,
    .brkPin = BRK_PIN[CH_B],
    .enPin  = DRV_EN_PIN,
    .slpPin = DRV_SLP_PIN,
    .fltPin = DRV_FLT_PIN
  },

  { 
    .ID = DRV_PORT_4B,
    .infoName = "M-4B", 
    .pwmPin = 2,
    .brkPin = BRK_PIN[CH_B],
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