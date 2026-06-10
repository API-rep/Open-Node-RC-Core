# Project Architecture Reference


## 1. Vue d'ensemble du projet

firmware multi-nœuds pour véhicules RC de chantier (modélisme).


```
[ Télécommande PS4 ]
        │  Bluetooth
        ▼
[ Machine node — ESP32 ]  ─── UART ───► [ Sound node — ESP32 ]
   src/machines/               src/sound_module/
   - Input processing          - ComBus RX
   - Motion simulation         - Sound engine
   - Driver output             - Servo/DC output
        │
        ▼
[ Moteurs, servos, lumières ]
```

**PlatformIO** — `espressif32@6.7.0` (IDF 4.4, Arduino).
Véhicule actif : **Volvo A60H Bruder** (`volvo_A60H_bruder`).

---

## 2. Hiérarchie config véhicule (`src/machines/config/`)

```
machines.h
  └─ (MACHINE == VOLVO_A60_H_BRUDER)
       volvo_A60H_bruder/volvo_A60H_bruder.h      ← Level 0 : kVehicleName, kVehicleCombusLayout, MACHINE_TYPE
         ├─ (IS_MAINBOARD) mainboard/mainboard.h  ← umbrella env
         │    └─ (BOARD == ESP32_8M_6S) ESP32_8M_6S/envCfg.h/.cpp
         └─ (IS_EXT_BOARD) ext_board/ext_board.h  ← (futur)
```

**Build flags :**
| Flag | Rôle |
|------|------|
| `-D MACHINE=VOLVO_A60_H_BRUDER` | Sélectionne le véhicule dans `machines.h` |
| `-D IS_MACHINE` | Flag structurel ComBus (toujours présent dans `[env:machines]`) |
| `-D IS_MAINBOARD` | Sélectionne l'arbre mainboard |
| `-D IS_EXT_BOARD` | Sélectionne l'arbre extension board (sound node) |
| `-D BOARD=ESP32_8M_6S` | Override board (optionnel, défaut dans `mainboard.h`) |

**Règles :**
- Level 0 déclare **uniquement** `kVehicleName`, `kVehicleCombusLayout`, `MACHINE_TYPE`.
- `envCfg.h` utilise ces constantes — aucun string literal.
- `MACHINE_TYPE` = `#define MACHINE_TYPE DUMPER_TRUCK` — valeur enum C++, **pas** entier pour dispatch préprocesseur.
- Dispatch préprocesseur → toujours `#if MACHINE == <vehicle_id>`.
- Nouveau véhicule = nouveau dossier `<vehicle>/` + même structure 3 niveaux.

---

## 3. Hiérarchie config machine-class (`src/core/config/machines/`)

Config partagée entre **tous les environnements** d'une même classe de machine.

```
src/core/config/machines/
  machine_config.h          ← dispatcher top : #if MACHINE == <id> → umbrella class
  combus_ids.h              ← dispatcher struct-safe (zéro cycle) : *_ids.h seulement
  combus_types.h            ← dispatcher ComBus complet : *combus.h
  <class>/
    <class>_config.h        ← umbrella class (combus + motion + inputs_map + sound)
    combus/<class>.h/.cpp   ← IDs canaux, extern arrays, extern comBus
    combus/<class>_ids.h    ← IDs seuls (zéro deps — sûr pour headers struct)
    motion/<class>_motion.h ← alias preset traction
    inputs_map/             ← mapping input → canal ComBus
    sound/<class>_sound.h/.cpp
```

**Classes définies :**
- `dumper_truck/` — tombereaux articulés (actif : Volvo A60H)
- `excavator/`   — bras hydraulique (combus/motion/inputs_map : TODO winter 2026)
- `loader/`      — chargeur sur roues (combus/motion/inputs_map : TODO winter 2026)

**Multi-class `.cpp` guard :** symbole global dans un `*_sound.cpp` → wrapper `#if MACHINE == <id>`.

---

## 4. Couche transport (`src/core/system/com/`)

```
NodeCom           — table de pointeurs de fonction (write/readByte/available + ctx)
ports/uart_com    — adaptateur UART : Serial.begin() + claim guard (pool 3 ports statique)
protocol/combus_tx — TX ComBus, transport-agnostique
protocol/combus_rx — RX ComBus, transport-agnostique
```

