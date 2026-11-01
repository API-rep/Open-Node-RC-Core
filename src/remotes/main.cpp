/******************************************************************************
 * @file  main.cpp
 * @brief Remote: entry point (chantier 4, embryo).
 *
 * @details The embryo opens the link to the machine it drives and runs it:
 *   the remote sends its inputs (the BASE view of the machine type,
 *   mirrored).  No physical input yet (INPUT_NONE, step 4.2): the channels
 *   keep their safe value.
 *****************************************************************************/

#include <Arduino.h>

#include <remotes/com/remote_links.h>


void setup() {
    Serial.begin(115200);
    remote_links_init();
}

void loop() {
    remote_links_update();
    delay(2);
}

// EOF main.cpp
