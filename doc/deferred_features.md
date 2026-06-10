# Deferred Features — Winter 2026 Backlog

> Features architecturalement définies mais non implémentées.
> Ne pas commencer avant la fin de saison 2026.
> Dernière mise à jour : 2026-06-10.

---

## 1. SimDev — ComBus inertia processor

**Objectif :** Extraire la FSM d'inertie ESC (actuellement dans `sound_module/main.cpp`, ~400 lignes)
en un processor ComBus pur `SimDev` (lit un canal, applique dynamiques, écrit un canal).

**Architecture :**
```
ComBus[inCh]  ──►  sim_dev_update()  ──►  ComBus[outCh]
                   (inertie, FSM 4 états,   (lu par srvDev/dcDev comme canal normal)
                    gear-ramp, failsafe)
```

**Hardware binding séparé :**
- ESC RC sur port servo → `SrvDevice` avec `SrvDevType::ESC_RC` (nouveau type à ajouter dans `machines_defs.h`).
- Pas de logique inertie dans `SrvDevice` — uniquement pin + plage pulse.

**Structs proposées :**
```cpp
struct SimCfg {
    uint16_t rampTime[3];      // ms par rapport
    uint8_t  brakeSteps;
    uint8_t  accelSteps;
    uint16_t neutralBand;      // unités ComBus autour de CbusNeutral = STAND
    uint8_t  lowRangePct;
    uint8_t  autoReverseAccelPct;
    uint16_t crawlerRampTime;
};
struct SimState {
    uint16_t inertiaPos;       // position courante [0..CbusMaxVal]
    int8_t   driveState;       // 0=STAND 1=FWD 2=BRK_FWD 3=REV 4=BRK_REV
    bool     escInReverse;
    bool     escIsBraking;
    bool     escIsDriving;
    bool     brakeDetect;
    uint16_t currentRampTime;
    uint32_t lastUpdateMs;
};
struct SimDev {
    const int8_t    ID;
    const char*     infoName;
    AnalogComBusID  inCh;
    AnalogComBusID  outCh;
    const SimCfg*   cfg;
    SimState*       state;
};
```

**Migration :**
1. Ajouter `SrvDevType::ESC_RC` dans `machines_defs.h`.
2. Définir `SimCfg`, `SimState`, `SimDev` dans `include/struct/machines_struct.h`.
3. Ajouter `simDev` / `simDevCount` dans `EnvCfg`.
4. Extraire `sim_dev_update()` de `esc()` dans `sound_module/main.cpp`.
5. Remplacer `esc()` par boucle sur `simDev[]`.
6. Supprimer les locals statiques (`escPulseWidth`, `driveRampRate`, `escMillis`…) → `SimState`.

---

## 2. TRACTION_BUS packed format (ComBus v2)

**Décision arrêtée 2026-05-17** — remplace `ESC_SPEED_BUS` + `DRIVE_STATE_BUS` :
```
bit15  : BRAKE     — état freinant actif
bit14  : FWD       — direction avant (0 = arrière)
bit13-0: magnitude — vitesse 0..8191
```
Décodeur : `mag = v & 0x1FFFu`, `fwd = v & 0x4000u`, `brake = v & 0x8000u`. Standing = `0x0000`.

**Impact :**
- Helpers `CbTraction::encode()/decode()` à créer (modèle : `CbAnalog` dans `combus_access.h`).
- `cb_dir_fn` devient obsolète (direction intégrée dans `TRACTION_BUS`).
- `DriveStateBus` retiré de `motion_struct.h`.
- Tous les consommateurs de `ESC_SPEED_BUS` mis à jour simultanément.
- Voir `@todo` dans `combus_ids.h` → `GEAR_SHIFTING`.

---

## 3. ComBus v2 — architecture bus en couches

**Décision arrêtée 2026-04-18** :
```
ComBus Core  { RunLevel, batteryIsLow, nodeId, sequence }        ← échangé par TOUS
GlobalLightBus  { digital[], analog[] }                          ← inter-node
GlobalSoundBus  { digital[], analog[] }                          ← inter-node
LocalLightBus   { LightState, LedDescriptor[] }                  ← sound node only
LocalSoundBus   { EngineSimState, ... }                          ← sound node only
```

**Règle dure :** toute donnée inter-node → ComBus Core ou GlobalXxxBus. LocalXxxBus = privé au nœud.

**Prérequis :** Refonte complète structs ComBus + transport frame. Ne pas commencer avant fin saison 2026.

---

## 4. GEAR_SHIFTING → canal WIRE (ComBus v2)

`GEAR_SHIFTING` est actuellement machine-local digital.
Doit être promu en canal WIRE avant `WIRE_END` pour que le sound node puisse le lire
directement — remplace la détection heuristique dans la FSM son.

Voir `@todo` en place dans `combus_ids.h` → `GEAR_SHIFTING`.
À faire lors du rework `TRACTION_BUS` (§2 ci-dessus).

