# RAPPORT FINAL V2 — OPEN RC NODE COMBUS REWORK

## 1. CONFIRMATION SUPPRESSION combus_types.h

### RÉSULTAT : combus_types.h N'EXISTE PAS
**FICHIERS QUI LE RÉFÉRENCENT** : 16 fichiers CASSÉS

| Fichier | Type |
|---------|------|
| `include/struct/combus_struct.h` | Commentaire only |
| `include/struct/outputs_struct.h` | Commentaire only |
| `src/core/config/config.h` | `#include` |
| `src/core/system/light/light_interpreter.cpp` | `#include` |
| `src/machines/config/machines/volvo_A60H_bruder/mainboard/ESP32_8M_6S/envCfg.cpp` | Commentaire only |
| `src/machines/config/machines/volvo_A60H_bruder/mainboard/ESP32_8M_6S/envCfg.h` | `#include` |
| `src/machines/init/hw/hw_init.cpp` | `#include` |
| `src/machines/init/hw/hw_init.h` | `#include` |
| `src/machines/init/input/input_init.h` | `#include` |
| `src/machines/system/debug/dashboard_input.cpp` | `#include` |
| `src/machines/system/drv_control.h` | `#include` |
| `src/sound_module/config/config.h` | `#include` |
| `src/sound_module/config/profiles/dumper_truck/dumper_truck.cpp` | `#include` |
| `src/sound_module/init/hw_init_dcdev.cpp` | `#include` |
| `src/sound_module/system/combus_sound_interpreter.cpp` | `#include` |
| `src/sound_module/system/debug/dashboard_sound.cpp` | `#include` |
| `src/sound_module/system/sound/sound_audio_init.cpp` | `#include` |

### CONFIRMATION : Ancien chemin combus/combus_ids.h N'EXISTE PAS
**CHEMIN VÉRIFIÉ** : `core/config/machines/dumper_truck/combus/combus_ids.h`
**RÉSULTAT** : FILE NOT FOUND

---

## 2. CONTENU combus_ids_remote.h

### ANALYSE : fichier n'existe pas encore
**CHEMIN ATTENDU** : `core/config/machines/dumper_truck/combus_ids_remote.h`
**RÉSULTAT** : FILE NOT FOUND

### CONTENU FRAGMENTS .INC

#### combus_ids_remote_analog.inc :
```
STEERING_BUS = 0,
ESC_RPM_BUS,
DUMP_BUS,
ESC_SPEED_BUS,
GEAR,
DRIVE_STATE_BUS,
```

**DIVERGENCE** : Canal `THROTTLE_BUS` manquant dans le fragment (attendu selon l'analyse précédente)

#### combus_ids_remote_digital.inc :
```
HORN_BTN = 0,
LIGHTS,
KEY_BTN,
BATTERY_LOW,
CORE_END,

INDICATOR_LEFT = CORE_END,
INDICATOR_RIGHT,
HAZARDS,
HIGH_BEAM,
ROOF_LIGHT,
LOW_BEAM,
LIGHT_END,

BRAKING = LIGHT_END,
KEY_ACTIVE,
MOTION_END,
```

---

## 3. CLASSIFICATION FICHIERS CASSÉS

### CATÉGORIE A — Core générique (REMOTE-only)
**Action** : Corriger vers `combus_ids_remote.h`

| Fichier | Action |
|---------|--------|
| `src/core/system/light/light_interpreter.cpp` | Corriger #include |

### CATÉGORIE B — Instance spécifique (LOCAL/SYSTEM + runtime)
**Action** : Migrer vers `volvo_A60H_bruder/`

| Fichier | Destination |
|---------|-------------|
| `src/machines/config/machines/volvo_A60H_bruder/mainboard/ESP32_8M_6S/envCfg.h` | volvo_A60H_bruder/mainboard/ESP32_8M_6S/ |
| `src/machines/init/hw/hw_init.cpp` | volvo_A60H_bruder/init/hw/ |
| `src/machines/init/hw/hw_init.h` | volvo_A60H_bruder/init/hw/ |
| `src/machines/init/input/input_init.h` | volvo_A60H_bruder/init/input/ |
| `src/machines/system/debug/dashboard_input.cpp` | volvo_A60H_bruder/system/debug/ |
| `src/machines/system/drv_control.h` | volvo_A60H_bruder/system/ |
| `src/sound_module/config/profiles/dumper_truck/dumper_truck.cpp` | sound_module/config/profiles/dumper_truck/ |
| `src/sound_module/init/hw_init_dcdev.cpp` | sound_module/init/ |
| `src/sound_module/system/combus_sound_interpreter.cpp` | sound_module/system/ |
| `src/sound_module/system/debug/dashboard_sound.cpp` | sound_module/system/debug/ |
| `src/sound_module/system/sound/sound_audio_init.cpp` | sound_module/system/sound/ |

### NON CLASSIFIABLE (config.h umbrella)
| Fichier | Action |
|---------|--------|
| `src/core/config/config.h` | Corriger ou laisser (umbrella) |
| `src/sound_module/config/config.h` | Corriger ou laisser (umbrella) |

---

## 4. NOUVELLE ARCHITECTURE

### NOUVELLE POSITION combus_ids_remote.h :
```
core/config/machines/dumper_truck/combus_ids_remote.h  (directement sous dumper_truck/)
```

### STRUCTURE ATTENDUE :
```cpp
#pragma once
#include <cstdint>

enum class AnalogComBusRemoteID : uint8_t {
    #include "combus_ids_remote_analog.inc"
    CH_COUNT
};

enum class DigitalComBusRemoteID : uint8_t {
    #include "combus_ids_remote_digital.inc"
    CH_COUNT
};
```

---

## 5. ACTIONS REQUISES

### IMMÉDIAT :
1. Créer `combus_ids_remote.h` à la nouvelle position
2. Corriger les fichiers catégorie A
3. Signaler divergence THROTTLE_BUS dans le fragment

### EN ATTENTE DE VALIDATION :
1. Migration fichiers catégorie B vers volvo_A60H_bruder
2. Corrections config.h umbrella

---

## 6. IS_MACHINE / IS_REMOTE

**NON ANALYSÉ** — À faire dans une prochaine passe

---

## 7. COMPILATION

**NON VÉRIFIÉ** — pio non testé