# Module failsafe

Ce document est la référence du module failsafe : ses deux chaînes (dure et douce), ses conventions de nom et ses règles. La chaîne douce a son propre document, `src/core/system/com/link_health/failsafe_soft.md`.

Mis à jour le 2026-10-27 (link rework, lot « sécurité », étapes S1 à S3a). Les noms d'avant (`FAILSAFE`, `REMOTE_LINK_LOST`, `FAILSAFE_VBAT`…) se retrouvent dans l'historique git.

---

## 1. Deux sévérités, deux chaînes

Le module distingue deux sévérités de faute. Chacune a son agrégat et sa réaction :

| Chaîne | Agrégat | Réaction (runlevel) | Exemple de cause |
|---|---|---|---|
| **dure** | `FAILSAFE_HARD` | `HALT` : moteurs arrêtés, endormis, désactivés ; clé bloquée tant que la faute dure | batterie basse, perte d'un lien entre cartes |
| **douce** | `FAILSAFE_SOFT` | `IDLE` : mise en attente ; la reprise demande la clé | perte de la télécommande, perte d'un lien vers un autre nœud |

L'agrégat dit la **sévérité**, pas la réaction : la réaction peut évoluer sans renommage. La chaîne dure l'emporte toujours sur la douce (§6).

Le module entier est activé par `HAS_FAILSAFE` (posé par `[env:machines]`). Sans lui, il n'y a ni chaîne, ni agrégat, ni contributeur.

---

## 2. Conventions de nom

| Élément | Forme | Exemples |
|---|---|---|
| agrégat | `FAILSAFE_<SÉVÉRITÉ>` | `FAILSAFE_HARD`, `FAILSAFE_SOFT` |
| contributeur | `<SOURCE>_FAIL` | `VBAT_FAIL`, `BOARD_LINK_FAIL`, `NODE_LINK_FAIL`, `LOCAL_REMOTE_FAIL` |
| flag d'un contributeur | `HAS_<SOURCE>_FAILSAFE` | `HAS_VBAT_FAILSAFE`, `HAS_BOARD_LINK_FAILSAFE` |
| flag du module | `HAS_FAILSAFE` | — |

Tous ces channels sont du thème **`failsafe`** : c'est le **groupe failsafe**. Ce sont des alarmes, « vrai = faute ». Leur valeur sûre est donc `true`, déclarée `value: fault` dans le `.cb` (§4).

---

## 3. Contributeurs

Un contributeur dit un **fait** (« la batterie défaille », « un lien entre cartes est tombé »), sans sévérité. C'est la chaîne qui le lit qui décide de la sévérité.

| Contributeur | Fait | Écrit par | Flag | Chaîne aujourd'hui |
|---|---|---|---|---|
| `VBAT_FAIL` | batterie sous le seuil | `vbat_update()` (`src/core/system/vbat/vbat.cpp`) | `HAS_VBAT_FAILSAFE` | dure |
| `BOARD_LINK_FAIL` | un lien qui reçoit, entre cartes du nœud, est perdu | `com_rx_update()` (`src/core/system/com/com_manager.cpp`) | `HAS_BOARD_LINK_FAILSAFE` | dure |
| `NODE_LINK_FAIL` | un lien qui reçoit, vers un autre nœud, est perdu | `com_rx_update()` | `HAS_NODE_LINK_FAILSAFE` | douce |
| `LOCAL_REMOTE_FAIL` | la télécommande locale (manette PS4 ; demain récepteur PPM/SBUS) ne répond plus | `input_update()` (`src/machines/system/input/input_update.cpp`) | `HAS_LOCAL_REMOTE_FAILSAFE` | douce |

Un contributeur exige `HAS_FAILSAFE` et **son propre flag**, jamais celui d'une chaîne : il reste neutre et **rebranchable**.

**Rebranchable veut dire déplaçable, jamais dupliqué.** Un contributeur est lu par **une seule chaîne** : la chaîne le réarme à `true` après lecture (§4), si bien qu'une seconde chaîne lirait une fausse faute. Pour changer la sévérité d'un contributeur, on déplace sa ligne d'une chaîne à l'autre.

`BOARD_LINK_FAIL` et `NODE_LINK_FAIL` sont alimentés par la **portée** du lien (link rework S2) : un lien entre cartes compte comme perdu s'il est silencieux, ou jamais vu une fois son délai de grâce (`startupGraceMs`) écoulé ; un lien vers un autre nœud compte dès qu'il n'est pas vivant, jamais vu compris. Il n'y a plus de politique de perte par lien (`onLoss`, supprimé) : la sévérité se décide en plaçant le contributeur dans une chaîne.

---

## 4. Preuve de vie : `fault` par défaut et réarmement

Chaque alarme vaut `true` (faute) par défaut : `value: fault` dans son `.cb`. Le builder le vérifie : un numérique du thème `failsafe` doit déclarer `fault`, et `fault` est refusé ailleurs.

À chaque cycle :

```text
module source  → écrit son contributeur à false (sain) s'il va bien
chaîne         → lit le contributeur, fait le OU dans l'agrégat,
                 puis le remet à true (cb_or_fn : « consume + guard »)
```

Si le module source cesse d'écrire, son contributeur reste à `true` : la faute est levée. Le contributeur est à la fois un état de santé et une **preuve de vie**, sans délai ni horodatage. Conséquence : la valeur instantanée d'un contributeur sur le bus n'est pas l'état de santé ; il faut lire l'agrégat.