**Règles :**
- `uart_com_init` appelé **une seule fois par port** (init top-level). Retourne `NodeCom*`.
- `combus_tx_init` / `combus_rx_init` reçoivent un `NodeCom*` — jamais `Serial.begin()` ni pins.
- Guard : `uart_com_init` retourne `nullptr` + erreur fatale si le port est déjà claimé.
- Nouveau transport = nouveau `adapter/*_com.h/.cpp` implémentant les 3 pointeurs de fonction.

---

## 5. ComBus — règle cross-core permanente

> **Ne jamais recréer `syncEngineSimState()` ni de flat global cross-core.**
> Toute nouvelle donnée partagée entre cores → champ `volatile` dans `gEngineSimState`.

**Variables flat restantes (par conception, non migrées) :**
| Variable | Raison |
|----------|--------|
| `escInReverse` | Core 0 écrit ET lit — même core, pas de risque |
| `hydraulicLoad` | Core 1 écrit, Core 0 + 1 lisent — volatile flat maintenu |
| `engineRunning`, `engineState`, `engineOn` | FSM Core 0, lu Core 1 — migration winter 2026 |
| `driveState` | Core 0 écrit, Core 1 lit — migration winter 2026 |

---

## 6. Pipeline ComBus Processors

Voir **[`src/core/system/combus/processors/README.md`](../src/core/system/combus/processors/README.md)** pour l'inventaire complet et le roadmap.

Architecture des chaînes actives (Volvo A60H) :

```
INPUT chain
  THROTTLE_STICK → [passthrough] → THROTTLE_BUS (bipolaire)
  Boutons        → [cb_btn]      → SUBGEAR_BUS, DIRECT_DRIVE, CRUISE_ACTIVE, CRUISE_UPDATE_BTN

SIM chain — THROTTLE
  THROTTLE_BUS → bypass(DIRECT_DRIVE) → ramp(cb_sym_ramp_fn) → dir(→DRIVE_STATE_BUS)
              → b-in(BRAKE_BUS→state) → brake(→BRAKE_BUS+extBrakeSteps)
              → cruise_sync → cruise_upd → cruise
              → center → abs → scale → ESC_RPM_BUS

SIM chain — GEAR
  ESC_RPM_BUS → ratio_inv(→ESC_RPM_BUS=engine_rpm) → subgear-claim → direct-claim
             → gear_fsm(DRIVE_STATE_BUS) → gear_dyn_ramp → GEAR

SIM chain — TRACTION
  ESC_RPM_BUS → upshift_damp → subgear_cap → gear_dir(DRIVE_STATE_BUS) → ESC_SPEED_BUS
```

---

## 7. Dashboard (`-D DEBUG_DASHBOARD`)

Architecture 4 couches :

| Couche | Localisation | Rôle |
|--------|-------------|------|
| Core shell | `src/core/system/debug/dashboard.h/.cpp` | Primitives frame, nav bar, clavier, ring buffer, slot registry |
| Env dashboard | `src/machines/system/debug/dashboard_machine.h/.cpp` | Vue overview, auto-détection modules, registration slots |
| Module views | `src/machines/system/debug/dashboard_drv/input/vbat.h/.cpp` | Un écran par sous-module (live + config-at-init en detail) |
| Serial init log | `src/core/system/debug/hw_init_debug.cpp` etc. | Dumps one-shot — à absorber par Layer 3 au fur et à mesure |

**Règles :**
- Zéro code dashboard dans `main.cpp`, `machine_init()`, `loop()` — sauf `dashboard_update()` et `dashboard_machine_setup()`.
- Événements runtime → `dashboard_push_event()` uniquement.
- Inner content width : `DashInnerW = 120`. Style : single-line box drawing.
- Hauteur de ligne **uniforme** par section de contenu (pas de layout shift entre sub-items).
- Refresh : `DashRefreshMs = 200 ms`. Keyboard : non-bloquant.

---

## 8. Règles de style et flags

### Debug flags
- Utiliser **uniquement** les flags partagés : `DEBUG_INPUT`, `DEBUG_HW`, `DEBUG_SYSTEM`, `DEBUG_COMBUS`, `DEBUG_ALL`.
- Pas de flag ad-hoc par module.
- Activation centralisée dans `src/core/system/debug/debug.h`.

### `static_assert` placement
- Assert sur constantes d'un **seul** header → dans ce header.
- Assert croisant **deux chaînes d'include distinctes** → dans le `.cpp` init où les deux chaînes sont résolues.

### Règle cross-core permanente
→ Voir §5 ci-dessus.
