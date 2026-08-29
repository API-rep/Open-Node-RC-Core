# Failsafe Module --- Architecture

## 1. Architecture générale

Le failsafe est un module **inconditionnel** situé dans :

``` text
src/core/system/failsafe/
```

Il existe dans toutes les builds, sans `HAS_FAILSAFE`.

En version minimale, il contient :

-   le ComBus global `FAILSAFE` ;
-   la chaîne de processors failsafe ;
-   un processor initial de reset ;
-   aucune source de failsafe optionnelle.

Une chaîne vide, hors reset, est donc valide.

------------------------------------------------------------------------

## 2. Responsabilité du module `core/system/failsafe`

Le module central est responsable de :

-   déclarer le ComBus global `FAILSAFE` ;
-   agréger les sous-ComBus `FAILSAFE_X` ;
-   initialiser les sous-ComBus nécessaires ;
-   construire la chaîne principale de processing ;
-   remettre `FAILSAFE` à `false` au début de chaque cycle ;
-   intégrer conditionnellement les contributions des autres modules ;
-   ne connaître ni les RunLevels, ni les machines, ni les remotes.

Le failsafe produit uniquement :

``` text
FAILSAFE = false → système sans fault détecté
FAILSAFE = true  → au moins un fault détecté
```

Il ne décide pas directement quoi faire de cet état.

------------------------------------------------------------------------

## 3. Organisation des modules contributeurs

Chaque module reste propriétaire de sa logique failsafe.

Exemple :

``` text
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

Le module central agrège les contributions conditionnellement selon les
flags du module :

``` cpp
#if defined(HAS_VBAT_SENSE)
#include "vbatSense/failsafe_channels.inc"
#endif
```

et :

``` cpp
#if defined(HAS_VBAT_SENSE)
#include "vbatSense/failsafe_processors.inc"
#endif
```

Le processor et la logique métier restent dans leur module d'origine.

Le module `failsafe` central ne contient pas de logique spécifique VBAT,
ComBus, température, etc.

------------------------------------------------------------------------

## 4. ComBus global et sous-ComBus

Chaque source possède son propre sous-ComBus :

``` text
FAILSAFE
FAILSAFE_VBAT
FAILSAFE_COMBUS
FAILSAFE_XXX
```

La relation est :

``` text
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

Le module source connaît son propre état.

Le module failsafe central agrège tous les états.

------------------------------------------------------------------------

## 5. Cycle de vie des `FAILSAFE_X`

Principe :

> Chaque `FAILSAFE_X` doit être réarmé à chaque cycle par son module
> propriétaire.

Puis son processor failsafe le remet systématiquement à `FAULT`.

Exemple :

``` text
cycle N

module VBAT
    ↓
FAILSAFE_VBAT = healthy

chaîne failsafe
    ↓
processor VBAT
    ├── lit FAILSAFE_VBAT
    ├── évalue son levier
    └── force FAILSAFE_VBAT = fault
```

Au cycle suivant :

``` text
module VBAT tourne
    → réarme FAILSAFE_VBAT

module VBAT ne tourne plus
    → FAILSAFE_VBAT reste fault
```

Ainsi, `FAILSAFE_X` représente à la fois :

-   un état de santé ;
-   une preuve de vie.

Il n'est pas nécessaire d'ajouter un système de TTL ou de timestamp.

------------------------------------------------------------------------

## 6. Contrat d'un processor failsafe

Un processor dispose conceptuellement de :

``` text
Entrées
├── ComBus principal de la chaîne : FAILSAFE
└── ComBus levier / état : FAILSAFE_X + valeur métier à évaluer

Sorties
├── FAILSAFE global
└── FAILSAFE_X traité
```

Déroulement :

``` text
1. Le FAILSAFE global est déjà true ?
   │
   ├── oui
   │   ├── skip de la logique métier
   │   └── force FAILSAFE_X à fault
   │
   └── non
       ├── vérifie FAILSAFE_X
       ├── évalue son levier
       ├── active FAILSAFE si nécessaire
       └── force FAILSAFE_X à fault
```

Il n'y a pas d'early exit global de la chaîne.

