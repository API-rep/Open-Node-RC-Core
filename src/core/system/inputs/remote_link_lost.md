# REMOTE_LINK_LOST — Chaîne d'agrégation de santé du lien remote

> **Note** : ce document décrit l'architecture de la chaîne `REMOTE_LINK_LOST`.  Il est complémentaire à `src/core/system/failsafe/failsafe_module.md` qui décrit le module failsafe central.

---

## 1. Vue d'ensemble

`REMOTE_LINK_LOST` est un signal **distinct** du `FAILSAFE` global.  Il représente l'état de santé du lien remote (PS4 Bluetooth, UART, etc.) et déclenche une réaction différente : mise en attente (`RunLevel::IDLE`) plutôt que sécurité hardware complète (`RunLevel::FAILSAFE`).

**Différence sémantique avec `FAILSAFE`** :

| Signal | Sémantique | Réaction |
|--------|------------|----------|
| `FAILSAFE` | Vraie faute (VBAT basse, etc.) | `RunLevel::FAILSAFE` (sécurité hardware complète) |
| `REMOTE_LINK_LOST` | Silence du lien remote (déconnexion, perte signal) | `RunLevel::IDLE` (mise en attente, reprise automatique) |

**Justification** : une déconnexion volontaire et brève (ex. changer de machine pilotée) ne doit **pas** déclencher la séquence de sécurité complète (`FAILSAFE`), seulement une mise en attente (`IDLE`) avec reprise automatique.

---

## 2. Architecture

```text
PS4_DS4_BT_LINK_LOST ──┐
                       │
UART_LINK_LOST ────────┼──► OR-guard (cb_or_fn) ──► REMOTE_LINK_LOST
                       │                              │
(future contributeurs)──┘                              │
                                                      ▼
                                              kRunlevelProcs[]
                                              (cb_runlevel_once_fn)
                                                      │
                                                      ▼
                                              RunLevel::IDLE
                                              (front montant)
```

**Priorité** : `failsafe > remote_link > runlevel` — une vraie faute (`FAILSAFE`) l'emporte toujours sur une simple perte de lien.

---

## 3. Fichiers

```text
src/core/system/inputs/
├── remote_link_lost.cb           // Canal REMOTE_LINK_LOST (agrégat)
├── ps4_ds4_bt_link_lost.cb       // Canal PS4_DS4_BT_LINK_LOST (contributeur)
├── uart_link_lost.cb             // Canal UART_LINK_LOST (contributeur)
├── remote_link_fallback_chain.h  // Déclarations kRemoteLinkFallbackChain[]
└── remote_link_fallback_chain.cpp // Chaîne d'agrégation (reset + OR-guard)
```

---

## 4. Contributeurs

### 4.1 — `PS4_DS4_BT_LINK_LOST`

**Canal** : `src/core/system/inputs/ps4_ds4_bt_link_lost.cb`

**Propriétaire** : `src/machines/system/input/input_update.cpp` (backend PS4_BT)

**Réarmement** : `combus_set_digital(bus, DigitalComBusID::PS4_DS4_BT_LINK_LOST, false, ChanLayer::LOCAL)` quand le contrôleur est actif.

**Gating** : `#if defined(INPUT_PS4_DS4_BT)`

### 4.2 — `UART_LINK_LOST`

**Canal** : `src/core/system/inputs/uart_link_lost.cb`

**Propriétaire** : `src/sound_module/system/combus_sound_interpreter.cpp` (côté sound node)

**Réarmement** : publié selon le RX-timeout (pas de frame dans la fenêtre → `true`).

**Gating** : `#if defined(COMBUS_UART_TX) || defined(COMBUS_UART_RX) || defined(COMBUS_UART)`

---

## 5. Chaîne d'agrégation

**Fichier** : `src/core/system/inputs/remote_link_fallback_chain.cpp`

**Pattern** : miroir de `failsafe_chain.cpp` — reset en tête, puis OR-guard sur chaque contributeur.

**Gating global** : `#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)` — si le flag n'est pas défini, la chaîne est un no-op complet (tableau placeholder de taille 1, count = 0).

**Sortie** : `DigitalComBusID::REMOTE_LINK_LOST` (canal LOCAL, scope partagé par tous les boards du node local).

---

## 6. Réaction runlevel

**Fichier** : `src/machines/config/machines/volvo_A60H_bruder/combus/processors/runlevel/runlevel_procs.h`

**Processor** : `cb_runlevel_once_fn` (edge-triggered) câblé sur `DigitalComBusID::REMOTE_LINK_LOST`.

**Config** : `kRemoteLinkRunlevelCfg`
```cpp
static constexpr CbRunlevelCfg kRemoteLinkRunlevelCfg {
    .high  = RunLevel::IDLE,        // REMOTE_LINK_LOST 0→1 → write IDLE
    .low   = RunLevel::STARTING,    // REMOTE_LINK_LOST 1→0 → write STARTING (recovery)
    .claim = true,                  // Block KEY_ACTIVE proc on link loss
};
```

**Comportement** :
- Front montant (lien perdu) → `RunLevel::IDLE` + claim
- Front descendant (lien retrouvé) → `RunLevel::STARTING` (pas de cold-start requis, reprise directe)

---

## 7. Orchestration

**Fichier** : `src/machines/system/sys_manager.cpp`

**Ordre d'exécution** :
```cpp
void sys_manager_update(ComBus& bus) {
    // 1. Input acquisition
    input_refresh();
    input_update(bus);

    // 2. vbat update
    vbat_update();

    // 3. Failsafe chain (publie FAILSAFE)
    failsafe_update(bus);

    // 4. REMOTE_LINK_LOST → IDLE fallback (chantier 12.5)
#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)
    remote_link_fallback_update(bus);
#endif
}
```

**Note** : `remote_link_fallback_update()` tourne **après** `failsafe_update()` et **avant** la FSM RunLevel.  Cela garantit que `REMOTE_LINK_LOST` est frais pour le même cycle.

---

## 8. Consommateurs

| Consommateur | Fichier | Usage |
|--------------|---------|-------|
| `kRunlevelProcs[]` | `runlevel_procs.h` | Force `RunLevel::IDLE` sur front montant |
| `dashboard_machine.cpp` | `src/machines/system/debug/` | Indicateur `DRV / ---` |
| `dashboard_input.cpp` | `src/machines/system/debug/` | Indicateur `*` (healthy) |
| `init.cpp` | `src/machines/init/` | Condition de sortie du bloc PAUSE |

---

## 9. Référence rapide

| Concept | Implémentation |
|---------|----------------|
| Canal agrégat | `src/core/system/inputs/remote_link_lost.cb` |
| Chaîne `kRemoteLinkFallbackChain[]` | `src/core/system/inputs/remote_link_fallback_chain.cpp` |
| Processor `cb_or_fn` | `src/core/system/combus/processors/logic/cb_or.h` |
| Réaction runlevel | `src/machines/config/machines/volvo_A60H_bruder/combus/processors/runlevel/runlevel_procs.h` |
| Orchestration | `src/machines/system/sys_manager.cpp` |
