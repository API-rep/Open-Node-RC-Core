#pragma once

#include <struct/combus_struct.h>

#include <core/config/combus/combus.h>

/**
 * @brief Fonctions de gestion du module d'entrée (PS4, etc.)
 */

// Initialise le module d'entrée (ex: PS4.begin)
void input_setup();

// À appeler dans le loop() pour synchroniser le mapping avec le ComBus
void input_update(ComBus &bus);