Tous les processors sont exécutés à chaque cycle afin que tous les
`FAILSAFE_X` soient consommés et remis à `FAULT`.

Dès qu'un processor a activé le `FAILSAFE` global, les processors
suivants ne font plus leur traitement métier, mais exécutent toujours
leur logique de maintenance :

``` text
force FAILSAFE_X = fault
```

Le comportement peut être décrit comme une **guarded execution** ou un
**local short-circuit**.

------------------------------------------------------------------------

## 7. Chaîne principale

La chaîne principale devient :

``` text
[ RESET FAILSAFE ]
        │
        ▼
[ FAILSAFE VBAT ]
        │
        ▼
[ FAILSAFE COMBUS ]
        │
        ▼
[ FAILSAFE XXX ]
```

Le premier processor fait systématiquement :

``` text
FAILSAFE = false
```

Ensuite :

``` text
aucun fault
    → FAILSAFE reste false

premier fault
    → FAILSAFE = true

processors suivants
    → skip logique métier
    → forcent leur FAILSAFE_X à fault
```

Le `FAILSAFE` global est donc une forme de latch limitée au cycle :

``` text
début de cycle : false
pendant le cycle : peut passer à true
fin de cycle : reste dans son état
cycle suivant : reset à false
```

------------------------------------------------------------------------

## 8. Initialisation et passe à vide

Au démarrage, une première passe peut avoir lieu avant que tous les
modules aient produit leur état normal.

Le comportement est donc :

``` text
boot
    ↓
initialisation
    ↓
première passe de traitement
    ↓
modules commencent à réarmer leurs FAILSAFE_X
    ↓
chaîne failsafe les consomme
```

Il n'est pas nécessaire d'introduire un état spécifique `INITIALIZING`.

Le fonctionnement normal se met en place après cette passe à vide.

------------------------------------------------------------------------

## 9. Réaction au `FAILSAFE`

Le module central ne contient aucune politique de réaction.

Il ne connaît pas :

``` text
RUNNING
IDLE
SLEEPING
machine
remote
```

Il expose uniquement :

``` text
FAILSAFE
```

Chaque environnement peut ensuite fournir son propre processor de
réaction.

Organisation proposée :

``` text
env/config/failsafe/
```

avec une structure adaptée à l'environnement :

``` text
machine
└── failsafe/
    └── proc_failsafe_reaction.*

remote
└── failsafe/
    └── proc_failsafe_reaction.*
```

Ces processors utilisent `FAILSAFE` comme levier et décident localement
quoi faire.

Exemple :

``` text
machine
    FAILSAFE → IDLE

remote
    FAILSAFE → neutral state

autre environnement
    FAILSAFE → comportement spécifique
```

Le découplage est strict :

``` text
core/system/failsafe
    → détecte et publie un arrêt d'urgence

env/config/failsafe
    → décide de la réaction
```

------------------------------------------------------------------------

## 10. Règles à conserver

### Le core failsafe

``` text
- toujours présent
- indépendant des RunLevels
- indépendant des machines
- indépendant des sources métier
- propriétaire du FAILSAFE global
- assemble les contributions compile-time
```

### Les modules contributeurs

``` text
- propriétaires de leur logique métier
- propriétaires de leur processor failsafe
- propriétaires de leurs fragments .inc
- réarment leur FAILSAFE_X à chaque cycle
```

### La chaîne failsafe

``` text
- reset FAILSAFE au début
- exécute tous les processors
- premier fault verrouille FAILSAFE pour le cycle
- processors suivants bypassent leur logique métier
- tous les FAILSAFE_X sont forcés à FAULT
```

### Les environnements

``` text
- ne modifient pas la logique centrale
- consomment FAILSAFE
- définissent leur propre réaction
```

------------------------------------------------------------------------

## 11. Vue d'ensemble

