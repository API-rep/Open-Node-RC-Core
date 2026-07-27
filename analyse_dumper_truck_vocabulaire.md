# ANALYSE VOCABULAIRE REMOTE-ONLY - DUMPER_TRUCK

## 📋 **FICHIERS IDENTIFIÉS**

### **Structure `dumper_truck/`** :
```
core/config/machines/dumper_truck/
├── dumper_truck_config.h          ← Umbrella (inclut combus.h supprimé)
├── combus/
│   ├── processors/
│   │   ├── input/
│   │   │   ├── proc_config.cpp    ← Configuration processeurs input
│   │   │   ├── proc_config.h
│   │   │   ├── cruise_input_config.h
│   │   │   ├── direct_drive_config.h
│   │   │   ├── key_runlevel_config.h
│   │   │   └── subgear_config.h
│   │   └── sim/
│   │       ├── proc_config.cpp    ← Configuration processeurs sim
│   │       ├── proc_config.h
│   │       ├── dump_config.h
│   │       ├── gear_config.h
│   │       ├── steering_config.h
│   │       ├── throttle_config.h
│   │       └── traction_config.h
├── inputs_map/
│   ├── PS4_dualshock_map.cpp      ← Mapping PS4 → ComBus
│   ├── inputs_map.h
│   └── PS4_dualshock_map.h
├── light/
│   └── dumper_truck_lights.h      ← Configuration lumières
├── motion/
│   └── dumper_truck_motion.h      ← Configuration motion
└── sound/
    ├── dumper_truck_sound.cpp     ← Configuration son
    └── dumper_truck_sound.h
```

## 🔍 **ANALYSE USAGE COMBUS**

### **1. Fichiers génériques (REMOTE-only)** :
**Critère** : Utilisent seulement canaux REMOTE (génériques dumper_truck)

#### **Canaux REMOTE identifiés** :
**Analog** :
- `THROTTLE_BUS` (ou `THROTTLE_STICK`)
- `STEERING_BUS`
- `DUMP_BUS`
- `ESC_RPM_BUS`
- `ESC_SPEED_BUS`
- `GEAR`
- `DRIVE_STATE_BUS`

**Digital** :
- `HORN_BTN`
- `LIGHTS`
- `KEY_BTN`
- `BATTERY_LOW`
- `INDICATOR_LEFT`
- `INDICATOR_RIGHT`
- `HAZARDS`
- `HIGH_BEAM`
- `ROOF_LIGHT`
- `LOW_BEAM`
- `BRAKING`
- `KEY_ACTIVE`

### **2. Fichiers spécifiques (LOCAL/SYSTEM)** :
**Critère** : Utilisent canaux LOCAL/SYSTEM spécifiques Volvo A60H Bruder

#### **Canaux LOCAL identifiés** :
**Digital** :
- `GEAR_UP_BTN`
- `GEAR_DOWN_BTN`
- `CRUISE_TOGGLE_BTN`
- `CRUISE_UPDATE_BTN`
- `DIRECT_DRIVE_BTN`
- `SUBGEAR_SET_BTN`

## 🎯 **CLASSIFICATION FICHIERS**

### **Catégorie A** : Génériques dumper_truck (REMOTE-only)
1. `dumper_truck_config.h` → Umbrella (à corriger)
2. `inputs_map/PS4_dualshock_map.cpp` → Mapping générique
3. `inputs_map/PS4_dualshock_map.h` → Déclarations génériques
4. `light/dumper_truck_lights.h` → Lumières génériques
5. `motion/dumper_truck_motion.h` → Motion générique
6. `sound/dumper_truck_sound.cpp/.h` → Son générique

### **Catégorie B** : Spécifiques Volvo A60H Bruder
1. `combus/processors/input/*` → Contient canaux LOCAL (`GEAR_UP_BTN`, etc.)
2. `combus/processors/sim/*` → Contient canaux LOCAL/SYSTEM

### **Catégorie C** : Ambigu
1. `inputs_map/inputs_map.h` → Header générique (à analyser)

