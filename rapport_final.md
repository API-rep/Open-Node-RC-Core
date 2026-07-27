# RAPPORT FINAL — OPEN RC NODE COMBUS REWORK

## 1. RÉSULTAT ÉTAPE 1 — ÉTAT DES LIEUX

### 1a. VÉRIFICATION FRAGMENTS .INC

| Chemin | Fichier | Statut |
|--------|---------|--------|
| `core/config/machines/dumper_truck/combus/` | `combus_ids_remote_analog.inc` | ✅ EXISTE |
| | `combus_ids_remote_digital.inc` | ✅ EXISTE |
| | `combus_channels_remote_analog.inc` | ✅ EXISTE |
| | `combus_channels_remote_digital.inc` | ✅ EXISTE |
| `src/machines/config/machines/volvo_A60H_bruder/combus/` | `combus_ids_local_analog.inc` | ✅ EXISTE |
| | `combus_ids_local_digital.inc` | ✅ EXISTE |
| | `combus_ids_system_analog.inc` | ✅ EXISTE |
| | `combus_ids_system_digital.inc` | ✅ EXISTE |
| | `combus_channels_local_analog.inc` | ✅ EXISTE |
| | `combus_channels_local_digital.inc` | ✅ EXISTE |
| | `combus_channels_system_analog.inc` | ✅ EXISTE |
| | `combus_channels_system_digital.inc` | ✅ EXISTE |

**RÉSULTAT** : Tous les 12 fragments .inc existent ✅

### 1b. VÉRIFICATION FICHIERS CLÉS

| Fichier | Statut |
|---------|--------|
| `volvo_A60H_bruder/combus/combus_ids.h` | ✅ EXISTE |
| `volvo_A60H_bruder/combus/combus.h` | ✅ EXISTE |
| `volvo_A60H_bruder/combus/combus.cpp` | ✅ EXISTE |
| `core/config/machines/combus_types.h` | ❌ **FILE NOT FOUND** |
| `include/struct/combus_struct.h` | ✅ EXISTE (forward declarations ajoutées) |
| `include/struct/machines_struct.h` | ✅ EXISTE |
| `core/config/outputs/combus_uart.h` | ✅ EXISTE |
| `dumper_truck/combus/combus_ids_remote.h` | ✅ EXISTE |

### 1c. GREP RÉSULTATS

#### "using namespace DumperTruck" — 2 fichiers CASSÉS :
```
src/core/config/machines/dumper_truck/combus/processors/input/proc_config.cpp
src/core/config/machines/dumper_truck/combus/processors/sim/proc_config.cpp
```

#### "enum class AnalogComBusID / DigitalComBusID" :
- **Définition complète** : `volvo_A60H_bruder/combus/combus_ids.h` ✅
- **Forward declarations DUPLIQUÉES** (12 fichiers) :
  - `include/struct/combus_proc_struct.h`
  - `include/struct/machines_struct.h`
  - `src/core/config/outputs/combus_uart.h`
  - `src/core/system/combus/combus_access.h`
  - `src/core/system/hw/dev/drv_dev.h`
  - `src/core/system/hw/dev/sig_dev.h`
  - `src/core/system/hw/dev/srv_dev.h`
  - `src/core/system/hw/drv.h`
  - `src/core/system/input/input_manager.cpp`
  - `src/core/system/light/light_interpreter.h`
  - `src/sound_module/system/sound/generators/effects_gen.h`
  - `src/core/config/machines/dumper_truck/combus/processors/sim/proc_config.cpp`

#### "combus_types.h" :
- **FILE NOT FOUND** — Ce dispatcher n'existe pas

#### "#include <core/config/machines/combus_ids.h>" :
```
include/struct/simulation_struct.h
src/machines/config/machines/volvo_A60H_bruder/combus/combus.h
src/machines/config/machines/volvo_A60H_bruder/volvo_A60H_bruder.h
src/sound_module/config/profiles/dumper_truck/dumper_truck_sound.cpp
src/sound_module/system/sound/generators/effects_gen.cpp
```
**PROBLÈME** : `#include <core/config/machines/combus_ids.h>` pointe vers un fichier supprimé (décision 8)

#### "AnalogComBusRemoteID / DigitalComBusRemoteID" :
- **Définition unique** : `dumper_truck/combus/combus_ids_remote.h` ✅

---

## 2. ÉTAPE 2 — combus_uart.h