``` text
                    CORE / SYSTEM

              ┌─────────────────────┐
              │      FAILSAFE       │
              │                     │
              │ FAILSAFE = false    │
              │ chaîne centrale     │
              └──────────┬──────────┘
                         │
          ┌──────────────┼──────────────┐
          │              │              │
          ▼              ▼              ▼

       vbatSense      combus          autre
          │              │              │
Une chaîne vide, hors reset, est donc valide.
          │ réarme       │ réarme       │ réarme
          ▼              ▼              ▼
     FAILSAFE_VBAT  FAILSAFE_COMBUS FAILSAFE_X
          │              │              │
          └──────────────┼──────────────┘
                         ▼
                 chaîne failsafe
                         │
                         ▼
                  FAILSAFE global
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼

       machine         remote         autre
          │              │              │
          ▼              ▼              ▼
      réaction        réaction        réaction
```

## Ligne de conduite

Le failsafe central agrège des preuves de santé fournies par les
modules, les consomme à chaque cycle, les remet à `FAULT`, et publie un
unique `FAILSAFE` global.

Les modules restent propriétaires de leur logique et de leurs
processors. Les environnements restent propriétaires de la réaction au
failsafe.

---

---

# 12. Roadmap d'implémentation

> Document d'implémentation, pas une spec théorique. Chaque étape doit laisser le projet compilable. La numérotation reflète l'ordre d'attaque recommandé, pas un ordre obligatoire d'exécution runtime.

---

## 12.0 — État du code existant au moment de cette roadmap

Constats objectifs tirés du repository (commit courant) :

- **Un seul failsafe existe** : `sys_manager_update()` calcule `failsafeActive = !bus.isDrived` après le tick d'inputs. La réaction est entièrement inline dans `src/machines/main.cpp` (sleep/stop/disable, force IDLE + clear `KEY_ACTIVE`).
- **Pas de `FAILSAFE` ComBus global** : la seule notion de failsafe actuelle côté trame est le bit `COMBUS_FLAG_FAILSAFE`, consommé par le sound node.
- **Pas de `FAILSAFE_X` sous-ComBus** : le seul pivot de santé actuel est `comBus.batteryIsLow`, écrit par `vbat_update()`.
- **Pas de chaîne Failsafe** : seules `inputChain` et `simChain` existent.
- **VBAT** : sa logique de seuil vit dans `vbat_sense.cpp`. `vbat_update()` écrit `comBus.batteryIsLow` une fois par cycle.
- **`comBus`** est un `extern` global instancié côté machine et sound.
- **Ordre d'exécution actuel (machine)** :

```text
loop()
  ├─ sys_manager_update()      ← isDrived pre-clear, input_refresh, vbat_sense_tick, failsafeActive
  ├─ if (sys.failsafeActive)   ← return early (procs/RunLevel sautés)
  ├─ proc_chain_update(inputChain)
  ├─ proc_chain_update(simChain)
  ├─ FSM RunLevel
  ├─ sys.vbatChanged -> low-bat -> SLEEPING
  └─ output_update(comBus, false)
```

- **Invariant d'ordre actuel** : le failsafe est évalué avant les chaînes de processors. Si `failsafeActive`, le `return` saute `inputChain + simChain + RunLevel`. Cela diffère de la garantie requise par le WIP : toutes les mises à jour des modules sources doivent être terminées avant l'exécution de la chaîne Failsafe.
- **Règle de premier cycle** : l'état initial des futurs `FAILSAFE_X` est volontairement `false` (non réarmé / stale / fault). Il ne faut pas ajouter de période de grâce artificielle : l'orchestrateur doit garantir que `failsafe_update()` n'est appelé qu'après que les contributeurs attendus ont eu l'occasion d'exécuter leur cycle de mise à jour.

---

## 12.1 — Étape 1 : Squelette du module central `failsafe`

**Objectif** : créer `src/core/system/failsafe/` et y poser le minimum vital : ComBus global, chaîne vide et processor de reset, sans changer le comportement runtime.

**Fichiers à créer** :

- `src/core/system/failsafe/failsafe.h`
- `src/core/system/failsafe/failsafe.cpp`
- `src/core/system/failsafe/failsafe_channels.inc`
- `src/core/system/failsafe/failsafe_processors.inc`
- `src/core/system/failsafe/failsafe_chain.h`
- `src/core/system/failsafe/failsafe_chain.cpp`
- `src/core/system/failsafe/proc_failsafe_reset.h`
- `src/core/system/failsafe/proc_failsafe_reset.cpp`