---

## 5. SoundDevice — moteur son table-driven

**Objectif :** Remplacer le monolithe `main.cpp` (1637 lignes, `pulseWidth[]`, fonctions `#ifdef`) par
un tableau de `SoundDevice` avec `SoundBehaviorFn` par device.

**Struct (décidée) :**
```cpp
struct SoundCfgHdr { int8_t gateDevID; };  // offset 0 obligatoire dans tous les cfg
struct SoundDevice {
    const int8_t                   ID;
    const char*                    infoName;
    DevUsage                       usage;
    std::optional<AnalogComBusID>  analogChan;
    std::optional<DigitalComBusID> digitalChan;
    SoundBehaviorFn                behaviorFn;
    const void*                    cfg;
    SoundDevState*                 state;
};
```

**Migration (dans l'ordre) :**
| # | Scope | Changement |
|---|-------|------------|
| 1 | `include/struct/sound_struct.h` | Ajouter `SoundDevice` + `SoundDevState` |
| 2 | `config/profiles/dumper_truck/` | Remplacer `soundMapper[]` par `kSoundDevArray[]` |
| 3 | `system/sound_interpreter.cpp` | Itérer `kSoundDevArray[]`, appeler `behaviorFn()` |
| 4 | `main.cpp` | Supprimer blocs `#ifdef LOADER_MODE loaderControl()` etc. |
| 5 | `main.cpp` | Supprimer `pulseWidth[]` consommateurs |
| 6 | Legacy tab headers | Absorber dans `vehicle_legacy_cfg.h` |

**Prérequis :** DiYGuy external linkage migration (L2 phases A–D) complète en premier.

---

## 6. Light module v2 — LedDescriptor + bitmask runlevels

**Objectif :** Remplacer `light_core.cpp` (port legacy) par une architecture table-driven.

**Décisions arrêtées 2026-04-18 :**
- `LedDescriptor` par canal : section `constexpr` (flash) + section état RAM.
- `activeMask : uint8_t` — bitmask runlevels (quand activer, pas seulement comment).
- Sous-structs comportementaux optionnels : `BeaconCfg`, `XenonCfg`, `IndicatorCfg`.
- Array variable par enum (même pattern que `DcDevID`).
- Parser `light_core_update()` : itération table, zéro `switch(lightsState)`.

**Prérequis :** Refactor `statusLED` en classe propre.

---

## 7. SwitchDevice → EnvCfg

**Objectif :** Séparer la config board (`SwitchPortCfg` avec pin/pullUp/activeHigh/debounce)
de la config machine (`SwitchDevice` avec usage/parentID dans `EnvCfg`).

```cpp
struct SwitchPortCfg { const char* infoName; int8_t pin; bool pullUp; bool activeHigh; uint16_t debounceMs; };
struct SwitchDevice  { const int8_t ID; const char* infoName; DevUsage usage; int8_t parentID = -1; };
```

**Prérequis :** ≥ 2 modules switch actifs simultanément pour justifier l'infrastructure.

---

## 8. Simulation struct — unification son/motion

**Objectif :** Éviter la duplication des paramètres dynamiques (courbes moteur, ratios, dynamiques)
entre config son et config motion.

**Modèle RPM-primary (décidé 2026-05-14) :**
```
stick → targetRpm → inertie sur RPM → currentRpm
                                    → speed = currentRpm × gearRatio[gear]
                                    → commande moteur ComBus
```
`GearShiftProfile` gagne `gearRatio[]`. `MotionRuntime::currentRpm` = variable primaire.

**Prérequis :** Saison 2026 terminée. Profils son/motion stabilisés.

---

## 9. Petites features différées

### ExtPort conflict detection
Pool `constexpr` de ports dans le header board. `port_claim(pin, "owner")` en init → assert sur doublon.
Prérequis : ≥ 2 modules output actifs simultanément.

### Sound transport cap cross-check
`static_assert(ComBusUartTxHz <= SoundRxMaxHz)` dans `output_init.h` une fois que `sound_config.h`
expose `SoundRxMaxHz` + `SoundRxMaxBaud` comme `constexpr`.

### Debug serial opt-out (`-D NO_DEBUG_SERIAL`)
`sys_log_info` et `Serial.begin()` supprimés à la compile pour architectures mono-UART.
Prérequis : une board réelle avec un seul UART disponible.

### ComBus bidirectionnel (`combus_link_init`)
Wrapper appelant `combus_tx_init()` + `combus_rx_init()` pour les nœuds bidirectionnels futurs.
Prérequis : un nœud bidirectionnel concret dans le projet.

### `kMotion_Hydraulic_Slow`
Preset `MotionRamp { .rampTimeMs = 200u, ... }` dans `simulation_presets.h`.
Créer quand un canal hydraulique utilisera la chaîne motion.
