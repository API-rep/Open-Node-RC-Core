/******************************************************************************
 * @file  main.cpp
 * @brief Remote: entry point.
 *
 * @details Opens the link to the machine it drives and runs it: the remote sends its
 *   inputs, the BASE view of the machine type, mirrored.
 *
 *   TODO(chantier 4): no physical input yet (INPUT_NONE): the channels keep their
 *   safe value.
 *****************************************************************************/

#include <Arduino.h>

#include <remotes/com/remote_links.h>



void setup() {
	Serial.begin(115200);
	remote_links_init();
}

void loop() {
	remote_links_update();

		// Pace the loop: each link keeps its own timer
	delay(2);
}

// EOF main.cpp