**API cible** :

```cpp
struct FailsafeComBus {
    bool active = false;
};

extern FailsafeComBus failsafeBus;

void failsafe_init(ComBus& mainBus);
void failsafe_update();
```

`failsafe_channels.inc` est vide à cette étape. `failsafe_processors.inc` contient uniquement le processor de reset.

**Points de vigilance** :

- aucune dépendance vers `machines/` ;
- `failsafeBus.active` est le seul pivot publié ;
- aucune logique RunLevel / IDLE / SLEEP ;
- le cycle Failsafe actuel est séquentiel et exécuté dans le contexte principal du firmware.

**Validation** : `pio run -e volvo_A60H_bruder`.

---

## 12.2 — Étape 2 : Processor de reset `proc_failsafe_reset`

**Objectif** : implémenter le premier processor de la chaîne, qui fait systématiquement `failsafeBus.active = false` au début du cycle.

**Fichiers à créer** :

- `proc_failsafe_reset.h`
- `proc_failsafe_reset.cpp`

Le processor écrit `failsafeBus.active = false`, ne modifie pas `value` et ne pose jamais `claimed`.

**Points de vigilance** :

- le reset ne concerne que le FAILSAFE global ;
- les `FAILSAFE_X` restent sous le contrôle de leur cycle propriétaire : réarmement par le module, consommation par leur processor ;
- pas de court-circuit ici.

**Validation** : compilation OK et `failsafe_update()` exécutable en isolation.

---

## 12.3 — Étape 3 : Façade de lecture `failsafe_access.h`

**Objectif** : permettre aux autres modules de lire l'état sans dépendre directement de `failsafe.h`.

**Fichier à créer** :

- `src/core/system/failsafe/failsafe_access.h`

API :

```cpp
bool failsafe_is_active();
```

**Points de vigilance** :

- pas de setter public ;
- façade minimale ;
- pas de dépendance vers `machines/`, la chaîne ou l'implémentation complète.

**Validation** : header incluable depuis les modules `core/system`.

---

## 12.4 — Étape 4 : Déprécier `failsafeActive` dans `SysResult`

**Objectif** : préparer la transition sans supprimer l'ancien mécanisme.

**Fichiers à modifier** :

- `src/machines/system/sys_manager.h`
- `src/machines/system/sys_manager.cpp`

`failsafeActive` reste présent pendant la transition.

**Règle de migration** :

> L'ancien chemin de réaction ne peut être supprimé qu'après validation de la nouvelle chaîne de réaction environnementale. À aucun moment une source de fault active ne doit pouvoir produire un `FAILSAFE` sans réaction associée.

**Validation** : aucune régression runtime.

---

## 12.5 — Étape 5 : Processor `proc_failsafe_input_link`

**Objectif** : créer le premier contributeur concret qui traduit `bus.isDrived == false` en fault.

**Fichiers à créer** :

- `src/core/system/failsafe/proc_failsafe_input_link.h`
- `src/core/system/failsafe/proc_failsafe_input_link.cpp`

Déclarer `FAILSAFE_INPUT_LINK` dans `failsafe_channels.inc`.

### Pattern de référence d'un processor

```text
si FAILSAFE global déjà actif
    skip la logique métier
    force FAILSAFE_X = false

sinon
    si FAILSAFE_X == false
        FAILSAFE global = true
    sinon
        évaluer le levier du module
        si le levier indique une faute
            FAILSAFE global = true

    force FAILSAFE_X = false
```

Le non-réarmement d'un `FAILSAFE_X` est donc lui-même une cause de fault.

### Application au lien d'entrée

```text
si failsafeBus.active
    force FAILSAFE_INPUT_LINK = false
sinon
    si !FAILSAFE_INPUT_LINK
        failsafeBus.active = true
    sinon si !bus.isDrived
        failsafeBus.active = true

    force FAILSAFE_INPUT_LINK = false
```

**Points de vigilance** :