## 🛠️ **SOLUTION PROPOSÉE**

### **Étape 1** : Créer vocabulaire REMOTE-only
**Fichier** : `core/config/machines/dumper_truck/dumper_truck_remote_vocab.h`
**Contenu** :
```cpp
#pragma once

// Vocabulaire REMOTE-only pour dumper_truck (générique)
// Ne contient que les canaux partagés par TOUTES les instances dumper_truck

namespace DumperTruckRemote {
    // Canaux analog REMOTE
    enum class AnalogRemoteID : uint8_t {
        THROTTLE_BUS = 0,
        STEERING_BUS,
        DUMP_BUS,
        ESC_RPM_BUS,
        ESC_SPEED_BUS,
        GEAR,
        DRIVE_STATE_BUS,
        CH_COUNT
    };
    
    // Canaux digital REMOTE
    enum class DigitalRemoteID : uint8_t {
        HORN_BTN = 0,
        LIGHTS,
        KEY_BTN,
        BATTERY_LOW,
        INDICATOR_LEFT,
        INDICATOR_RIGHT,
        HAZARDS,
        HIGH_BEAM,
        ROOF_LIGHT,
        LOW_BEAM,
        BRAKING,
        KEY_ACTIVE,
        CH_COUNT
    };
}
```

### **Étape 2** : Corriger fichiers génériques
**Pattern** :
```diff
-#include <core/config/machines/dumper_truck/combus/combus.h>
-using namespace DumperTruck;
+// Pour fichiers génériques (REMOTE-only) :
+#include <core/config/machines/dumper_truck/dumper_truck_remote_vocab.h>
+using namespace DumperTruckRemote;
```

### **Étape 3** : Déplacer fichiers spécifiques
**Destination** : `src/machines/config/machines/volvo_A60H_bruder/`
**Fichiers** :
- `combus/processors/input/` → `volvo_A60H_bruder/combus/processors/input/`
- `combus/processors/sim/` → `volvo_A60H_bruder/combus/processors/sim/`

### **Étape 4** : Mettre à jour `dumper_truck_config.h`
```diff
// Ligne 27 :
-#include <core/config/machines/dumper_truck/combus/combus.h>
+// Inclut vocabulaire REMOTE-only OU dispatche vers machine spécifique
+#if MACHINE == VOLVO_A60_H_BRUDER
+  #include <machines/config/machines/volvo_A60H_bruder/combus/combus.h>
+#else
+  #include <core/config/machines/dumper_truck/dumper_truck_remote_vocab.h>
+#endif
```

## 📊 **IMPACT**

### **Avantages** :
- ✅ Séparation claire générique vs spécifique
- ✅ Vocabulaire REMOTE-only réutilisable
- ✅ Fichiers spécifiques déplacés au bon endroit
- ✅ Architecture scalable (nouvelles instances dumper_truck)

### **Inconvénients** :
- ❌ Migration fichiers nécessaires
- ❌ Mise à jour includes partout
- ❌ Risque compilation temporaire

## ✅ **VALIDATION**

### **Test 1** : Vocabulaire REMOTE-only
```cpp
#include <core/config/machines/dumper_truck/dumper_truck_remote_vocab.h>
DumperTruckRemote::AnalogRemoteID::THROTTLE_BUS  // Doit être valide
```

### **Test 2** : Fichiers génériques
```cpp
// inputs_map/PS4_dualshock_map.cpp :
#include <core/config/machines/dumper_truck/dumper_truck_remote_vocab.h>
// Doit compiler avec seulement canaux REMOTE
```

### **Test 3** : Fichiers spécifiques
```cpp
// volvo_A60H_bruder/combus/processors/input/proc_config.cpp :
#include <machines/config/machines/volvo_A60H_bruder/combus/combus.h>
// Doit avoir accès à tous les canaux (REMOTE + LOCAL + SYSTEM)
```

**État** : Analyse complète, solution architecturale proposée