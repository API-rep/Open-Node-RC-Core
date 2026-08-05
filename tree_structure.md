# Open Node RC Core — Arborescence du projet

> **État au commit `1e7612d`** (branche `combus-frame-handshake`).
> Reflète la régression `CombusLayout` et le split handshake RX/TX.

## Structure principale

```
Open Node RC Core/
├── doc/                          # Documentation
├── include/                      # Headers partagés (const.h, defs/, struct/)
├── lib/                          # Bibliothèques externes
├── scripts/                      # 🆕 Scripts de build (PlatformIO extra_scripts)
├── src/
│   ├── core/                     # 🔷 COEUR GÉNÉRIQUE (réutilisable across machines)
│   │   ├── config/               # Configuration du core
│   │   └── system/               # Implémentation du core
│   ├── machines/                 # 🟢 MACHINE-SPÉCIFIQUE (instance)
│   │   ├── config/               # Configuration machine
│   │   ├── init/                 # Initialisation machine
│   │   └── system/               # Logique machine
│   ├── remotes/                  # 🟠 CONFIGURATION REMOTE
│   └── sound_module/             # 🔵 MODULE SON (fork rc_engine_sound — sous-projet PIO)
├── test/                         # Tests unitaires
├── platformio.ini                # Configuration build
└── project_version.h             # 🆕 Version projet (racine, peer de platformio.ini)
```

---

## 🔷 CORE — `src/core/`

### Niveau 1 : `src/core/config/`