- pas d'early exit structurel ;
- la chaîne parcourt toujours tous les processors ;
- le court-circuit est local au processor ;
- la consommation du `FAILSAFE_X` est toujours exécutée.

**Validation** : débrancher le contrôleur et vérifier le passage à `true`.

---

## 12.6 — Étape 6 : Câblage VBAT dans Failsafe

**Objectif** : faire de VBAT un contributeur Failsafe.

**Fichiers à créer** :

- `src/core/system/vbat/failsafe_channels.inc`
- `src/core/system/vbat/failsafe_processors.inc`
- `src/core/system/vbat/proc_failsafe_vbat.h`
- `src/core/system/vbat/proc_failsafe_vbat.cpp`

**Fichiers à modifier** :

- `src/core/system/vbat/vbat.cpp` : à la fin de `vbat_update()`, réarmer `FAILSAFE_VBAT = true`.
- inclure les `.inc` VBAT dans les `.inc` du module central sous les `#if defined(...)` appropriés.

### Logique processor VBAT

```text
si failsafeBus.active
    force FAILSAFE_VBAT = false

sinon
    si !FAILSAFE_VBAT
        failsafeBus.active = true
    sinon si vbat_is_low(0)
        failsafeBus.active = true

    force FAILSAFE_VBAT = false
```

**Points de vigilance** :

- `true = réarmé / preuve de vie` ;
- `false = non réarmé / stale / fault / consommé` ;
- le non-réarmement déclenche le Failsafe indépendamment du levier ;
- `comBus.batteryIsLow` reste un canal utilisateur séparé ;
- le processor Failsafe doit s'exécuter après `vbat_update()`.

**Validation** :

1. batterie faible → `failsafeBus.active == true` ;
2. **test de staleness** : empêcher volontairement le réarmement de `FAILSAFE_VBAT`, continuer la boucle normalement et vérifier que `failsafe_is_active()` passe à `true` au cycle suivant.

Le mécanisme concret peut être un mock, hook ou macro de test selon l'infrastructure existante, mais il doit empêcher le réarmement sans modifier la logique du processor.

---

## 12.7 — Étape 7 : Emplacement COMBUS_LINK (no-op)

**Objectif** : réserver l'intégration de `FAILSAFE_COMBUS_LINK` sans figer son mécanisme de détection.

**Fichiers à créer** :

- `src/core/system/combus/failsafe_channels.inc`
- `src/core/system/combus/failsafe_processors.inc`
- `src/core/system/combus/proc_failsafe_combus_link.h`
- `src/core/system/combus/proc_failsafe_combus_link.cpp`

**Contrat volontairement non figé** :

Cette étape ne décide pas encore si le levier sera un timeout, une perte de frames, un compteur, une erreur de checksum ou autre chose. Son objectif est uniquement de réserver le point d'intégration.

**Validation** : compilation dans toutes les variantes.

---

## 12.8 — Étape 8 : Enregistrement runtime de la chaîne Failsafe

**Objectif** : appeler `failsafe_update()` après toutes les sources et avant toute réaction.

**Fichiers à modifier** :

- `src/machines/main.cpp`
- `src/machines/init/init.cpp`
- éventuellement `output_manager.*` selon le besoin réel.

### Ordre cible

```text
loop()
  ├─ sys_manager_update()
  ├─ proc_chain_update(machine.inputChain)
  ├─ proc_chain_update(machine.simChain)
  ├─ vbat_update()
  ├─ autres sources / contributeurs
  ├─ failsafe_update()
  ├─ réaction temporairement conservée pendant transition
  ├─ FSM RunLevel
  └─ output_update(comBus)
```

**Invariant** :

> Toutes les mises à jour des modules sources doivent être terminées avant que la chaîne Failsafe ne s'exécute, à chaque cycle, sans exception.

**Points de vigilance** :

- `inputChain` et `simChain` tournent désormais à chaque cycle, y compris pendant un fault ;
- la chaîne Failsafe n'a pas d'early exit structurel ;
- `simChain` peut enfin réellement converger vers le neutre pendant un fault ;
- l'ancien chemin de réaction reste temporairement actif tant que la nouvelle réaction n'est pas validée ;
- `failsafe_update()` reste indépendant des RunLevels.

