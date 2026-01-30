#include "hw_init_srv.h"

#include "../config.h"

void servoInit() {
  if (machine.srvDevCount > 0) {
      for (int i = 0; i < machine.srvDevCount; i++) {
        // init servo devices
    }
  }
  else {
      // do nothing
  }
}