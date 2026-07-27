# RAPPORT ÉTAPE 3 — CLASSIFICATION FICHIERS

## CATÉGORIE A — GÉNÉRIQUES (REMOTE-only)

### Fichiers analysés :

| Fichier | Canaux utilisés | Classification |
|---------|----------------|----------------|
| `light/dumper_truck_lights.h` | `LedCh` enum local (pas de ComBusID) | **A** |
| `motion/dumper_truck_motion.h` | Aucun (référence kGearShift_VolvoD16J) | **A** |
| `sound/dumper_truck_sound.h` | ⏳ À analyser | ? |
| `sound/dumper_truck_sound.cpp` | ⏳ À analyser | ? |

## CATÉGORIE B — SPÉCIFIQUES INSTANCE (LOCAL/SYSTEM)

### Fichiers analysés :

| Fichier | Canaux LOCAL/SYSTEM | Classification |
|---------|---------------------|----------------|
| `inputs_map/PS4_dualshock_map.cpp` | `DIRECT_DRIVE_BTN`, `SUBGEAR_SET_BTN`, `GEAR_UP_BTN`, `GEAR_DOWN_BTN`, `CRUISE_TOGGLE_BTN`, `CRUISE_UPDATE_BTN` (LOCAL) | **B** |
| `inputs_map/PS4_dualshock_map.h` | Dépend de .cpp | **B** |
| `combus/processors/input/proc_config.cpp` | `using namespace DumperTruck;` (CASSÉ) + canaux LOCAL | **B** |
| `combus/processors/sim/proc_config.cpp` | `using namespace DumperTruck;` (CASSÉ) + canaux LOCAL/SYSTEM | **B** |
| `combus/processors/input/*.h` | ⏳ À analyser | ? |
| `combus/processors/sim/*.h` | ⏳ À analyser | ? |

## RÉSUMÉ

### Catégorie A (OK) :
1. `light/dumper_truck_lights.h` ✅
2. `motion/dumper_truck_motion.h` ✅
3. `sound/dumper_truck_sound.h` — À vérifier
4. `sound/dumper_truck_sound.cpp` — À vérifier

### Catégorie B (à migrer) :
1. `inputs_map/PS4_dualshock_map.cpp` ⚠️
2. `inputs_map/PS4_dualshock_map.h` ⚠️
3. `combus/processors/input/proc_config.cpp` ⚠️
4. `combus/processors/sim/proc_config.cpp` ⚠️
5. `combus/processors/input/*.h` ⚠️
6. `combus/processors/sim/*.h` ⚠️