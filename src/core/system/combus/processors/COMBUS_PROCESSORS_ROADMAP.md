# ComBus Processors — Roadmap des intentions futures

**Date** : 26 juillet 2026
**Statut** : Document d'intentions — RIEN n'est implémenté dans cette session

---

## État actuel (implémenté)

### Processors côté instance machine

Les processors input/ et sim/ vivent entièrement côté instance machine :
- `src/machines/config/machines/volvo_A60H_bruder/combus/processors/input/`
- `src/machines/config/machines/volvo_A60H_bruder/combus/processors/sim/`

Ces processors travaillent sur le ComBus complet (AnalogComBusID/DigitalComBusID), sans distinction remote/local en leur sein.

### Mapping manette (PS4)

Le mapping manette vit également côté instance :
- `src/machines/config/machines/volvo_A60H_bruder/inputs_map/`

### Vocabulaire manette générique

Le vocabulaire de la manette elle-même (boutons, sticks — pas le mapping vers les canaux) reste générique en core :
- `core/config/inputs/PS4_dualshock.h`

Ce vocabulaire est réutilisable par toute instance/machine qui embarque ce type de manette.

---

## Piste 1 — Processors actifs au niveau REMOTE

### Contexte

Certaines télécommandes (ex: une télécommande DIY) sont capables de formater directement leurs données au standard ComBus avant émission. Contrairement à une manette générique (ex: PS4) où c'est la machine qui doit faire ce travail de formatage.

### Objectif futur

Permettre à des processors de tourner côté REMOTE (pas seulement côté machine), pour les cas où le nœud remote lui-même peut produire un ComBus déjà conforme.

### Implications

Réévaluer, au cas par cas, si un processor peut/doit être découpé en :
- Une partie "calcul pur, réutilisable côté remote"
- Une partie "branchement ComBus, propre à chaque nœud"

**Non tranché** — à étudier processor par processor le moment venu.

---

## Piste 2 — Bibliothèque de templates de chaînes de processors (core)

### Objectif

Créer, sous un dossier core (proposition : `src/core/system/combus/processors/modules/`), une bibliothèque de briques de traitement ComBus réutilisables et assemblables.

### Exemples

- Chaîne "gears" combinant plusieurs étapes simples
- Chaîne "throttle" avec rampes configurables
- Chaîne "steering" avec filtrage

### Principe

Chaque brique reste un template paramétrable, pas lié à une instance précise. L'utilisateur final n'a pas à coder depuis zéro un comportement courant.

---

## Piste 3 — Presets de configuration pour ces templates

### En complément de la piste 2

Fournir des jeux de paramètres prêts-à-l'emploi pour ces templates.

### Exemple existant

`kGearShift_VolvoD16J` dans `dumper_truck_motion.h` — preset de boîte de vitesse Volvo D16J.

### Extension future

Créer des presets similaires pour :
- Les chaînes de processors throttle
- Les chaînes de processors steering
- Les chaînes de processors dump

### Utilisation

L'utilisateur choisit un comportement + preset, et peut ensuite affiner en créant sa propre variante.

---

## Piste 4 — Enregistrement modulaire de canaux ComBus

### Constat

Une chaîne de processors peut avoir besoin de canaux ComBus intermédiaires (valeurs transitoires de sortie, non persistantes au niveau machine).

Un module manette (PS4) doit lui aussi déclarer ses propres canaux d'acquisition brute (bouton carré, etc.) avant traitement vers le ComBus machine.

### Objectif futur

Un mécanisme permettant à un module (processor chain, manette, autre) de déclarer/enregistrer dynamiquement ses propres canaux ComBus "modulaires", plutôt que de tout figer dans le tableau statique unique de l'instance.

### Architecture

Non définie à ce stade. Nécessite une réflexion dédiée.

### Liens avec travaux antérieurs

Probablement lié aux discussions déjà eues sur combus_v2.md, layering et fusion runtime. Certaines pistes ont déjà été écartées pour coût mémoire/malloc — à réévaluer sous cet angle précis plutôt que repartir de zéro.

---

## Non actées / non tranchées

### Points en suspens

1. Découpage processors en "calcul pur" vs "binding ComBus" — reporté à une itération future
2. Mécanisme input_map_provider (InputMapView + input_map_active()) — non implémenté dans cette session
3. Enregistrement dynamique de canaux ComBus modulaires — architecture non définie
4. Taille de trame UART variable : actuellement calculée sur le seul contrat REMOTE (hypothèse simplificatrice). À revoir lors de l'ajout de flags d'émission conditionnels (ex: télémétrie uniquement) — fichier concerné : core/config/outputs/combus_uart.h.

### Avertissement

Ce document capture les décisions et intentions discutées. Il ne constitue PAS une spec d'implémentation.

**À relire et mettre à jour AVANT toute implémentation de ces pistes.**

---

## Prochaines étapes suggérées

1. Valider la migration processors/inputs_map vers instance (cette session)
2. Tester la compilation effective avec PlatformIO
3. Évaluer la piste 1 (processors remote) sur un cas concret
4. Concevoir la piste 4 (enregistrement modulaire) en lien avec combus_v2.md

---

*Document généré le 26 juillet 2026 — À mettre à jour avant toute implémentation*