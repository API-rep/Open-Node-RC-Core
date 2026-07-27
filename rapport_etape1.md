# RAPPORT ÉTAPE 1 — ÉTAT DES LIEUX

## 1a. VÉRIFICATION FRAGMENTS .INC

### core/config/machines/dumper_truck/combus/ (TYPE dumper_truck)
```
✅ combus_ids_remote_analog.inc   — EXISTE
✅ combus_ids_remote_digital.inc  — EXISTE
✅ combus_channels_remote_analog.inc — EXISTE
✅ combus_channels_remote_digital.inc — EXISTE
```

### src/machines/config/machines/volvo_A60H_bruder/combus/ (INSTANCE Volvo A60H)
```
✅ combus_ids_local_analog.inc    — EXISTE
✅ combus_ids_local_digital.inc   — EXISTE
✅ combus_ids_system_analog.inc  — EXISTE
✅ combus_ids_system_digital.inc — EXISTE
✅ combus_channels_local_analog.inc — EXISTE
✅ combus_channels_local_digital.inc — EXISTE
✅ combus_channels_system_analog.inc — EXISTE
✅ combus_channels_system_digital.inc — EXISTE
```

**RÉSULTAT** : Tous les 12 fragments .inc existent ✅

## 1b. VÉRIFICATION FICHIERS CLÉS

### Fichiers volvo_A60H_bruder/combus/
```
✅ combus_ids.h — EXISTE
✅ combus.h — EXISTE
✅ combus.cpp — EXISTE
```

### core/config/machines/combus_types.h
```
❌ FILE NOT FOUND — CE FICHIER N'EXISTE PAS
```
**PROBLÈME** : Ce dispatcher est manquant selon décision architecturale 7.

### include/struct/combus_struct.h
```
✅ EXISTE — Forward declarations ajoutées à l'étape précédente
```

### include/struct/machines_struct.h
```
✅ EXISTE — Forward declarations présentes
```

### core/system/vbat/vbat.cpp, vbat_alert.cpp
```
⏳ Non vérifié — à faire
```

### core/config/outputs/combus_uart.h
```
✅ EXISTE — Bug connu à vérifier
```

## 1c. GREP RÉSULTATS

### "using namespace DumperTruck"
```
src/core/config/machines/dumper_truck/combus/processors/input/proc_config.cpp: using namespace DumperTruck;
src/core/config/machines/dumper_truck/combus/processors/sim/proc_config.cpp: using namespace DumperTruck;
```
**RÉSULTAT** : 2 fichiers CASSÉS (ne compilent plus)

### "enum class AnalogComBusID" / "enum class DigitalComBusID"
```
# Définition complète (INSTANCE volvo_A60H_bruder) :
src/machines/config/machines/volvo_A60H_bruder/combus/combus_ids.h
  → enum class AnalogComBusID : uint8_t { ... }
  → enum class DigitalComBusID : uint8_t { ... }

# Forward declarations (devrait être centralisé dans combus_struct.h) :
include/struct/combus_proc_struct.h — DUPLIQUÉ
include/struct/machines_struct.h — DUPLIQUÉ
src/core/config/outputs/combus_uart.h — DUPLIQUÉ
src/core/system/combus/combus_access.h — DUPLIQUÉ
src/core/system/hw/dev/drv_dev.h — DUPLIQUÉ
src/core/system/hw/dev/sig_dev.h — DUPLIQUÉ
src/core/system/hw/dev/srv_dev.h — DUPLIQUÉ
src/core/system/hw/drv.h — DUPLIQUÉ
src/core/system/input/input_manager.cpp — DUPLIQUÉ
src/core/system/light/light_interpreter.h — DUPLIQUÉ
src/sound_module/system/sound/generators/effects_gen.h — DUPLIQUÉ
src/core/config/machines/dumper_truck/combus/processors/sim/proc_config.cpp — DUPLIQUÉ
```

### "combus_types.h" includes
```
# Aucun fichier ne référence combus_types.h — LE FICHIER N'EXISTE PAS
```

### "combus_ids.h" includes
```
include/struct/simulation_struct.h: #include <core/config/machines/combus_ids.h>
src/machines/config/machines/volvo_A60H_bruder/combus/combus.h: #include "combus_ids.h"
src/machines/config/machines/volvo_A60H_bruder/volvo_A60H_bruder.h: #include <core/config/machines/combus_ids.h>
src/sound_module/config/profiles/dumper_truck/dumper_truck_sound.cpp: #include <core/config/machines/combus_ids.h>
src/sound_module/system/sound/generators/effects_gen.cpp: #include <core/config/machines/combus_ids.h>
```
**PROBLÈME** : `#include <core/config/machines/combus_ids.h>` pointe vers un fichier supprimé (décision 8)

### "PS4_dualshock_map"
```
src/core/config/machines/dumper_truck/inputs_map/PS4_dualshock_map.cpp — EXISTE
src/core/config/machines/dumper_truck/inputs_map/PS4_dualshock_map.h — EXISTE
```

### "AnalogComBusRemoteID" / "DigitalComBusRemoteID"
```
src/core/config/machines/dumper_truck/combus/combus_ids_remote.h
  → enum class AnalogComBusRemoteID : uint8_t { ... }
  → enum class DigitalComBusRemoteID : uint8_t { ... }
```
**RÉSULTAT** : Correctement défini UNE SEULE FOIS ✅