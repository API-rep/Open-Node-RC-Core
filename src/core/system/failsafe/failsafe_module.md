# Failsafe Module — Architecture

> **Note** : ce document décrit l'architecture du module failsafe central.  La chaîne `REMOTE_LINK_LOST` (signal séparé) est documentée dans `src/core/system/inputs/remote_link_lost.md`.

---

## 1. Architecture générale

Le failsafe est un module **inconditionnel** situé dans :

```text
src/core/system/failsafe/
```

Il existe dans toutes les builds, sans `HAS_FAILSAFE`.

En version minimale, il contient :

- le ComBus global `FAILSAFE` ;
- la chaîne de processors failsafe ;
- un processor initial de reset ;
- aucune source de failsafe optionnelle.

Une chaîne vide, hors reset, est donc valide.

---

## 2. Responsabilité du module `core/system/failsafe`

Le module central est responsable de :

- déclarer le ComBus global `FAILSAFE` ;
- agréger les sous-ComBus `FAILSAFE_X` ;
- construire la chaîne principale de processing ;
- remettre `FAILSAFE` à `false` au début de chaque cycle ;
- intégrer conditionnellement les contributions des autres modules ;
- ne connaître ni les RunLevels, ni les machines, ni les remotes.

Le failsafe produit uniquement :

```text
FAILSAFE = false → système sans fault détecté
FAILSAFE = true  → au moins un fault détecté
```

Il ne décide pas directement quoi faire de cet état.

---

## 3. Organisation des modules contributeurs

Chaque module reste propriétaire de sa logique failsafe.

Exemple :

```text
core/system/
├── failsafe/
│   ├── failsafe.h/.cpp
│   ├── failsafe_chain.h/.cpp
│   └── failsafe.cb
│
└── vbat/
    ├── vbat.h/.cpp
    ├── vbat_sense.h/.cpp
    ├── vbat_alert.h/.cpp
    └── vbat_failsafe.cb
```

Le module central agrège les contributions via la chaîne `kFailsafeChain[]` (déclarée dans `failsafe_chain.cpp`).  Chaque contributeur est un `CbProc` câblé sur un canal `FAILSAFE_X` avec le pattern `cb_or_fn` (consume + guard).

Le processor et la logique métier restent dans leur module d'origine.  Le module `failsafe` central ne contient pas de logique spécifique VBAT, ComBus, température, etc.

---

## 4. ComBus global et sous-ComBus

Chaque source possède son propre sous-ComBus :

```text
FAILSAFE
FAILSAFE_VBAT
FAILSAFE_XXX
```

La relation est :

```text
module source
    │
    ├── réarme FAILSAFE_X
    │
    ▼
FAILSAFE_X
    │
    ▼
processor FAILSAFE_X (cb_or_fn)
    │
    ▼
FAILSAFE
```

Le module source connaît son propre état.  Le module failsafe central agrège tous les états.

---

## 5. Cycle de vie des `FAILSAFE_X`

Principe :

> Chaque `FAILSAFE_X` doit être réarmé à chaque cycle par son module propriétaire, puis son processor failsafe le remet systématiquement à `FAULT`.

Exemple :

```text
cycle N
  module VBAT → FAILSAFE_VBAT = healthy
  chaîne failsafe → processor VBAT lit, évalue, force FAILSAFE_VBAT = fault

cycle suivant
  module VBAT tourne      → réarme FAILSAFE_VBAT
  module VBAT ne tourne plus → FAILSAFE_VBAT reste fault
```

`FAILSAFE_X` représente à la fois un état de santé et une preuve de vie.  Pas besoin de TTL ni de timestamp.

---

## 6. Contrat d'un processor failsafe

```text
Entrées : FAILSAFE (global) + FAILSAFE_X (levier/état)
Sorties : FAILSAFE (global, éventuellement activé) + FAILSAFE_X (toujours forcé à fault)

1. FAILSAFE global déjà true ?
   oui → skip logique métier, force FAILSAFE_X = fault
   non → vérifie FAILSAFE_X, évalue le levier, active FAILSAFE si nécessaire, force FAILSAFE_X = fault
```