---

## 5. Les chaînes

Les deux chaînes ont la même forme : un reset en tête (`cb_reset_fn`, agrégat à 0 = sain), puis un `cb_or_fn` par contributeur (entrée = sortie = le contributeur). L'agrégat passe à `true` dès qu'un contributeur est en faute, pour le cycle.

| | Chaîne dure | Chaîne douce |
|---|---|---|
| code | `src/core/system/failsafe/failsafe_chain.cpp` (`kFailsafeChain[]`) | `src/core/system/com/link_health/remote_link_fallback_chain.cpp` (`kRemoteLinkFallbackChain[]`) |
| lancée par | `failsafe_update()` | `remote_link_fallback_update()` |
| contributeurs | `VBAT_FAIL`, `BOARD_LINK_FAIL` | `LOCAL_REMOTE_FAIL`, `NODE_LINK_FAIL` |

Chaque contributeur est dans un bloc `#if defined(HAS_<SOURCE>_FAILSAFE)`. Une chaîne sans contributeur écrit `false` à chaque cycle.

**Ordre d'exécution** (`sys_manager_update()`, `src/machines/system/sys_manager.cpp`) : réception (`com_rx_update`, qui écrit les contributeurs de lien), entrées (`input_update`), batterie (`vbat_update`), chaîne dure, chaîne douce. Toutes les sources ont donc écrit avant les chaînes, et les deux agrégats sont frais avant la chaîne runlevel et la machine à états de `main.cpp`. La valeur de démarrage des agrégats n'atteint jamais la machine à états.

---

## 6. Réaction : la chaîne runlevel

Le module ne connaît ni les runlevels, ni les machines. La réaction est dans la chaîne runlevel de la machine, `kRunlevelProcs[]` (`src/machines/config/machines/volvo_A60H_bruder/combus/processors/input/runlevel_config.h`), dans cet ordre :

1. **`FAILSAFE_HARD`** (`cb_runlevel_fn`, à chaque cycle) : tant qu'il est vrai, écrit `HALT` et fait un *claim*, ce qui saute les étapes suivantes ;
2. **`FAILSAFE_SOFT`** (`cb_runlevel_once_fn`, sur front montant) : écrit `IDLE` et fait un *claim* ;
3. **la clé** (`KEY_ACTIVE`, sur ses fronts) : `STARTING` ou `TURNING_OFF`.

`main.cpp` réagit au runlevel : `case RunLevel::HALT` arrête, endort et désactive les moteurs à l'entrée.

**Cas à connaître** : pendant un *claim* de la chaîne dure, l'étape douce est sautée et ne met pas à jour sa mémoire de front. Si la faute douce est apparue pendant la faute dure et dure encore quand celle-ci retombe, l'étape douce voit alors un front et écrit `IDLE`. La machine passe de `HALT` à `IDLE` sans la clé : on applique la réaction de la faute qui reste.

**Décidé, pas encore fait** (lot « sécurité », S3) : sortir de `HALT` demandera un redémarrage. Ce cas et la sortie par la clé disparaîtront alors.

---

## 7. Règles à conserver

- **Le module** ne contient aucune logique métier (batterie, liens, entrées) : il agrège des contributeurs et publie deux agrégats.
- **Un module source** est propriétaire de son contributeur et l'écrit à chaque cycle.
- **Un contributeur** est lu par une seule chaîne.
- **Une machine** choisit ses contributeurs (flags) et leur chaîne (ligne dans la chaîne), et fournit sa réaction (chaîne runlevel).

---

## 8. Fichiers

```text
src/core/system/failsafe/
├── failsafe.h / .cpp         // failsafe_update() : lance la chaîne dure
├── failsafe_chain.h / .cpp   // kFailsafeChain[] : reset + contributeurs de la chaîne dure
└── failsafe_hard.cb          // FAILSAFE_HARD (SYSTEM, both_or)

src/core/system/com/link_health/
├── failsafe_soft.cb          // FAILSAFE_SOFT (SYSTEM, both_or)
├── remote_link_fallback_chain.h / .cpp   // kRemoteLinkFallbackChain[] : chaîne douce
├── board_link_fail.cb        // BOARD_LINK_FAIL (BOARD)
├── node_link_fail.cb         // NODE_LINK_FAIL (BOARD)
└── failsafe_soft.md          // la chaîne douce

src/core/system/vbat/vbat_fail.cb          // VBAT_FAIL (SYSTEM, uplink_or)
src/core/config/inputs/local_remote_fail.cb // LOCAL_REMOTE_FAIL (BOARD)
src/core/system/combus/processors/logic/cb_or.h   // cb_or_fn
```

---

## 9. Référence rapide

| Concept | Où |
|---|---|
| Agrégats | `failsafe_hard.cb`, `failsafe_soft.cb` |
| Chaînes | `failsafe_chain.cpp`, `remote_link_fallback_chain.cpp` |
| Contributeurs de lien | `com_rx_update()`, `src/core/system/com/com_manager.cpp` |
| Ordre d'exécution | `sys_manager_update()`, `src/machines/system/sys_manager.cpp` |
| Réaction runlevel | `kRunlevelProcs[]`, `.../processors/input/runlevel_config.h` |
| Réaction moteurs | `src/machines/main.cpp`, `case RunLevel::HALT` |
| Lecture | `comBus.digitalBus[DigitalComBusID::FAILSAFE_HARD].value` (ou `FAILSAFE_SOFT`) |