**FICHIER** : `src/core/config/outputs/combus_uart.h`

**RÉSULTAT** : ✅ **DÉJÀ CORRECT**
- Lignes 55-56 : Utilise `AnalogComBusID::WIRE_END` et `DigitalComBusID::WIRE_END`
- Pas de bug `CH_COUNT` détecté
- Forward declarations DUPLIQUÉES (lignes 27-28) — à corriger en étape 6

---

## 3. ÉTAPE 3 — CLASSIFICATION CATÉGORIE A / B

### CATÉGORIE A — GÉNÉRIQUES (REMOTE-only)

| Fichier | Canaux utilisés | Classification |
|---------|----------------|---------------|
| `light/dumper_truck_lights.h` | `LedCh` enum local | **A** |
| `motion/dumper_truck_motion.h` | Aucun ComBusID | **A** |
| `sound/dumper_truck_sound.h` | Aucun ComBusID | **A** |

### CATÉGORIE B — SPÉCIFIQUES INSTANCE (LOCAL/SYSTEM)

| Fichier | Canaux LOCAL/SYSTEM | Destination |
|---------|---------------------|-------------|
| `inputs_map/PS4_dualshock_map.cpp` | `DIRECT_DRIVE_BTN`, `SUBGEAR_SET_BTN`, `GEAR_UP_BTN`, `GEAR_DOWN_BTN`, `CRUISE_TOGGLE_BTN`, `CRUISE_UPDATE_BTN` | `volvo_A60H_bruder/inputs_map/` |
| `inputs_map/PS4_dualshock_map.h` | Dépend du .cpp | `volvo_A60H_bruder/inputs_map/` |
| `combus/processors/input/proc_config.cpp` | `using namespace DumperTruck;` (CASSÉ) + canaux LOCAL | `volvo_A60H_bruder/combus/processors/input/` |
| `combus/processors/input/proc_config.h` | InputCh enum | `volvo_A60H_bruder/combus/processors/input/` |
| `combus/processors/input/subgear_config.h` | À vérifier | `volvo_A60H_bruder/combus/processors/input/` |
| `combus/processors/input/direct_drive_config.h` | À vérifier | `volvo_A60H_bruder/combus/processors/input/` |
| `combus/processors/input/key_runlevel_config.h` | À vérifier | `volvo_A60H_bruder/combus/processors/input/` |
| `combus/processors/input/cruise_input_config.h` | À vérifier | `volvo_A60H_bruder/combus/processors/input/` |
| `combus/processors/sim/proc_config.cpp` | `using namespace DumperTruck;` (CASSÉ) + canaux LOCAL/SYSTEM | `volvo_A60H_bruder/combus/processors/sim/` |
| `combus/processors/sim/proc_config.h` | SimCh enum | `volvo_A60H_bruder/combus/processors/sim/` |
| `combus/processors/sim/dump_config.h` | À vérifier | `volvo_A60H_bruder/combus/processors/sim/` |
| `combus/processors/sim/gear_config.h` | À vérifier | `volvo_A60H_bruder/combus/processors/sim/` |
| `combus/processors/sim/steering_config.h` | À vérifier | `volvo_A60H_bruder/combus/processors/sim/` |
| `combus/processors/sim/throttle_config.h` | À vérifier | `volvo_A60H_bruder/combus/processors/sim/` |
| `combus/processors/sim/traction_config.h` | À vérifier | `volvo_A60H_bruder/combus/processors/sim/` |

---

## 4. ÉTAPE 4 — DIFF FICHIERS CATÉGORIE A

### AUCUNE CORRECTION NÉCESSAIRE
Les fichiers catégorie A n'utilisent pas de canaux `AnalogComBusID`/`DigitalComBusID` — aucun changement requis.

---

## 5. ÉTAPE 5 — LISTE CATÉGORIE B

### FICHIERS À DÉPLACER (destination proposée) :

```
SOURCE (core/config/machines/dumper_truck/) → DESTINATION (volvo_A60H_bruder/)
```

