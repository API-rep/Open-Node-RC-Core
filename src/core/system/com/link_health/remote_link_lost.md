# FAILSAFE_SOFT — Chaîne d'agrégation de santé du lien remote

> **Note** : ce document décrit l'architecture de la chaîne `FAILSAFE_SOFT`.  Il est complémentaire à `src/core/system/failsafe/failsafe_module.md` qui décrit le module failsafe central.

---

## 1. Vue d'ensemble

`FAILSAFE_SOFT` est un signal **distinct** du `FAILSAFE` global.  Il représente l'état de santé du lien remote (PS4 Bluetooth, UART, etc.) et déclenche une réaction différente : mise en attente (`RunLevel::IDLE`) plutôt que sécurité hardware complète (`RunLevel::HALT`).

**Différence sémantique avec `FAILSAFE`** :

| Signal | Sémantique | Réaction |
|--------|------------|----------|
| `FAILSAFE` | Vraie faute (VBAT basse, etc.) | `RunLevel::HALT` (sécurité hardware complète) |
| `FAILSAFE_SOFT` | Silence du lien remote (déconnexion, perte signal) | `RunLevel::IDLE` (mise en attente, reprise automatique) |

**Justification** : une déconnexion volontaire et brève (ex. changer de machine pilotée) ne doit **pas** déclencher la séquence de sécurité complète (`FAILSAFE`), seulement une mise en attente (`IDLE`) avec reprise automatique.

---

## 2. Architecture

```text
LOCAL_REMOTE_FAIL ──┐
                       │
NODE_LINK_FAIL ─────────┼──► OR-guard (cb_or_fn) ──► FAILSAFE_SOFT
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
src/core/system/com/link_health/
├── remote_link_lost.cb           // Canal FAILSAFE_SOFT (agrégat)
├── com_link_lost.cb              // Canal NODE_LINK_FAIL (contributeur, requires HAS_REMOTE_LINK_LOST_FALLBACK)
├── failsafe_com_link.cb          // Canal BOARD_LINK_FAIL (contributeur de FAILSAFE, requires HAS_FAILSAFE)
├── remote_link_fallback_chain.h  // Déclarations kRemoteLinkFallbackChain[]
└── remote_link_fallback_chain.cpp // Chaîne d'agrégation (reset + OR-guard)

src/core/config/inputs/
└── ps4_ds4_bt_link_lost.cb       // Canal LOCAL_REMOTE_FAIL (contributeur, requires INPUT_PS4_DS4_BT)
```

Les quatre canaux sont du thème `com`.

---

## 4. Contributeurs

### 4.1 — `LOCAL_REMOTE_FAIL`

**Canal** : `src/core/config/inputs/ps4_ds4_bt_link_lost.cb` (à côté du backend PS4, `requires: INPUT_PS4_DS4_BT`)

**Propriétaire** : `src/machines/system/input/input_update.cpp` (backend PS4_BT)

**Réarmement** : `combus_set_digital(bus, DigitalComBusID::LOCAL_REMOTE_FAIL, false, ChanLayer::BOARD)` quand le contrôleur est actif.

**Gating** : `#if defined(INPUT_PS4_DS4_BT)`

### 4.2 — `NODE_LINK_FAIL`

**Canal** : `src/core/system/com/link_health/com_link_lost.cb` (BOARD, `requires: HAS_REMOTE_LINK_LOST_FALLBACK`)

**Propriétaire** : `com_rx_update()`, `src/core/system/com/com_manager.cpp`

**Réarmement** : écrit à chaque cycle avec `combus_links_lost(LinkLossAction::IDLE)` : vrai tant qu'un lien qui reçoit, de politique de perte `IDLE` (`ComDev::onLoss`, par défaut un lien inter-nœud), n'est pas vivant, jamais vu compris. Un lien qui n'émet que ne compte jamais.

**Gating** : aucun dans la chaîne (faux à chaque cycle sur une carte sans tel lien).

### 4.3 — La politique de perte d'un lien (`onLoss`)

Chaque `ComDev` porte sa réaction à la perte (`LinkLossAction`) :

| `onLoss` | Réaction | Quand le lien compte comme perdu |
|---|---|---|
| `BY_SCOPE` (défaut) | `FAILSAFE` pour un lien intra-nœud, `IDLE` pour un lien inter-nœud | — |
| `IDLE` | `NODE_LINK_FAIL` → `FAILSAFE_SOFT` → runlevel `IDLE` | jamais vu, ou silence > `linkTimeoutMs` |
| `FAILSAFE` | `BOARD_LINK_FAIL` → `FAILSAFE` | silence > `linkTimeoutMs`, ou jamais vu après `startupGraceMs` (3 s sur la Volvo) |
| `NONE` | journal seulement | — |

Chaque perte et chaque retour d'un lien est journalisé une fois (`combus_links_update()`). Si plusieurs liens tombent, `FAILSAFE` l'emporte sur `IDLE` par la priorité du runlevel.

---

## 5. Chaîne d'agrégation

**Fichier** : `src/core/system/com/link_health/remote_link_fallback_chain.cpp`

**Pattern** : miroir de `failsafe_chain.cpp` — reset en tête, puis OR-guard sur chaque contributeur.

**Gating global** : `#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)` — si le flag n'est pas défini, la chaîne est un no-op complet (tableau placeholder de taille 1, count = 0).

**Sortie** : `DigitalComBusID::FAILSAFE_SOFT` (canal SYSTEM, scope partagé par toutes les cartes du nœud).

---

## 6. Réaction runlevel

**Fichier** : `src/machines/config/machines/volvo_A60H_bruder/combus/processors/runlevel/runlevel_procs.h`

**Processor** : `cb_runlevel_once_fn` (edge-triggered) câblé sur `DigitalComBusID::FAILSAFE_SOFT`.

**Config** : `kRemoteLinkRunlevelCfg`
```cpp
static constexpr CbRunlevelCfg kRemoteLinkRunlevelCfg {
    .high  = RunLevel::IDLE,        // FAILSAFE_SOFT 0→1 → write IDLE
    .low   = std::nullopt,          // FAILSAFE_SOFT 1→0 → no write (stays IDLE until KEY)
    .claim = true,                  // Block KEY_ACTIVE proc on link loss
};
```

**Comportement** :
- Front montant (lien perdu) → `RunLevel::IDLE` + claim
- Front descendant (lien retrouvé) → aucune écriture : la machine reste en `IDLE` et c'est l'opérateur qui la redémarre avec la clé (Triangle). Même chose au premier branchement de la manette après le démarrage.

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

    // 4. FAILSAFE_SOFT → IDLE fallback (chantier 12.5)
#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)
    remote_link_fallback_update(bus);
#endif
}
```

**Note** : `remote_link_fallback_update()` tourne **après** `failsafe_update()` et **avant** la FSM RunLevel.  Cela garantit que `FAILSAFE_SOFT` est frais pour le même cycle.

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
| Canal agrégat | `src/core/system/com/link_health/remote_link_lost.cb` |
| Chaîne `kRemoteLinkFallbackChain[]` | `src/core/system/com/link_health/remote_link_fallback_chain.cpp` |
| Processor `cb_or_fn` | `src/core/system/combus/processors/logic/cb_or.h` |
| Réaction runlevel | `src/machines/config/machines/volvo_A60H_bruder/combus/processors/runlevel/runlevel_procs.h` |
| Orchestration | `src/machines/system/sys_manager.cpp` |
