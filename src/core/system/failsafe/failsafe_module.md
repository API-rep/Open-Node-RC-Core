# Failsafe Module — Architecture & Roadmap

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
- initialiser les sous-ComBus nécessaires ;
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
│   ├── failsafe.*
│   ├── failsafe_channels.inc
│   └── failsafe_processors.inc
│
└── vbatSense/
    ├── ...
    ├── failsafe_channels.inc
    ├── failsafe_processors.inc
    └── proc_vbat_failsafe.*
```

Le module central agrège les contributions conditionnellement selon les flags du module :

```cpp
#if defined(HAS_VBAT_SENSE)
#include "vbatSense/failsafe_channels.inc"
#endif
```

et :

```cpp
#if defined(HAS_VBAT_SENSE)
#include "vbatSense/failsafe_processors.inc"
#endif
```

Le processor et la logique métier restent dans leur module d'origine. Le module `failsafe` central ne contient pas de logique spécifique VBAT, ComBus, température, etc.

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
processor FAILSAFE_X
    │
    ▼
FAILSAFE
```

Le module source connaît son propre état. Le module failsafe central agrège tous les états.

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

`FAILSAFE_X` représente à la fois un état de santé et une preuve de vie. Pas besoin de TTL ni de timestamp.

---

## 6. Contrat d'un processor failsafe

```text
Entrées : FAILSAFE (global) + FAILSAFE_X (levier/état)
Sorties : FAILSAFE (global, éventuellement activé) + FAILSAFE_X (toujours forcé à fault)

1. FAILSAFE global déjà true ?
   oui → skip logique métier, force FAILSAFE_X = fault
   non → vérifie FAILSAFE_X, évalue le levier, active FAILSAFE si nécessaire, force FAILSAFE_X = fault
```

Pas d'early exit global de la chaîne — tous les processors tournent à chaque cycle pour que tous les `FAILSAFE_X` soient consommés et remis à `FAULT`. Une fois `FAILSAFE` activé, les processors suivants bypassent leur logique métier mais exécutent toujours leur maintenance (`force FAILSAFE_X = fault`). Comportement : **guarded execution** / **local short-circuit**.

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

Au démarrage, une première passe peut avoir lieu avant que tous les modules aient produit leur état normal. Pas besoin d'état `INITIALIZING` explicite — le fonctionnement normal se met en place après cette passe à vide, à condition que l'orchestrateur garantisse l'ordre (voir §12.17, point 10).

---

## 9. Réaction au `FAILSAFE`

Le module central ne connaît ni `RUNNING`/`IDLE`/`SLEEPING`, ni machine, ni remote. Il expose uniquement `FAILSAFE`. Chaque environnement fournit son propre processor de réaction :

```text
env/config/failsafe/
  machine/failsafe/proc_failsafe_reaction.*
  remote/failsafe/proc_failsafe_reaction.*
```

Découplage strict :

```text
core/system/failsafe  → détecte et publie
env/config/failsafe   → décide de la réaction
```

---

## 10. Règles à conserver

**Core failsafe** : toujours présent, indépendant des RunLevels/machines/sources métier, propriétaire du `FAILSAFE` global, assemble les contributions compile-time.

**Modules contributeurs** : propriétaires de leur logique métier, leur processor failsafe, leurs `.inc` ; réarment leur `FAILSAFE_X` à chaque cycle.

**Chaîne failsafe** : reset en tête, exécute tous les processors, premier fault verrouille `FAILSAFE` pour le cycle, processors suivants bypassent la logique métier mais forcent leur `FAILSAFE_X` à `FAULT`.

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
       vbatSense      combus link     autre
          │ réarme       │ réarme       │ réarme
          ▼              ▼              ▼
     FAILSAFE_VBAT  FAILSAFE_X       FAILSAFE_Y
          └──────────────┼──────────────┘
                         ▼
                 chaîne failsafe → FAILSAFE global
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
       machine         remote         autre
          ▼              ▼              ▼
      réaction        réaction        réaction
