# Failsafe Module — Design Intent

Résumé d'intention pour la refonte du failsafe, actuellement éclaté entre
le main et l'accesseur combus.

---

## Constat actuel

- Logique failsafe dispersée entre le main et l'accesseur combus —
  brouillon, freine la croissance saine du projet.
- Le runlevel `IDLE`/`SLEEPING` est aujourd'hui assignable librement par
  n'importe quel process système.
- La détection de perte de connexion combus repose sur un flag
  activé/reset à chaque loop — fonctionnel, mais fragile (silencieux en
  cas de gel du process censé le mettre à jour).

---

## Besoin

- Centraliser détection (par source) et agrégation en un état failsafe
  global cohérent.
- Éliminer tout mode de défaillance silencieux (source figée = jamais
  détectée).
- Réutiliser l'infrastructure existante (combus, processeurs, build
  flags) plutôt qu'un nouveau mécanisme parallèle.
- Rester 100% compile-time (pas de registration runtime nécessaire).
- Permettre une réaction différenciée par board/scope, pas uniquement une
  réaction globale unique.

---

## Solution retenue (intention)

### Détection par source

- Chaque source de fault potentiel (VBAT, lien combus, etc.) a son propre
  canal combus dédié (ex. `FAILSAFE_VBAT`), regroupé sous une classe/thème
  combus **failsafe**, au même titre que les classes existantes
  (input, vbat, ...).
- Polarité de sécurité inversée : chaque canal failsafe est **en erreur
  par défaut**. C'est le process propriétaire qui doit activement clear
  l'erreur à chaque cycle tant qu'il est sain. L'absence de mise à jour
  (process figé/planté) laisse donc le canal en erreur par défaut, plutôt
  que de faire confiance à une dernière valeur connue.

### Agrégation — chaîne de processeurs combus (portée SYSTEM)

- Chaque process concerné contribue son propre processeur à une chaîne
  dédiée, qui évalue son canal `FAILSAFE_X` et écrit le canal `FAILSAFE`
  général, avec early exit dès le premier fault détecté.
- La chaîne série + early exit donne les deux sémantiques voulues sans
  opérateur séparé :
  - **Entrée** en failsafe dès qu'une seule source fault (OR à
    court-circuit).
  - **Sortie** de failsafe seulement si toutes les sources ont été
    traversées sans déclencher (AND obtenu gratuitement).
- Règle d'implémentation : la chaîne doit toujours s'exécuter dans son
  intégralité à chaque cycle (aucun saut conditionnel d'un stage), pour
  ne jamais retomber dans un état stale silencieux.

### Composition de la chaîne (compile-time, sans X-macro)

- Pas de registre générique type X-macro (jugé peu intuitif à
  maintenir). Chaque module contribue son processeur à la chaîne,
  conditionné par son propre flag `HAS_*`, en s'appuyant sur le système
  de déclaration de canaux combus déjà existant comme registre naturel.

### Réaction — seconde chaîne courte, portée LOCAL

- Une chaîne de processeurs distincte et courte évalue le résultat
  `FAILSAFE` (SYSTEM) pour déterminer le comportement local adéquat (ex.
  transition vers l'état `IDLE`).
- Sépare détection/agrégation (chaîne longue, portée SYSTEM) et réaction
  (chaîne courte, portée LOCAL) : chaque board combine le failsafe SYSTEM
  propagé sur le bus avec ses propres faults locaux, pour décider de son
  propre comportement — sans dupliquer toute la logique d'agrégation sur
  chaque board.

---

## Cohérence avec l'existant

- Réutilise l'infrastructure de processeurs combus déjà documentée
  (config-driven, enum + tableau de structures) plutôt qu'un moteur
  d'agrégation parallèle.
- S'appuie sur les mêmes conventions de build flags (`HAS_*`/`IS_*`)
  déjà en place ailleurs dans le projet.
- Remplace la fragilité actuelle du flag combus "activé/reset à chaque
  loop" par une propriété générique de canal (staleness par défaut),
  réutilisable pour toute source de failsafe — pas seulement le lien
  combus principal.
- Devient l'unique autorité écrivant la transition `IDLE`/`SLEEPING`,
  remplaçant le comportement actuel où n'importe quel process système
  peut l'assigner librement.

---

## Points ouverts

- Emplacement exact des canaux `FAILSAFE_X` : nouvelles entrées dans les
  enums `AnalogComBusID`/`DigitalComBusID` existants, ou espace dédié à
  la classe failsafe.
- Granularité exacte de la portée LOCAL (par board ? par process ?) — à
  préciser au fil de l'implémentation.
- Découpage exact des canaux combus en thèmes (input, vbat, failsafe, ...)
  — pas encore tranché.

---

## Annexe — Pattern d'include dynamique (`#ifdef` + `.inc`)

Mécanisme retenu pour que chaque module contribue ses canaux/processeurs
sans qu'un fichier central doive être édité à la main en connaissance de
tous les modules existants.

### Canaux combus — un `.inc` par thème, agrégés dans `combus_system.inc`

Chaque thème (failsafe, vbat, input, ...) possède son propre `.inc`,
contenant lui-même un guard `HAS_*` par canal. Le fichier d'agrégation ne
fait qu'inclure un `.inc` par thème existant — aucune logique
conditionnelle à ce niveau.

```cpp
// failsafe_channels.inc — un seul fichier, un seul thème
#ifdef HAS_VBAT_MONITOR
    FAILSAFE_VBAT,
#endif
#ifdef HAS_COMBUS_LINK
    FAILSAFE_COMBUS_LINK,
#endif
// tout nouveau canal failsafe s'ajoute ici, une ligne, son propre guard
```

```cpp
// combus_system.inc — agrégateur, un include par thème
#include "failsafe_channels.inc"
#include "vbat_channels.inc"
#include "input_channels.inc"
```

```cpp
// combus_ids.h
enum class ComBusID : uint8_t {
#include "combus_system.inc"
    WIRE_END,
};
```

### Chaîne de processeurs — `#ifdef` inline, sans indirection

Contrairement aux canaux, la chaîne de processeurs reste un seul fichier
plat avec ses guards `HAS_*` inline — pas de `.inc` séparé. Choix motivé
par la lisibilité sous IDE (grisage automatique de l'intellisense sur le
code inactif), plus parlant qu'un fichier séparé à ouvrir pour savoir ce
qui est actif.

```cpp
// failsafe_chain_config.h
static constexpr ProcessorConfig failsafe_chain[] = {
#ifdef HAS_VBAT_MONITOR
    { ProcessorType::VBAT_FAILSAFE, FailsafeComBusID::FAILSAFE_VBAT },
#endif
#ifdef HAS_COMBUS_LINK
    { ProcessorType::LINK_FAILSAFE, FailsafeComBusID::FAILSAFE_COMBUS_LINK },
#endif
};
```