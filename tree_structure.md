# Open Node RC Core — Arborescence du projet

## Structure principale

```
Open Node RC Core/
├── doc/                          # Documentation
├── include/                      # Headers partagés (const.h, defs/, struct/)
├── lib/                          # Bibliothèques externes
├── src/
│   ├── core/                     # 🔷 COEUR GÉNÉRIQUE (réutilisable across machines)
│   │   ├── config/               # Configuration du core
│   │   └── system/               # Implémentation du core
│   ├── machines/                 # 🟢 MACHINE-SPÉCIFIQUE
│   │   ├── config/              # Configuration machine
│   │   ├── init/                # Initialisation machine
│   │   └── system/              # Logique machine
│   ├── remotes/                  # 🟠 CONFIGURATION REMOTE
│   └── sound_module/             # 🔵 MODULE SON
├── test/                         # Tests unitaires
└── platformio.ini                 # Configuration build
```

---

## 🔷 CORE — src/core/

### Niveau 1 : src/core/config/
Configuration générique du core (chemins d'accès, presets).

```
src/core/config/
├── config.h                     # Point d'entrée config core
├── hw/                          # Configuration hardware générique
│   ├── board_clock.h            # Fréquences horloges
│   ├── motion_presets.h        # Presets mouvement
│   ├── servo_presets.h         # Presets servo
│   ├── shaker_presets.h        # Presets vibration
│   ├── simulation_presets.h    # Presets simulation
│   ├── esc/                    # Configuration ESC
│   ├── inputs/                 # Configuration entrées
│   ├── machines/               # Config machines (combus IDs)
│   ├── outputs/                # Configuration sorties
│   └── sound/                  # Configuration son
├── inputs/                      # Modules d'entrée génériques
│   ├── inputs.h                # Interface inputs
│   ├── nop.cpp / nop.h        # Input vide (aucune entrée)
│   └── PS4_dualshock.cpp/h     # Module PS4 Dualshock
├── machines/                    # Config machines partagées
│   ├── machine_type_combus_ids.h
│   ├── combus_ids_remote.h
│   └── dumper_truck/           # Machine example
│       ├── excavator/
│       └── loader/
├── outputs/                     # Configuration sorties
│   ├── outputs.h
│   ├── combus_espnow.h
│   └── combus_uart.h
├── sound/
│   └── sound_presets.h
└── vbat/                        # 🔋 Configuration batterie
    ├── config.h                 # Point d'entrée VBAT
    └── bat_lipo.h               # Profil LiPo
```

### Niveau 1 : src/core/system/
Implémentation générique du core (traitements, bus, debug).

```
src/core/system/
├── combus/                      # 🚌 ComBus (communication inter-nœuds)
│   ├── combus.h / combus.cpp    # Point d'entrée ComBus
│   ├── combus_manager.h/cpp     # Gestionnaire ComBus
│   ├── combus_access.h/cpp      # Accès bus
│   ├── combus_frame.h/cpp       # Cadres Trame
│   ├── combus_res.h             # Ressources partagées
│   ├── processors/              # Processeurs ComBus
│   │   ├── proc_chain.h/cpp     # Chaîne de processeurs
│   │   └── base/                # Processeurs de base
│   │       ├── input/           # Processeurs entrée
│   │       ├── math/           # Processeurs calcul
│   │       ├── modules/        # Processeurs modules
│   │       └── motion/         # Processeurs mouvement
│   ├── protocol/                # Protocole ComBus
│   │   ├── combus_protocol.h/cpp
│   │   ├── combus_rx.h/cpp     # Réception
│   │   └── combus_tx.h/cpp     # Transmission
│   └── README.md
│
├── debug/                       # 🖥️ Débogage / Dashboard
│   ├── dashboard/               # Vues dashboard
│   └── logging/                # Système de log
│
├── hw/                          # Hardware générique
│   ├── pin_reg.h / pin_reg.cpp  # Registres pins
│   ├── node_com.h               # Communication nœud
│   ├── drv.h                    # Drivers
│   ├── dev/                     # Périphériques
│   ├── sound/                   # Son hardware
│   ├── transport/               # Transport
│   └── sound/
│
├── input/                       # 📡 Gestionnaire d'entrée
│   ├── input_manager.h/cpp
│
├── light/                       # 💡 Contrôle éclairage
│   ├── light.h / light.cpp
│   ├── light_core.h/cpp
│   ├── light_interpreter.h/cpp
│   ├── light_state.h
│   ├── light_servo_beacon.h/cpp
│   └── neopixel/                # Support NeoPixel
│
├── output/                      # 📤 Gestionnaire sortie
│   ├── output_manager.h/cpp
│
├── sound/                       # 🔊 Configuration son
│   ├── sound_device.h
│   └── sound_device_cfg.h
│
└── vbat/                        # 🔋 Battery sensing
    ├── vbat.h / vbat.cpp       # Point d'entrée
    ├── vbat_sense.h/cpp        # Sensing ADC
    ├── vbat_alert.h/cpp        # Alertes batterie
    └── (dépend de src/core/config/vbat/)
```

---

## 🟢 MACHINES — src/machines/

### Niveau 1 : src/machines/

```
src/machines/
├── main.cpp                    # Point d'entrée machine
├── config/                     # 🔧 CONFIGURATION MACHINE
├── init/                       # 🚀 INITIALISATION
└── system/                     # ⚙️ LOGIQUE MACHINE
```

### Niveau 2 : src/machines/config/

```
src/machines/config/
├── config.h                     # Point d'entrée config machine
├── boards/                     # Configuration cartes
│   ├── boards.h                 # Sélection carte
│   ├── ESP32_8M_6S.h / ESP32_8M_6S.cpp
│   └── drivers/                 # Pilotes drivers
│       ├── drivers.h
│       ├── DRV8801.h
│       └── DRV8874.h
└── machines/                    # Machines disponibles
    ├── machines.h               # Sélection machine
    └── volvo_A60H_bruder/       # 🟨 VOLVO A60H (exemple)
        ├── volvo_A60H_bruder.h / volvo_A60H_bruder.cpp
        ├── combus/              # 🔌 ComBus machine
        │   ├── combus.h / combus.cpp
        │   ├── combus_ids.h
        │   ├── combus_channels_local_analog.inc
        │   ├── combus_channels_local_digital.inc
        │   ├── combus_channels_system_analog.inc
        │   ├── combus_channels_system_digital.inc
        │   ├── combus_ids_local_analog.inc
        │   ├── combus_ids_local_digital.inc
        │   ├── combus_ids_system_analog.inc
        │   ├── combus_ids_system_digital.inc
        │   └── processors/     # Processeurs ComBus machine
        │       ├── input/      # Input processing
        │       │   ├── proc_config.h/cpp
        │       │   ├── cruise_input_config.h
        │       │   ├── direct_drive_config.h
        │       │   ├── key_runlevel_config.h
        │       │   ├── subgear_config.h
        │       │   └── ...
        │       └── sim/       # Simulation processing
        │           ├── proc_config.h/cpp
        │           ├── dump_config.h
        │           ├── gear_config.h
        │           ├── steering_config.h
        │           ├── throttle_config.h
        │           ├── traction_config.h
        │           └── ...
        ├── inputs_map/         # 🕮️ MAP INPUTS
        │   ├── inputs_map.h
        │   ├── PS4_dualshock_map.h / PS4_dualshock_map.cpp
        │   └── ...
        └── mainboard/          # 🖥️ MAINBOARD
            ├── mainboard.h
            └── ESP32_8M_6S/   # Variante carte
                ├── envCfg.h / envCfg.cpp
                └── (autres variantes)
```

### Niveau 2 : src/machines/init/

```
src/machines/init/
├── init.h / init.cpp           # Point d'entrée initialisation
├── com/                        # Communication
│   ├── com_init.h / com_init.cpp
│   └── combus_uart_init.h / combus_uart_init.cpp
├── hw/                         # Hardware init
│   ├── hw_init.h / hw_init.cpp
│   ├── hw_init_com.h / hw_init_com.cpp
│   ├── hw_init_drv.h / hw_init_drv.cpp
│   ├── hw_init_sig.h / hw_init_sig.cpp
│   └── hw_init_srv.h / hw_init_srv.cpp
├── input/                      # Entrées init
│   └── input_init.h / input_init.cpp
├── output/                     # Sorties init
│   └── output_init.h / output_init.cpp
└── sys/                        # Système init
    ├── sys_init.h / sys_init.cpp
```

### Niveau 2 : src/machines/system/

```
src/machines/system/
├── drv_control.h / drv_control.cpp  # Contrôle drivers
├── sys_manager.h / sys_manager.cpp  # Gestionnaire système
├── utils.h                          # Utilitaires
├── debug/                          # 🖥️ Dashboard machine
│   ├── dashboard_drv.h / dashboard_drv.cpp
│   ├── dashboard_input.h / dashboard_input.cpp
│   ├── dashboard_machine.h / dashboard_machine.cpp
│   ├── dashboard_sig.h / dashboard_sig.cpp
│   ├── dashboard_simulation.h / dashboard_simulation.cpp
│   └── dashboard_vbat.h / dashboard_vbat.cpp
└── input/                          # Mise à jour entrées
    └── input_update.h / input_update.cpp
```

---

## Include / Headers partagés

```
include/
├── const.h                      # Constantes globales
├── README.md
├── defs/                        # Définitions (core_defs.h, etc.)
└── struct/                      # Structures de données partagées
```

---

## Résumé des couches

| Couche | Emplacement | Rôle |
|--------|-------------|------|
| **CONFIG** | `src/machines/config/` | Définit la machine (pins, drivers, inputs) |
| **INIT** | `src/machines/init/` | Initialise le hardware et ComBus |
| **SYSTEM** | `src/machines/system/` | Logique runtime machine |
| **CORE CONFIG** | `src/core/config/` | Présets et configurations génériques |
| **CORE SYSTEM** | `src/core/system/` | Implémentation générique (ComBus, vbat, light...) |
| **REMOTE** | `src/remotes/` | Configuration contrôleur distant |
| **SOUND** | `src/sound_module/` | Module son séparé |

---

## Chaîne d'include typique

```
main.cpp (machine)
  └─> machines/config/config.h
        ├─> machines/boards/boards.h
        │     └─> boards/ESP32_8M_6S.h
        │           └─> (pins, drivers)
        └─> machines/machines.h
              └─> machines/volvo_A60H_bruder/volvo_A60H_bruder.h
                    └─> machines/volvo_A60H_bruder/mainboard/ESP32_8M_6S/envCfg.h
                          └─> core/config/vbat/config.h
                                └─> core/config/vbat/bat_lipo.h
```

---

## Conventions de nommage

| Type | Extension | Rôle |
|------|-----------|------|
| Module principal | `.h` / `.cpp` | Interface et implémentation |
| Config | `envCfg.h` / `envCfg.cpp` | Configuration environnement |
| Init | `*_init.h` / `*_init.cpp` | Initialisation |
| Dashboard | `dashboard_*.h` / `dashboard_*.cpp` | Débogage |
| Map | `*_map.h` / `*_map.cpp` | Mappage entrée/sortie |
| Preset | `*_presets.h` | Préselection |
| Config | `*_config.h` / `*_config.cpp` | Configuration traitement |
| Processor | `proc_*.h` / `proc_*.cpp` | Processeur ComBus |