Pas d'early exit global de la chaîne — tous les processors tournent à chaque cycle pour que tous les `FAILSAFE_X` soient consommés et remis à `FAULT`.  Une fois `FAILSAFE` activé, les processors suivants bypassent leur logique métier mais exécutent toujours leur maintenance (`force FAILSAFE_X = fault`).  Comportement : **guarded execution** / **local short-circuit**.

---

## 7. Chaîne principale

```text
[ RESET FAILSAFE ]
        │
        ▼
[ FAILSAFE VBAT ]
        │
        ▼
[ FAILSAFE ... ]
        │
        ▼
[ FAILSAFE XXX ]
```

`FAILSAFE` est une forme de latch limitée au cycle : `false` en tête, peut passer à `true` pendant le cycle, reste dans son état en fin de cycle, reset à `false` au cycle suivant.

---

## 8. Initialisation et passe à vide

Au démarrage, une première passe peut avoir lieu avant que tous les modules aient produit leur état normal.  Pas besoin d'état `INITIALIZING` explicite — le fonctionnement normal se met en place après cette passe à vide, à condition que l'orchestrateur garantisse l'ordre (voir §10).

---

## 9. Réaction au `FAILSAFE`

Le module central ne connaît ni `RUNNING`/`IDLE`/`SLEEPING`, ni machine, ni remote.  Il expose uniquement `FAILSAFE`.  Chaque environnement fournit sa propre réaction via la chaîne `kRunlevelProcs[]` :

```text
machine/config/combus/processors/runlevel/
  runlevel_procs.h  // kRunlevelProcs[] — failsafe + remote_link + runlevel
```

Le processor `cb_runlevel_fn` (générique "set runlevel") est câblé sur `DigitalComBusID::FAILSAFE` avec la config `kFailsafeRunlevelCfg` :

```cpp
static constexpr CbRunlevelCfg kFailsafeRunlevelCfg {
    .high  = RunLevel::FAILSAFE,    // FAILSAFE=true → write FAILSAFE (valeur dédiée)
    .low   = std::nullopt,          // FAILSAFE=false → no write
    .claim = true,                  // Block KEY_ACTIVE proc on link loss
};
```

Le `case RunLevel::FAILSAFE` dans le `switch(curRunLevel)` de `main.cpp` exécute `stopAllDcDrivers` / `sleepAllDcDrivers` / `disableAllDcDrivers` sur `isNewRunLevel`.

Découplage strict :

```text
core/system/failsafe  → détecte et publie FAILSAFE
machine/.../runlevel  → consomme FAILSAFE, force RunLevel::FAILSAFE
main.cpp              → réagit à RunLevel::FAILSAFE (sécurité hardware)
```

---

## 10. Règles à conserver

**Core failsafe** : toujours présent, indépendant des RunLevels/machines/sources métier, propriétaire du `FAILSAFE` global, assemble les contributions compile-time.

**Modules contributeurs** : propriétaires de leur logique métier, leur processor failsafe, leur canal `FAILSAFE_X` ; réarment leur `FAILSAFE_X` à chaque cycle.

**Chaîne failsafe** : reset en tête, exécute tous les processors, premier fault verrouille `FAILSAFE` pour le cycle, processors suivants bypassent la logique métier mais forcent leur `FAILSAFE_X` à `FAULT`.

**Ordre d'exécution** : toutes les sources (`input_update`, `vbat_update`, ...) doivent avoir tourné avant `failsafe_update()`, à chaque cycle, sans exception.  Cet invariant est garanti par `sys_manager_update()`.

**Environnements** : ne modifient pas la logique centrale, consomment `FAILSAFE`, définissent leur propre réaction.

---

## 11. Vue d'ensemble

```text
                    CORE / SYSTEM
              ┌─────────────────────┐
              │      FAILSAFE       │
              │ FAILSAFE = false    │
              │ chaîne centrale     │
              └──────────┬──────────┘
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
        vbat          autre           autre
          │ réarme       │ réarme       │ réarme
          ▼              ▼              ▼
     FAILSAFE_VBAT  FAILSAFE_X       FAILSAFE_Y
          └──────────────┼──────────────┘
                         ▼
                 chaîne failsafe → FAILSAFE global
                         │
                         ▼
              kRunlevelProcs[] (machine)
                         │
                         ▼
              RunLevel::FAILSAFE
                         │
                         ▼
              main.cpp → stop/sleep/disable drivers
```