| Source | Destination | Canaux LOCAL/SYSTEM |
|--------|-------------|---------------------|
| `inputs_map/PS4_dualshock_map.cpp` | `inputs_map/PS4_dualshock_map.cpp` | DIRECT_DRIVE_BTN, SUBGEAR_SET_BTN, GEAR_UP_BTN, GEAR_DOWN_BTN, CRUISE_TOGGLE_BTN, CRUISE_UPDATE_BTN |
| `inputs_map/PS4_dualshock_map.h` | `inputs_map/PS4_dualshock_map.h` | (dépend du .cpp) |
| `combus/processors/input/proc_config.cpp` | `combus/processors/input/proc_config.cpp` | using namespace DumperTruck; (CASSÉ) |
| `combus/processors/input/proc_config.h` | `combus/processors/input/proc_config.h` | InputCh enum |
| `combus/processors/input/*.h` | `combus/processors/input/` | À vérifier |
| `combus/processors/sim/proc_config.cpp` | `combus/processors/sim/proc_config.cpp` | using namespace DumperTruck; (CASSÉ) |
| `combus/processors/sim/proc_config.h` | `combus/processors/sim/proc_config.h` | SimCh enum |
| `combus/processors/sim/*.h` | `combus/processors/sim/` | À vérifier |

**AUCUN DÉPLACEMENT EFFECTUÉ** — En attente de validation.

### PS4_dualshock_map — MÉCANISME input_map_provider :
- Le mécanisme `input_map_provider` n'existe pas encore
- Création requise selon instructions ETAPE 5

---

## 6. ÉTAPE 6 — FORWARD DECLARATIONS À CENTRALISER

### FICHIERS AVEC FORWARD DECLARATIONS DUPLIQUÉES :

| Fichier | Action |
|---------|--------|
| `include/struct/combus_proc_struct.h` | Remplacer par `#include <struct/combus_struct.h>` |
| `include/struct/machines_struct.h` | Remplacer par `#include <struct/combus_struct.h>` |
| `src/core/config/outputs/combus_uart.h` | Remplacer par `#include <struct/combus_struct.h>` |
| `src/core/system/combus/combus_access.h` | Remplacer par `#include <struct/combus_struct.h>` |
| `src/core/system/hw/dev/drv_dev.h` | Remplacer par `#include <struct/combus_struct.h>` |
| `src/core/system/hw/dev/sig_dev.h` | Remplacer par `#include <struct/combus_struct.h>` |
| `src/core/system/hw/dev/srv_dev.h` | Remplacer par `#include <struct/combus_struct.h>` |
| `src/core/system/hw/drv.h` | Remplacer par `#include <struct/combus_struct.h>` |
| `src/core/system/input/input_manager.cpp` | Remplacer par `#include <struct/combus_struct.h>` |
| `src/core/system/light/light_interpreter.h` | Remplacer par `#include <struct/combus_struct.h>` |
| `src/sound_module/system/sound/generators/effects_gen.h` | Remplacer par `#include <struct/combus_struct.h>` |
| `src/core/config/machines/dumper_truck/combus/processors/sim/proc_config.cpp` | Remplacer par `#include <struct/combus_struct.h>` |

---

## 7. ÉTAPE 7 — VÉRIFICATION COMPILATION

### PLATFORMIO DISPONIBILITÉ :
```
⏳ Non testé — pio non vérifié dans cette session
```

### FICHIERS MODIFIÉS DANS CETTE SESSION :
- `include/struct/combus_struct.h` — forward declarations ajoutées
- `src/core/config/machines/dumper_truck/combus/combus_ids_remote.h` — créé
- `src/core/config/machines/dumper_truck/dumper_truck_config.h` — corrigé

**NON VÉRIFIÉ PAR COMPILATION RÉELLE**

---

## RÉSUMÉ DES ACTIONS REQUISES

### IMMÉDIAT :
1. Créer `core/config/machines/combus_types.h` (dispatcher manquant)
2. Corriger `#include <core/config/machines/combus_ids.h>` → `#include <core/config/machines/dumper_truck/combus/combus_ids_remote.h>` dans les fichiers qui référencent l'ancien chemin
3. Centraliser 12 forward declarations dupliquées
4. Créer mécanisme `input_map_provider` pour PS4_dualshock_map
5. Préparer migration fichiers catégorie B vers volvo_A60H_bruder

### EN ATTENTE DE VALIDATION :
1. Déplacement fichiers B (proc_config.cpp/h, *_config.h)
2. Déplacement PS4_dualshock_map.cpp/h vers volvo_A60H_bruder/inputs_map/

### NON RÉSOLU :
1. `combus_types.h` manquant — à créer selon décision architecturale 7