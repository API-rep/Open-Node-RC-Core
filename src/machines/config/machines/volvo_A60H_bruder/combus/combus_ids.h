/*!****************************************************************************
 * @file  combus_ids.h
 * @brief ComBus channel ID enumerations — Volvo A60H Bruder (type: dumper_truck)
 *
 * Assemble, dans cet ordre fixe :
 *   REMOTE  <- core/config/machines/dumper_truck/combus/ (définition de TYPE,
 *              commune à toute machine dumper_truck, zéro notion hardware)
 *   LOCAL   <- ce dossier (spécifique à l'instance Volvo A60H Bruder)
 *   SYSTEM  <- ce dossier (umbrella modules d'exécution, à peupler plus tard)
 *
 * Fichier zéro-dépendance par construction : seul <cstdint> est inclus en
 * dehors des fragments .inc, et les fragments eux-mêmes ne contiennent que
 * des tokens bruts (pas de #include). Cette propriété doit être préservée —
 * ne jamais ajouter de #include dans un fragment .inc.
 *
 * NE PAS réordonner les #include ci-dessous : l'ordre détermine la valeur
 * numérique de WIRE_END / MACHINE_END, utilisées par le protocole de
 * transport (n_analog / n_digital côté frame TX/RX).
 *******************************************************************************///
#pragma once

#include <cstdint>

enum class AnalogComBusID : uint8_t {
  #include <core/config/machines/dumper_truck/combus/combus_ids_remote_analog.inc>
  WIRE_END,
  #include "combus_ids_local_analog.inc"
  #include "combus_ids_system_analog.inc"
  CH_COUNT
};

enum class DigitalComBusID : uint8_t {
  #include <core/config/machines/dumper_truck/combus/combus_ids_remote_digital.inc>
  WIRE_END,
  #include "combus_ids_local_digital.inc"
  MACHINE_END,
  #include "combus_ids_system_digital.inc"
  CH_COUNT
};

// EOF combus_ids.h