---

## 12.9 — Étape 9 : Réaction au Failsafe côté environnement

**Objectif** : déplacer la réaction hors du `main.cpp` vers `env/config/failsafe/`.

**Fichiers à créer** :

- `src/machines/config/failsafe/failsafe_reaction.h`
- `src/machines/config/failsafe/failsafe_reaction.cpp`
- `src/machines/config/failsafe/proc_failsafe_reaction.h`
- `src/machines/config/failsafe/proc_failsafe_reaction.cpp`
- `src/machines/config/failsafe/failsafe_chain.inc`

### Convention

- `proc_failsafe_reaction` = processor de réaction ;
- `failsafe_reaction_update()` = façade qui exécute la chaîne.

```text
main.cpp
    └─ failsafe_reaction_update()
            └─ proc_chain_update(failsafeReactionChain)
                    └─ proc_failsafe_reaction(...)
```

### Ordre runtime définitif

```text
sources
    ↓
failsafe_update()
    ↓
failsafe_reaction_update()
    ↓
FSM RunLevel
    ↓
output
```

La réaction intervient donc **avant la FSM du même cycle**.

**Points de vigilance** :

- elle ne lit pas les `FAILSAFE_X` ;
- elle conserve la logique rising edge si nécessaire ;
- elle force IDLE, nettoie `KEY_ACTIVE` et désactive les drivers selon le comportement historique.

**Validation** : aucun comportement de bord perdu.

---

## 12.10 — Étape 10 : Réaction côté remote (placeholder)

**Objectif** : préparer `src/remotes/config/failsafe/`.

**Fichiers à créer** :

- `failsafe_reaction.h`
- `failsafe_reaction.cpp`
- `proc_failsafe_reaction.h`

La réaction concrète reste à définir lorsque l'architecture remote sera outillée.

**Validation** : `pio run -e remotes`.

---

## 12.11 — Étape 11 : SYSTEM / LOCAL et `COMBUS_FLAG_FAILSAFE`

Chaque binaire possède son propre Failsafe local. Le bit `COMBUS_FLAG_FAILSAFE` transporte un état **SYSTEM** entre firmwares.

### Exemple sound node

```text
local failsafe
        OR
received COMBUS_FLAG_FAILSAFE
        ↓
effective failsafe state
```

Le sound node combine donc :

- son propre `failsafeBus.active` ;
- l'état SYSTEM reçu par trame.

Les deux sources se combinent ; elles ne se remplacent pas.

**Points de vigilance** :

- `failsafe_is_active()` reste local au firmware courant ;
- le bit trame ne remplace jamais les fautes locales.

---

## 12.12 — Étape 12 : Suppression de l'ancien chemin

**Objectif** : retirer définitivement la décision historique `failsafe = !isDrived`.

### Précondition dure

> La nouvelle chaîne de réaction environnementale doit être active et validée avant toute suppression de `SysResult.failsafeActive` ou de l'ancien chemin de réaction.

**Fichiers à modifier** :

- `src/machines/system/sys_manager.h`
- `src/machines/system/sys_manager.cpp`

`isDrived` reste un signal open-drain, pas une décision Failsafe.

**Validation** :

1. PS4 débranchée → Failsafe + réaction IDLE ;
2. batterie faible → même comportement ;
3. aucun ancien `return early` structurel ne reste ;
4. régression sur les variantes principales.

---

## 12.13 — Étape 13 : Publication TX de `COMBUS_FLAG_FAILSAFE`

**Objectif** : publier l'état Failsafe local de l'émetteur dans la trame.

**Fichiers à modifier** :

- `src/core/system/combus/protocol/combus_tx.cpp`
- `src/core/system/combus/frame/combus_frame.h`

Le bit reste un transport d'état SYSTEM ; il ne partage pas le `failsafeBus` local.

**Validation** : machine → sound node avec perte de manette.

---

## 12.14 — Fichiers impactés (consolidé)

### Création