Configuration générique du core (chemins d'accès, presets, dispatchers).

```
src/core/config/
├── config.h                     # Point d'entrée config core
├── hw/                          # Configuration hardware générique
│   ├── board_clock.h            # Fréquences horloges
│   ├── motion_presets.h         # Presets mouvement
│   ├── servo_presets.h          # Presets servo
│   ├── shaker_presets.h         # Presets vibration
│   ├── simulation_presets.h     # Presets simulation
│   ├── esc/                     # Configuration ESC
│   ├── inputs/                  # Configuration entrées
│   ├── machines/                # 🆕 Config machines par TYPE (combus, motion, light, sound)
│   ├── outputs/                 # Configuration sorties
│   └── sound/                   # Configuration son
├── inputs/                      # Modules d'entrée génériques
│   ├── inputs.h                 # Interface inputs (dispatcher INPUT_*)
│   ├── nop.cpp / nop.h          # Input vide (aucune entrée)
│   └── PS4_dualshock.cpp/h      # Module PS4 Dualshock
├── machines/                    # 🆕 Config machines par TYPE (dumper_truck, excavator, loader)
│   ├── machine_type.h           # 🆕 Top-level dispatcher MACHINE_TYPE_* → <type>_config.h
│   ├── dumper_truck/            # TYPE: dumper truck / articulated hauler
│   │   ├── dumper_truck_config.h    # Sub-umbrella (combus + motion + light + sound)
│   │   ├── combus/                  # 🆕 ComBus REMOTE runtime + vocab
│   │   │   ├── combus_remote.h          # TYPE-level runtime umbrella
│   │   │   ├── combus_remote.cpp        # TYPE-level runtime definitions
│   │   │   ├── combus_ids_remote.h      # REMOTE vocab (Analog/Digital RemoteID)
│   │   │   ├── combus_ids_remote_analog.inc
│   │   │   ├── combus_ids_remote_digital.inc
│   │   │   ├── combus_remote_analog.inc
│   │   │   └── combus_remote_digital.inc
│   │   ├── light/                  # Light profile (dumper_truck_lights.h)
│   │   ├── motion/                 # Motion preset (dumper_truck_motion.h)
│   │   └── sound/                  # Sound profile (dumper_truck_sound.{h,cpp})
│   ├── excavator/                # TYPE: excavator (squelette — winter 2026)
│   │   ├── excavator_config.h
│   │   ├── combus/                # (vide — à compléter)
│   │   ├── inputs_map/            # (vide — à compléter)
│   │   ├── light/                # (vide — à compléter)
│   │   ├── motion/               # (vide — à compléter)
│   │   └── sound/                # Sound profile (excavator_sound.{h,cpp})
│   └── loader/                   # TYPE: wheel loader (squelette — winter 2026)
│       ├── loader_config.h
│       ├── combus/               # (vide — à compléter)
│       ├── inputs_map/           # (vide — à compléter)
│       ├── light/               # (vide — à compléter)
│       ├── motion/              # (vide — à compléter)
│       └── sound/               # Sound profile (loader_sound.{h,cpp})
├── outputs/                     # Configuration sorties
│   ├── outputs.h
│   ├── combus_espnow.h
│   └── combus_uart.h
├── sound/
│   └── sound_presets.h
└── vbat/                        # 🔋 Configuration batterie
    ├── config.h                 # Point d'entrée VBAT (dispatcher VBAT_*)
    └── bat_lipo.h               # Profil LiPo
```

> **Notes de migration** (commits `13b4c5f` + `1e7612d`) :
> - `src/core/config/machines/combus_ids_remote.h` (dispatcher racine) **supprimé**.
> - `src/core/config/machines/machine_type_combus_ids.h` (doublon) **supprimé**.
> - `src/core/config/machines/dumper_truck/combus/combus_channels_remote_*.inc` **renommés** en `combus_remote_*.inc` (préfixe correct).
> - `src/core/config/machines/dumper_truck/combus_ids_remote.h` (chemin fantôme sans `/combus/`) **supprimé**.
> - `src/core/config/machines/machine_type.h` **créé** — dispatcher top-level `MACHINE_TYPE_*` → `<type>_config.h`.
> - `src/core/config/machines/<type>/combus/combus_remote.{h,cpp}` **créés** — runtime umbrella TYPE (mirror de `combus_ids_remote.h` côté runtime).

### Niveau 1 : `src/core/system/`

Implémentation générique du core (traitements, bus, debug).

```
src/core/system/
├── combus/                      # 🚌 ComBus (communication inter-nœuds)
│   ├── combus.h / combus.cpp    # Point d'entrée ComBus
│   ├── combus_manager.h/cpp     # Gestionnaire ComBus
│   ├── combus_access.h/cpp      # Accès bus
│   ├── combus_res.h             # Ressources partagées
│   ├── frame/                   # 🆕 Encodage/décodage trame + handshake
│   │   ├── combus_frame.h/cpp           # Codec trame (encode/decode + CRC)
│   │   ├── combus_frame_defs.h          # Constantes wire (SOF, header len, etc.)
│   │   ├── combus_handshake.h           # 🆕 Umbrella handshake (constants + boot helper + md5 hex)
│   │   ├── combus_handshake.cpp
│   │   ├── combus_handshake_rx.h/cpp    # 🆕 RX handshake (decode + CRC + compare-and-log)
│   │   └── combus_handshake_tx.h/cpp    # 🆕 TX handshake (frame build + manual sendOnce)
│   ├── processors/              # Processeurs ComBus (CbChain)
│   │   ├── proc_chain.h/cpp     # Chaîne de processeurs
│   │   ├── COMBUS_PROCESSORS_ROADMAP.md # 🆕 Roadmap (déplacé depuis dumper_truck/combus/)
│   │   ├── base/                # Processeurs de base
│   │   │   ├── cb_bypass.cpp/h
│   │   │   ├── cb_io.cpp/h
│   │   │   └── cb_runlevel.cpp/h
│   │   ├── input/               # Processeurs entrée
│   │   │   └── cb_btn.cpp/h
│   │   ├── math/                # Processeurs calcul
│   │   │   ├── cb_abs.cpp/h
│   │   │   ├── cb_center.cpp/h
│   │   │   └── cb_scale.cpp/h
│   │   ├── modules/             # Processeurs modules
│   │   │   ├── README.md
│   │   │   └── gear/            # Module gear (FSM)
│   │   │       ├── cb_gear.cpp/h
│   │   │       └── gear_fsm.cpp/h
│   │   └── motion/              # Processeurs mouvement
│   │       ├── cb_brake.cpp/h
│   │       ├── cb_cruise.cpp/h
│   │       ├── cb_dir.cpp/h
│   │       └── cb_ramp.cpp/h
│   └── protocol/                # Protocole ComBus (RX/TX)
│       ├── combus_protocol.h/cpp
│       ├── combus_rx.h/cpp      # Réception (transport-agnostic)
│       └── combus_tx.h/cpp      # 🆕 Transmission (transport-agnostic)
│
├── debug/                       # 🖥️ Débogage / Dashboard
│   ├── dashboard/               # Vues dashboard
│   └── logging/                 # Système de log
│
├── hw/                          # Hardware générique
│   ├── pin_reg.h / pin_reg.cpp  # Registres pins
│   ├── node_com.h               # Communication nœud
│   ├── drv.h                    # Drivers
│   ├── dev/                     # Périphériques (esc, drv, sig, srv, shaker, switch)
│   ├── sound/                   # Son hardware (sound_hal_dac)
│   └── transport/               # Transport (uart_com)
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
    ├── vbat.h / vbat.cpp        # Point d'entrée
    ├── vbat_sense.h/cpp         # Sensing ADC
    ├── vbat_alert.h/cpp         # Alertes batterie
    └── (dépend de src/core/config/vbat/)
```

> **Notes de migration** :
> - `src/core/system/combus/frame/` **créé** — extraction du codec trame + handshake depuis l'ancien chemin plat.
> - `combus_handshake.{h,cpp}` **splitté** en `combus_handshake.{h,cpp}` (umbrella) + `combus_handshake_rx.{h,cpp}` + `combus_handshake_tx.{h,cpp}`.
> - `combus_tx.h/cpp` **créé** (mirror du split RX/TX déjà en place pour le transport).
> - `COMBUS_PROCESSORS_ROADMAP.md` **déplacé** depuis `dumper_truck/combus/` vers `processors/` (sa place canonique).

---

## 🟢 MACHINES — `src/machines/`

### Niveau 1 : `src/machines/`

```
src/machines/
├── main.cpp                     # Point d'entrée machine
├── config/                      # 🔧 CONFIGURATION MACHINE
├── init/                        # 🚀 INITIALISATION
└── system/                      # ⚙️ LOGIQUE MACHINE
```

### Niveau 2 : `src/machines/config/`

```
src/machines/config/
├── config.h                     # Point d'entrée config machine
├── boards/                      # Configuration cartes
│   ├── boards.h                 # Sélection carte (dispatcher BOARD_*)
│   ├── ESP32_8M_6S.h / ESP32_8M_6S.cpp
│   └── drivers/                 # Pilotes drivers
│       ├── drivers.h            # Sélection driver (dispatcher BOARD_DC_DRIVER_*)
│       ├── DRV8801.h
│       └── DRV8874.h
└── machines/                    # Machines disponibles (instances)
    ├── machines.h               # Dispatcher MACHINE_* → <instance>/<instance>.h
    └── volvo_A60H_bruder/       # 🟨 VOLVO A60H (exemple)
        ├── volvo_A60H_bruder.h / volvo_A60H_bruder.cpp
        ├── combus/              # 🔌 ComBus machine (instance)
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
        │   └── processors/      # Processeurs ComBus machine
        │       ├── input/       # Input processing
        │       │   ├── proc_config.h/cpp
        │       │   ├── cruise_input_config.h
        │       │   ├── direct_drive_config.h
        │       │   ├── key_runlevel_config.h
        │       │   ├── subgear_config.h
        │       │   └── ...
        │       └── sim/         # Simulation processing
        │           ├── proc_config.h/cpp
        │           ├── dump_config.h
        │           ├── gear_config.h
        │           ├── steering_config.h
        │           ├── throttle_config.h
        │           ├── traction_config.h
        │           └── ...
        ├── inputs_map/          # 🕮️ MAP INPUTS
        │   ├── inputs_map.h
        │   ├── PS4_dualshock_map.h / PS4_dualshock_map.cpp
        │   └── ...
        └── mainboard/           # 🖥️ MAINBOARD
            ├── mainboard.h
            └── ESP32_8M_6S/      # Variante carte
                ├── envCfg.h / envCfg.cpp
                └── (autres variantes)
```

### Niveau 2 : `src/machines/init/`

```
src/machines/init/
├── init.h / init.cpp            # Point d'entrée initialisation
├── com/                         # Communication
│   ├── com_init.h / com_init.cpp
│   └── combus_uart_init.h / combus_uart_init.cpp
├── hw/                          # Hardware init
│   ├── hw_init.h / hw_init.cpp
│   ├── hw_init_com.h / hw_init_com.cpp
│   ├── hw_init_drv.h / hw_init_drv.cpp
│   ├── hw_init_sig.h / hw_init_sig.cpp
│   └── hw_init_srv.h / hw_init_srv.cpp
├── input/                       # Entrées init
│   └── input_init.h / input_init.cpp
├── output/                      # Sorties init
│   └── output_init.h / output_init.cpp
└── sys/                         # Système init
    ├── sys_init.h / sys_init.cpp
```

### Niveau 2 : `src/machines/system/`

```
src/machines/system/
├── drv_control.h / drv_control.cpp  # Contrôle drivers
├── sys_manager.h / sys_manager.cpp  # Gestionnaire système
├── utils.h                          # Utilitaires
├── debug/                           # 🖥️ Dashboard machine
│   ├── dashboard_drv.h / dashboard_drv.cpp
│   ├── dashboard_input.h / dashboard_input.cpp
│   ├── dashboard_machine.h / dashboard_machine.cpp
│   ├── dashboard_sig.h / dashboard_sig.cpp
│   ├── dashboard_simulation.h / dashboard_simulation.cpp
│   └── dashboard_vbat.h / dashboard_vbat.cpp
└── input/                           # Mise à jour entrées
    └── input_update.h / input_update.cpp
```

---

## 🔵 SOUND MODULE — `src/sound_module/`

> **Statut** : fork intégré du projet `rc_engine_sound` (TheDIYGuy999). Conserve son propre dispatcher `MACHINE_*` **interne** au module. Sous-projet PIO distinct (`[env:sound_node_volvo]`). Rapatriement dans l'env machine prévu plus tard dans l'année.

```
src/sound_module/
├── main.cpp                     # Point d'entrée sound node
├── 0_generalSettings.h
├── 1_Vehicle.h
├── sound_module.md
├── config/                      # Configuration sound
│   ├── config.h
│   ├── boards/                  # Cartes sound (sound_board_esp32, _DIYGuy)
│   ├── machines/                # Dispatcher MACHINE_* → <instance>/<instance>.h
│   └── profiles/                # Profils son par TYPE de machine
│       ├── profiles.h           # Dispatcher MACHINE_* → <type>/<type>.h
│       └── dumper_truck/        # Profil dumper_truck
│           ├── dumper_truck.h
│           ├── dumper_truck.cpp
│           └── dumper_truck_sound.cpp
├── init/                        # Initialisation sound
│   ├── hw_init.cpp / hw_init.h
│   ├── hw_init_dcdev.cpp / hw_init_dcdev.h
│   ├── hw_init_esc.cpp / hw_init_esc.h
│   ├── hw_init_light.cpp / hw_init_light.h
│   ├── hw_init_sig.cpp / hw_init_sig.h
│   ├── hw_init_srv.cpp / hw_init_srv.h
│   ├── sound_init.cpp / sound_init.h
│   ├── com/                     # Com init (UART RX)
│   ├── input/                   # Input init
│   └── sys/                     # Sys init
├── src/                         # Code source tiers (rc_engine_sound)
│   ├── curves.h
│   └── helper.h
├── state/                       # État sound
│   ├── sound_state.cpp / sound_state.h
├── system/                      # Système sound
│   ├── combus_sound_interpreter.h/cpp
│   ├── engine_volume.h/cpp
│   ├── sound_behaviors.h/cpp
│   ├── sound_core.h/cpp
│   ├── sound_interpreter.h/cpp
│   ├── sound_mixer_state.h
│   ├── sound_system.h
│   ├── trailer_switch.h/cpp
│   ├── debug/                   # Dashboard sound
│   └── sound/                   # Sound HAL
└── vehicles/                    # 60+ véhicules tiers (VolvoFH16, ScaniaV8, Kenworth, ...)
    ├── 00_Master.h
    ├── 1000HpScaniaV8.h
    ├── 1965FordMustangV8.h
    ├── ... (60+ fichiers)
    ├── old/                     # Anciens véhicules
    └── sounds/                  # Assets audio
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

## Scripts de build

```
scripts/
└── combus_md5.py                # 🆕 extra_script PIO — génère combus_handshake_md5.h
                                 #   (MD5 des .inc REMOTE + version projet)
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
| **SOUND** | `src/sound_module/` | Module son (fork tiers, sous-projet PIO) |
| **SCRIPTS** | `scripts/` | Scripts de build (extra_scripts PIO) |

---

## Chaîne d'include typique (machine)

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
                          └─> core/config/machines/machine_type.h
                                └─> core/config/machines/dumper_truck/dumper_truck_config.h
                                      ├─> core/config/machines/dumper_truck/combus/combus_remote.h
                                      │     └─> core/config/machines/dumper_truck/combus/combus_ids_remote.h
                                      ├─> core/config/machines/dumper_truck/motion/dumper_truck_motion.h
                                      └─> core/config/machines/dumper_truck/sound/dumper_truck_sound.h
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
| Fragment | `*.inc` | Fragment statique inclus dans enums/tableaux |

---

## 🆕 Notes de migration (commits `13b4c5f` + `1e7612d`)

### Régression `CombusLayout`

L'enum `CombusLayout` et son champ associé dans `ComBusFrameCfg` / `CombusFrameHeader` ont été **supprimés**. Le wire format est raccourci d'1 octet. Aucun mécanisme de remplacement (handshake, MD5, etc.) n'a été implémenté — uniquement la régression propre.

### Split handshake RX/TX

`combus_handshake.{h,cpp}` a été splitté en :
- `combus_handshake.{h,cpp}` — umbrella (constants + boot helper + `combus_handshake_formatMd5Hex`)
- `combus_handshake_rx.{h,cpp}` — RX (decode + CRC + compare-and-log)
- `combus_handshake_tx.{h,cpp}` — TX (frame build + manual `sendOnce`)

### Dispatcher `machine_type.h`

`src/core/config/machines/machine_type.h` est le **nouveau dispatcher top-level** :
```
machine_type.h
  └─> <machine_type>_config.h
        └─> <machine_type>/combus/combus_remote.h
              └─> <machine_type>/combus/combus_ids_remote.h
```

Le dispatcher racine `src/core/config/machines/combus_ids_remote.h` (qui doublonnait cette chaîne) a été **supprimé**.

### Renommage `.inc`

`combus_channels_remote_*.inc` → `combus_remote_*.inc` (préfixe correct, le mot "channels" était trompeur — ces fragments contiennent des **noms de canaux**, pas des bytes).

### `project_version.h` à la racine

`include/project_version.h` → `project_version.h` (racine, peer de `platformio.ini`). Switché de `#define` à `static constexpr uint8_t`. Le script `scripts/combus_md5.py` accepte les deux formes.