**Ligne de conduite** : le failsafe central agrège des preuves de santé fournies par les modules, les consomme à chaque cycle, les remet à `FAULT`, et publie un unique `FAILSAFE` global.  Les modules restent propriétaires de leur logique et de leurs processors.  Les environnements restent propriétaires de la réaction au failsafe.

---

## 12. Lien avec `REMOTE_LINK_LOST` (signal séparé)

`REMOTE_LINK_LOST` est un signal **distinct** du `FAILSAFE` global.  Il est géré par sa propre chaîne et déclenche une réaction différente (`RunLevel::IDLE` au lieu de `RunLevel::FAILSAFE`).

**Voir** : `src/core/system/inputs/remote_link_lost.md` pour le détail de l'architecture, des contributeurs et de la réaction.

**Résumé** :

| Signal | Sémantique | Réaction |
|--------|------------|----------|
| `FAILSAFE` | Vraie faute (VBAT basse, etc.) | `RunLevel::FAILSAFE` (sécurité hardware complète) |
| `REMOTE_LINK_LOST` | Silence du lien remote (déconnexion, perte signal) | `RunLevel::IDLE` (mise en attente, reprise automatique) |

**Priorité** : `failsafe > remote_link > runlevel` — une vraie faute (`FAILSAFE`) l'emporte toujours sur une simple perte de lien.

---

## 13. Fichiers du module

```text
src/core/system/failsafe/
├── failsafe.h           // API publique (failsafe_update)
├── failsafe.cpp         // Implémentation orchestrateur
├── failsafe_chain.h     // Déclarations kFailsafeChain[]
├── failsafe_chain.cpp   // Définition kFailsafeChain[] (reset + contributeurs)
└── failsafe.cb          // Déclaration canal ComBus FAILSAFE
```

**Contributeurs actifs** :

```text
src/core/system/vbat/
└── vbat_failsafe.cb     // Canal FAILSAFE_VBAT (réarmé par vbat_update)
```

**Chaîne REMOTE_LINK_LOST** (séparée) :

```text
src/core/system/inputs/
├── remote_link_lost.cb           // Canal REMOTE_LINK_LOST (agrégat)
├── ps4_ds4_bt_link_lost.cb       // Canal PS4_DS4_BT_LINK_LOST (contributeur)
├── uart_link_lost.cb             // Canal UART_LINK_LOST (contributeur)
├── remote_link_fallback_chain.h  // Déclarations kRemoteLinkFallbackChain[]
└── remote_link_fallback_chain.cpp // Chaîne d'agrégation (reset + OR-guard)
```

---

## 14. Référence rapide

| Concept | Implémentation |
|---------|----------------|
| Canal `FAILSAFE` | `src/core/system/failsafe/failsafe.cb` |
| Chaîne `kFailsafeChain[]` | `src/core/system/failsafe/failsafe_chain.cpp` |
| Processor `cb_or_fn` | `src/core/system/combus/processors/logic/cb_or.h` |
| Réarmement `FAILSAFE_VBAT` | `src/core/system/vbat/vbat.cpp` (`vbat_update()`) |
| Orchestration | `src/machines/system/sys_manager.cpp` (`sys_manager_update()`) |
| Réaction runlevel | `src/machines/config/machines/volvo_A60H_bruder/combus/processors/runlevel/runlevel_procs.h` |
| Réaction hardware | `src/machines/main.cpp` (`case RunLevel::FAILSAFE`) |
| Lecture état | `comBus.digitalBus[DigitalComBusID::FAILSAFE].value` |

**Note** : `runlevel_procs.h` est actuellement situé dans la config machine (`volvo_A60H_bruder/...`).  Une migration vers le core est prévue lors d'un rework futur des processors combus, pour harmoniser l'organisation des chaînes de processors.