- `src/core/system/failsafe/*`
- `src/core/system/vbat/failsafe_channels.inc`
- `src/core/system/vbat/failsafe_processors.inc`
- `src/core/system/vbat/proc_failsafe_vbat.*`
- `src/core/system/combus/failsafe_channels.inc`
- `src/core/system/combus/failsafe_processors.inc`
- `src/core/system/combus/proc_failsafe_combus_link.*`
- `src/machines/config/failsafe/*`
- `src/remotes/config/failsafe/*`

### Modification

- `src/machines/main.cpp`
- `src/machines/init/init.cpp`
- `src/machines/system/sys_manager.h`
- `src/machines/system/sys_manager.cpp`
- `src/core/system/output/output_manager.*` si nécessaire
- `src/core/system/combus/protocol/combus_tx.cpp`
- `src/core/system/combus/frame/combus_frame.h`
- `src/core/system/vbat/vbat.cpp`
- `src/sound_module/system/combus_sound_interpreter.cpp`

### Suppression / déplacement

Aucun fichier n'est à supprimer ni déplacer. L'ancien mécanisme est retiré uniquement après validation du nouveau chemin complet.

---

## 12.15 — Ordre d'implémentation

1. Squelette `failsafe`.
2. Processor de reset.
3. `failsafe_access.h`.
4. Dépréciation de `failsafeActive`, ancien comportement intact.
5. `proc_failsafe_input_link`, non encore branché.
6. VBAT contributor + test de staleness.
7. Emplacement COMBUS_LINK no-op.
8. Branchement runtime de `failsafe_update()`.
9. Réaction via `env/config/failsafe/`.
10. **Validation obligatoire de la détection et de la réaction ensemble.**
11. Squelette remote.
12. SYSTEM / LOCAL et consommation du flag trame.
13. Suppression de l'ancien chemin.
14. Finalisation de la publication TX du bit trame.

### Règle de transition

> L'ancien chemin de réaction ne peut être supprimé qu'après validation de la chaîne de réaction environnementale. À aucun moment une source de fault active ne doit pouvoir produire un `FAILSAFE` sans réaction associée.

À chaque étape applicable :

```text
pio run -e volvo_A60H_bruder
pio run -e remotes
pio run -e sound_node_volvo
pio test -e test_combus_loopback
```

---

## 12.16 — Points à vérifier avant codage

1. `failsafe_init(ComBus&)` est-il réellement nécessaire au core ou seulement à certains processors ?
2. Quel doit être le périmètre final de `sys_manager_update()` après refonte ?
3. Quel mécanisme de test de staleness s'intègre le mieux à l'infrastructure existante ?
4. Quel sera le levier concret de `FAILSAFE_COMBUS_LINK` ?
5. Quelle réaction concrète doit appliquer le remote ?

---

## 12.17 — Risques de régression

1. **Réordonnancement du `loop()`** : l'ordre définitif est `sources → failsafe_update() → failsafe_reaction_update() → FSM → output`.
2. **Transition ancien / nouveau chemin** : ne pas supprimer l'ancien avant validation de la nouvelle réaction.
3. **`PAUSE_LOG_AFTER_INIT`** : initialiser le Failsafe correctement sans appeler prématurément son `update()`.
4. **`simChain` pendant un fault** : elle tourne désormais toujours ; vérifier que les sorties dangereuses restent bloquées.
5. **Évolution des layers ComBus** : si les pivots deviennent des canaux, expliciter les layers.
6. **Tests loopback** : éviter les dépendances involontaires dans les filtres de build.
7. **Sound node** : combiner SYSTEM reçu et LOCAL.
8. **Ordre des sources** : toute nouvelle chaîne doit finir avant `failsafe_update()`.
9. **Staleness** : chaque contributeur doit avoir un test où son propriétaire cesse de réarmer son `FAILSAFE_X`.
10. **Premier cycle** : pas de grace period ; la sûreté repose sur l'ordre d'exécution et le premier passage des sources avant la première évaluation.

---

## 12.18 — Journal d'étapes (validation)

Cette section consigne la validation de chaque étape une fois terminée.
Elle est ajoutée au WIP pour matérialiser les étapes "validation pure"
(aucun changement de code) dans l'historique git.