```

**Ligne de conduite** : le failsafe central agrège des preuves de santé fournies par les modules, les consomme à chaque cycle, les remet à `FAULT`, et publie un unique `FAILSAFE` global. Les modules restent propriétaires de leur logique et de leurs processors. Les environnements restent propriétaires de la réaction au failsafe.

---

## 12. Roadmap d'implémentation

> Chaque étape doit laisser le projet compilable. La numérotation reflète l'ordre d'attaque recommandé, pas un ordre obligatoire d'exécution runtime.

### 12.0 — État du code au moment de l'écriture de cette roadmap

- Un seul failsafe existait : `sys_manager_update()` calculait `failsafeActive = !bus.isDrived`, réaction inline dans `main.cpp`.
- Pas de `FAILSAFE` ComBus global à l'époque ; seul `COMBUS_FLAG_FAILSAFE` (bit trame) existait, consommé par le sound node.
- Pas de `FAILSAFE_X`, pas de chaîne failsafe ; seul pivot de santé = `comBus.batteryIsLow`.
- Ordre d'exécution : failsafe évalué **avant** les chaînes de processors, avec `return` early qui sautait `inputChain`/`simChain`/RunLevel — contraire à l'invariant requis (toutes les sources doivent avoir tourné avant `failsafe_update()`).
- Règle de premier cycle : état initial des `FAILSAFE_X` = `false` (fault), volontairement — pas de grace period artificielle.

### 12.1-12.3 — Squelette, reset, façade de lecture

**✅ Validées** (voir journal §12.18) : `failsafe.h/.cpp`, `failsafe_channels.inc`, `failsafe_processors.inc`, `proc_failsafe_reset`, `failsafe_access.h` (`failsafe_is_active()`) sont en place et compilent.

### 12.4 — Dépréciation de `failsafeActive` dans `SysResult`  ✅ CLOS (FS2)

**Statut** : clos en FS2.  Le accessor `SysResult::failsafeActive` était déjà supprimé en A16.5 ; FS2 a finalisé le retrait de toutes les occurrences actives :
- Variable locale `failsafeActive` dans `main.cpp` : supprime (bloc ad-hoc retiré).
- `s_failsafeWasActive` : supprimé (`isNewRunLevel` du `switch(curRunLevel)` fait le travail).
- Paramètre `failsafeActive` de `output_update()` / `combus_tx_update()` : supprimé.
- Paramètre `failSafe` de `combus_frame_encode()` : supprimé.
- Bit hors-bande `COMBUS_FLAG_FAILSAFE` : supprimé (cf. §12.13, `FAILSAFE` est un canal ComBus LOCAL `both_or`).

**Remplacé par** : la chaîne `kRunlevelProcs[]` (proc `failsafe` câblé sur `DigitalComBusID::FAILSAFE`) écrit `RunLevel::FAILSAFE` (= 6, valeur dédiée ajoutée à l'enum), qui déclenche un `case RunLevel::FAILSAFE` dans le `switch(curRunLevel)` de `main.cpp` exécutant le `stopAllDcDrivers` / `sleepAllDcDrivers` / `disableAllDcDrivers` sur `isNewRunLevel`.

**Règle de migration historique** : l'ancien chemin de réaction ne peut être supprimé qu'après validation de la nouvelle chaîne de réaction environnementale.  Cette condition est remplie — la nouvelle chaîne est active et validée sur `volvo_A60H_bruder` (compile SUCCESS, 36.3% Flash, -8 bytes vs avant).

### 12.5 — Dépréciation de `bus.isDrived`  ✅ CLOS (chantier 12.5 final, 2026‑09‑25)

**Décision actée** : `isDrived` est une dette de l'ancien embryon failsafe. À supprimer au profit du combus failsafe dédié aux inputs. Les modules reprendront cette charge à leur rework, une fois le chantier failsafe terminé.

**Statut** : clos lors du chantier 12.5 final (2026‑09‑25).  Voir §12.6 pour le détail des suppressions code et §12.18 (entrée "chantier 12.5 final") pour la validation build.

### 12.6 — Nettoyage FS1 legacy (`bus.isDrived` / `bus.isNotDrived`)  ✅ CLOS (chantier 12.6, 2026‑09‑25)

Une fois le §12.5 validé, le flag open‑drain historique n'a plus de raison d'exister. Ce chantier de cleanup le supprime partout dans la base de code, sans changer le comportement fonctionnel (la sémantique est déjà portée par `REMOTE_LINK_LOST`).

**Actions réalisées** :

| Fichier | Avant | Après |
|---|---|---|
| `src/core/system/combus/combus_defs.h` | champ `bool isNotDrived = true;` dans `ComBus` | champ supprimé ; commentaire de section remplacé par une note explicative |
| `src/machines/system/sys_manager.cpp` | `bus.isNotDrived = true;` au début de `sys_manager_update()` + corps de `sys_manager_reset(ComBus&)` | pré‑clear supprimé ; `sys_manager_reset()` réduit à un stub no‑op (commenté) |
| `src/machines/system/input/input_update.cpp` | `bus.isNotDrived = false;` en fin de fonction (commentaire "legacy FS1") | ligne supprimée ; commentaire remplacé par la référence à `PS4_DS4_BT_LINK_LOST` |
| `src/core/system/combus/protocol/frame/combus_frame.cpp` | `combus->isNotDrived = false;` en fin de `combus_frame_apply()` (commentaire "legacy FS1") | ligne supprimée ; codec reste 100 % pur |
| `src/machines/system/debug/dashboard_*.cpp` (3 fichiers) | lecture directe de `bus.isNotDrived` pour afficher l'état "driven" | remplacée par lecture de `REMOTE_LINK_LOST` (avec fallback `lastFrameMs` proxy sur builds autonomes sans `HAS_REMOTE_LINK_LOST_FALLBACK`) |
| `src/machines/init/init.cpp` (pause bloc) | condition de sortie sur `!comBus.isNotDrived && KEY` | remplacée par `!REMOTE_LINK_LOST && KEY` (avec même fallback) |
| `src/core/system/combus/combus_manager.h` | commentaire obsolète sur `isDrived` | remplacé par référence au §12.5 / §12.6 |
| `src/core/system/combus/processors/proc_chain.cpp` | note "they do not set bus.isDrived" | reformulée pour refléter le nouveau design par contributeurs |
| `src/machines/system/sys_manager.h` | doc détaillée de l'invariant open‑drain | remplacée par la description de la nouvelle chaîne (input → vbat → failsafe → link‑fallback) |
| `src/machines/main.cpp` | commentaire "isDrived always true when RUNNING is reached" | remplacé par la note chantier 12.6 |

**Test suite** : `test/test_combus_loopback/test_combus_loopback.cpp` — le groupe **Group F** (FAILSAFE_COMBUS_LINK re‑arm) a été supprimé (canal + champ n'existent plus) ; les autres groupes (A codec, B loopback, C view identity, D wire‑end, E MD5, G RUNLEVEL) restent valides.

**Garanties préservées** :

* Le codec `combus_frame.cpp` n'écrit plus aucun état de lien — il est strictement "decode + apply".
* Les dashboards affichent toujours un indicateur `DRV / ---` lisible, dérivé de `REMOTE_LINK_LOST` quand la chaîne est active, sinon d'un proxy `lastFrameMs` (compatible builds autonomes sans contributeur de lien).
* Le bloc `PAUSE_LOG_AFTER_INIT` sort toujours sur "remote KEY pressé ET bus driven" — la condition est juste dérivée d'un autre signal (sémantiquement équivalent : on n'a plus de driver, donc pas de drive).
* `sys_manager_reset(ComBus&)` est conservé en stub pour la compatibilité source avec les callers out‑of‑tree (sound node historique, etc.).

### 12.7 — Câblage VBAT

Processor `proc_failsafe_vbat` (pattern reset-first, cf. §6), réarmé par `vbat_update()`. Validation attendue : batterie faible → `failsafe_is_active() == true` ; test de staleness (empêcher le réarmement, vérifier le passage à `true` au cycle suivant sans modifier la logique du processor).

**Statut réel non vérifié** — à confirmer sur le code actuel avant de supposer cette étape close.


### 12.8 — Enregistrement runtime de la chaîne

**Pas encore fait.** Objectif : appeler `failsafe_update()` après toutes les sources et avant toute réaction.

Ordre cible :

```text
loop()
  sys_manager_update()
  proc_chain_update(machine.inputChain)
  proc_chain_update(machine.simChain)
  vbat_update()
  autres sources / contributeurs
  failsafe_update()
  réaction (temporairement l'ancien chemin, pendant transition)
  FSM RunLevel
  output_update(comBus)
```

Invariant : toutes les mises à jour des modules sources doivent être terminées avant l'exécution de la chaîne failsafe, à chaque cycle, sans exception.

### 12.9 — Réaction au failsafe côté environnement

**Décision actée** : un processor combus en fin de chaîne, type "set runlevel" — pas un `RUNLEVEL` figé générique pour toutes les machines. Le mapping ("quel runlevel adopter en cas de failsafe", ex. `IDLE`) est défini dans la config de la machine, cohérent avec le fait que les runlevels eux-mêmes sont définis côté `main`/machine.

> **Réalisable directement** : `RUNLEVEL` est désormais un canal combus standard (chantier `RL1`-`RL5`, clos) — ce processor peut écrire dessus via l'accesseur générique (`combus_set_analog(..., AnalogComBusID::RUNLEVEL, ...)`), sans mécanisme dédié à inventer.

Convention : `proc_failsafe_reaction` (processor) + `failsafe_reaction_update()` (façade exécutant la chaîne).

Ordre runtime définitif :

```text
sources → failsafe_update() → failsafe_reaction_update() → FSM RunLevel → output
```

La réaction intervient donc avant la FSM du même cycle.

### 12.10 — Réaction côté remote

**Hors scope, reporté.** Structure `src/remotes/config/failsafe/` à préparer plus tard, quand l'architecture remote sera outillée.

### 12.11 — Ownership et fusion (SYSTEM/LOCAL, `*_OR`)

**Décision actée** : le layering REMOTE/LOCAL/SYSTEM reste le bon modèle — un mécanisme d'ownership dédié a déjà été tenté par le passé sans succès. Ajout d'une nuance sur le paramètre `direction` des combus : **`both_or`**, qui indique qu'à la fusion d'un import, une valeur `true` de n'importe quel côté suffit à donner `true` (similaire au `cb_or_fn` du module failsafe, mais appliqué à la fusion d'imports en général).

### 12.12 — Suppression de l'ancien chemin

**Décision actée**, conditionnée à `12.5` : oui, avec adaptation future des modules d'input au nouveau mécanisme failsafe — pas immédiat, pas avant que la nouvelle chaîne de réaction soit validée.

Précondition dure : la nouvelle chaîne de réaction environnementale doit être active et validée avant toute suppression de `SysResult.failsafeActive` ou de l'ancien chemin de réaction.

> **Chantier 12.6 (cleanup, 2026‑09‑25)** : la précondition est désormais remplie.  `isDrived` / `isNotDrived` ont été retirés complètement de la base de code (voir §12.6).  Le nouvel invariant est : "chaque contributeur `*_LINK_LOST` est indépendant et réarmé par son propre backend — pas de flag partagé, pas d'ordonnancement à respecter pour l'état de lien".

### 12.13 — Publication TX de l'état failsafe

**Résolue par `12.11`** : `FAILSAFE` sera `LOCAL`, direction `both_or` — pas besoin d'un bit `SYSTEM` séparé transporté hors-bande (`COMBUS_FLAG_FAILSAFE`). La fusion se fait nativement via le mécanisme `both_or` du canal `LOCAL`, ce qui simplifie l'architecture décrite en §11 (plus besoin de combiner explicitement "local OR reçu" côté sound node — le canal `both_or` le fait par construction).

---

## 12.14 — Fichiers impactés (consolidé)

**Création** : `src/core/system/failsafe/*` ; `vbat/failsafe_channels.inc`, `vbat/failsafe_processors.inc`, `vbat/proc_failsafe_vbat.*` ; `combus/failsafe_channels.inc`, `combus/failsafe_processors.inc`, ; `machines/config/failsafe/*` ; `remotes/config/failsafe/*` (reporté).

**Modification** : `machines/main.cpp`, `machines/init/init.cpp`, `machines/system/sys_manager.{h,cpp}`, `output/output_manager.*` (si nécessaire), `combus/protocol/combus_tx.cpp` (obsolète depuis `12.13`), `combus/frame/combus_frame.h` (obsolète depuis `12.13`), `vbat/vbat.cpp`, `sound_module/system/combus_sound_interpreter.cpp`.

**Suppression/déplacement** : aucun fichier à ce stade — l'ancien mécanisme n'est retiré qu'après validation complète du nouveau chemin (`12.12`).

---

## 12.15 — Ordre d'implémentation

1. Squelette `failsafe`. ✅
2. Processor de reset. ✅
3. `failsafe_access.h`. ✅
4. Dépréciation de `failsafeActive`, ancien comportement intact.
5. `proc_failsafe_input_link` / dépréciation `isDrived` → **couvert par `FS1`**, à vérifier (voir `12.5`).
6. VBAT contributor + test de staleness — statut réel à vérifier.
8. Branchement runtime de `failsafe_update()`.
9. Réaction via `env/config/failsafe/` (processor "set runlevel").
10. **Validation obligatoire de la détection et de la réaction ensemble.**
11. Squelette remote (reporté).
12. SYSTEM/LOCAL et `both_or` → **`both_or` fait côté combus-builder (`ND6`)**.
13. Suppression de l'ancien chemin.
14. Publication TX → **devenue obsolète par `12.13`**, plus besoin de bit SYSTEM dédié.

Règle de transition : l'ancien chemin de réaction ne peut être supprimé qu'après validation de la chaîne de réaction environnementale. À aucun moment une source de fault active ne doit pouvoir produire un `FAILSAFE` sans réaction associée.

À chaque étape applicable :

```text
pio run -e volvo_A60H_bruder
pio run -e remotes
pio run -e sound_node_volvo
pio test -e test_combus_loopback
```

---

## 12.16 — Points encore ouverts

1. `failsafe_init(ComBus&)` est-il réellement nécessaire au core, ou seulement à certains processors ?
2. Quel doit être le périmètre final de `sys_manager_update()` après refonte ?
3. Quel mécanisme de test de staleness s'intègre le mieux à l'infrastructure existante ?
4. Où et comment le mapping machine "runlevel failsafe" (`12.9`) est-il défini concrètement — nouveau champ de config machine à créer ?
5. Quelle réaction concrète doit appliquer le remote (`12.10`, reporté) ?

---

## 12.17 — Risques de régression

1. Réordonnancement du `loop()` : ordre définitif = `sources → failsafe_update() → failsafe_reaction_update() → FSM → output`.
2. Ne pas supprimer l'ancien chemin avant validation de la nouvelle réaction.
3. Initialiser le failsafe correctement sans appeler prématurément son `update()`.
4. `simChain` tourne désormais toujours, y compris pendant un fault — vérifier que les sorties dangereuses restent bloquées.
5. Layers ComBus : si les pivots deviennent des canaux, expliciter les layers (largement traité par le chantier combus-handshake ce soir).
6. Tests loopback : éviter les dépendances involontaires dans les filtres de build.
7. Sound node : combiner SYSTEM reçu et LOCAL — simplifié par `12.13`/`both_or`.
8. Toute nouvelle chaîne de sources doit finir avant `failsafe_update()`.
9. Chaque contributeur doit avoir un test où son propriétaire cesse de réarmer son `FAILSAFE_X`.
10. Premier cycle : pas de grace period ; la sûreté repose sur l'ordre d'exécution.

---

## 12.18 — Journal de validation

### Étape 3 — Façade `failsafe_access.h` (validation, sans modification)

**Date** : 8/12/2026 · **Branche** : `failsafe-module` · **Commit parent** : `3d6e62d`

Conclusion : `failsafe_access.h` créé à l'étape 1 satisfait déjà le contrat (`inline bool failsafe_is_active() { return failsafeBus.active; }`). Aucune modification nécessaire.

Vérifications : compilation `volvo_A60H_bruder` SUCCESS (1 139 029 octets Flash, 45 748 octets RAM) ; smoke-test temporaire compilé sans erreur, supprimé après validation ; aucune dépendance vers `machines/` ; pas de header lourd non justifié. Diff vs étape 2 : 0 octet, 0 fichier modifié.

### À faire — nouvelle entrée à ajouter après audit croisé avec `FS1`

Une entrée de journal reste à écrire une fois confirmé si `12.5`/`12.7` sont réellement closes par `FS1`, ou s'il reste un travail de câblage (voir §12.16 point 1 et le prompt d'audit en cours).

### Chantier 12.5 final — `REMOTE_LINK_LOST` aggregator + retrait `FAILSAFE_COMBUS_LINK`  ✅ CLOS (2026‑09‑25)

**Date** : 25/09/2026 · **Branche** : `failsafe-module` · **Scope** : §12.5 + §12.7

**Conclusion** : le chantier 12.5 final est clos.  La nouvelle chaîne `REMOTE_LINK_LOST` est active et validée.  Le contributeur historique `FAILSAFE_COMBUS_LINK` a été retiré (la sémantique est portée directement par les contributeurs `*_LINK_LOST` — un pour chaque backend de transport).  Le codec `combus_frame.cpp` reste 100 % pur (decode + apply uniquement, sans toucher à l'état de lien).

**Fichiers livrés** :
- 3 canaux combus : `src/core/system/inputs/{remote_link_lost,ps4_ds4_bt_link_lost,uart_link_lost}.cb`
- Chaîne d'agrégation : `src/core/system/inputs/remote_link_fallback_chain.{h,cpp}` (reset + OR‑guard, gérée par `sys_manager_update()` via le guard `HAS_REMOTE_LINK_LOST_FALLBACK`)
- Processor `cb_runlevel_fn` (runlevel.cb) câblé pour forcer `RUNLEVEL = IDLE` sur front montant de `REMOTE_LINK_LOST`
- Écritures migrées : `ps4_ds4_bt.cpp` publie `PS4_DS4_BT_LINK_LOST = false` quand le contrôleur est actif, `combus_sound_interpreter.cpp` publie `UART_LINK_LOST` selon le RX‑timeout
- Dashboards : indicateurs `DRV / ---` dérivés de `REMOTE_LINK_LOST` (avec fallback `lastFrameMs` proxy)
- Build : SUCCESS (les deux builds cibles `volvo_A60H_bruder` et `remotes`)

**Rétention `FAILSAFE_COMBUS_LINK`** : NON — le canal a été retiré.  Le rôle d'agrégateur de "lien perdu" est joué directement par `REMOTE_LINK_LOST` (qui est lui-même un contributeur du `FAILSAFE` global via la chaîne de processors).

### Chantier 12.6 — Nettoyage FS1 legacy (`bus.isDrived` / `bus.isNotDrived`)  ✅ CLOS (2026‑09‑25)

**Date** : 25/09/2026 · **Branche** : `failsafe-module` · **Scope** : §12.6

**Conclusion** : le flag open-drain historique `bus.isDrived` / `bus.isNotDrived` a été complètement retiré de la base de code.  Le comportement fonctionnel est préservé (la sémantique est portée par `REMOTE_LINK_LOST`).  Voir §12.6 pour le détail des 10 fichiers modifiés et la table de correspondance avant/après.

**Build** : SUCCESS (les trois builds cibles `volvo_A60H_bruder`, `remotes`, `sound_node_volvo`).
**Test suite** : `pio test -e test_combus_loopback` — Group F (FAILSAFE_COMBUS_LINK re-arm) supprimé ; autres groupes valides.