### Étape 3 — Façade `failsafe_access.h` (validation, sans modification)

**Date** : 8/12/2026
**Branche** : `failsafe-module`
**Commit parent** : `3d6e62d` (étape 2)

**Conclusion** : `failsafe_access.h` créé à l'étape 1 satisfait déjà le
contrat. Aucune modification du code n'est nécessaire.

Contrat validé :

```cpp
inline bool failsafe_is_active() { return failsafeBus.active; }
```

Vérifications effectuées :

- Compilation `volvo_A60H_bruder` SUCCESS (1 139 029 octets Flash,
  45 748 octets RAM).
- Smoke-test temporaire `_smoke_access.cpp` compilé sans erreur ni
  warning. Le fichier a été supprimé après validation.
- Aucune dépendance vers `machines/`.
- Aucun header lourd non justifié (`failsafe.h` inclut uniquement
  `<stdint.h>`).

Diff vs étape 2 : 0 octet, 0 fichier modifié.


TMP

Étape	Action A16.5	Risque	Commit attendu
D1	Réécrire doc/WIP - Failsafe module design.md §6, §7, §12.1-12.3, §12.14 (aligner sur l'architecture actuelle)
12.5	Hors scope — TODO ajoutée dans WIP	—	—
12.7	Hors scope	—	—
12.9 partiel	Créer cb_set_runlevel (générique, proc->cfg->target). PAS de câblage dans failsafe_chain.	Faible — code isolé, testé au build.	A16.5
12.10	Hors scope	—	—
12.11	Audit seulement (lecture combus_struct.h + combus_set_digital). Proposition de modèle, pas de code.	Aucun (lecture seule).	(rapport)
12.12	Fusionné avec D2 (suppression de l'inférence Failsafe depuis isDrived, pas suppression d'isDrived).	—	A16.5
12.13	Dépend 12.11


1. Oui
2. Oui si plus utilisé et remplacé par la nouvelle implémentation failsafe. Si 
3. Topo :
- 12.5 - bus.isDrived : Déprécié pour moi ... Il s'agit d'une dette de l'ancien enbrillon failsafe. Je supprimerai au profit de combus failsafe dédié aux inputs. Donc, pour ce point, analyse le débranchement + supression du paramètre (structure combus) et supression de l'ancien code failsafe qui s'y raporte. Les modules reprendront cette charge lors de leur rework (dès qu'on en a fini avec failsafe)
- 12.7 - sera de la responsabilité du module link. Etape à ignorer.
- 12.9 - Réaction à failsafe : Je pencherai aussi pour un processeur combus, en fin de chaine. Genre "set runlevel". J'aimerai juste ne pas figer le runlevel pour toutes les machines. Un runlevel failsafe branché quelquepart dans la config machine (runlevel failsafe = runlevel iddle) me semble le plus propre. Les runlevels sont de toute façon défini dans le main, côté machine.
12.10 : hors scope. On s'en occupera plus tard.
12.11 :  Le point soulève une notion assez importante du failsafe, et des canneaux combus en général. "Qui a la main dessus?" (propriétaire) et "comment fusionner les données" (encas d'input par exemple). Un ownership a déjà été tenté, sans succes, et le layering REMOTE/LOCAL/SYSTEM s'avère le plus efficace. Pour continuer dans ce sens, j'ajouterai don une petite surcouche au paramètre de direction des cobus. Ex : un "both_or" permettrait d'indiquer que si un des deux combus à fusionner lors de l'input est true, écrire true. C'est fort semblable au cb_proc_or créé tout à l'heure, pais pour la gestion de l'import des combus par input module.
12.12 - Suppression de l'ancien chemin -> se raporter au point 12.5 ci haut. Donc oui, avec adaptation future des modules d'input au mécanisme de failsafe
12.13 - Publication TX de `COMBUS_FLAG_FAILSAFE` -> résolu par le point 12.11. FAILSAFE sera LOCAL, de direction "booth_or"

Je t'invite à analyser ces propositions, faire une petite audite de leu mise en place et me proposer ton champs d'action avant toute modif.
