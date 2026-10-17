# ComBus v2 — Migration Roadmap

## Objectif

Le ComBus actuel remplit son rôle mais son architecture atteint ses limites.

Le but de ComBus v2 est de conserver la simplicité du modèle actuel tout en permettant :

- une meilleure modularité des configurations via un layering;
- une organisation plus claire des canaux par thèmes;
- une prise en charge plug an play des accessoires et extensions ;
- une nouvelle métadonnée canaux dans un optique transport (RX/TX)

Le rework concerne uniquement l'architecture du ComBus. Son principe reste inchangé : le ComBus demeure le registre fonctionnel partagé utilisé par les modules pour collaborer.

---

# Architecture cible

## Layering

Le ComBus distingue trois niveaux de visibilité/définition :

| Layer | Utilisation | Définition |
|--------|-------------|
| **BASE** | Données synchronisées entre plusieurs nœuds | Fichier de définiton "core" d'un type de machine |
| **SYSTEM** | Données partagées au niveau de la machine | Fichier de définiton "machine" |
| **BOARD** | Données privées au firmware d'une carte | Fichier de définiton "core" d'un module d'execution |


Le layer d'un canal définit uniquement sa portée de diffusion et son emplacement de définition.

Noms avant le 2026-10-18 : `BASE` s'appelait `REMOTE`, `SYSTEM` s'appelait `LOCAL` et `BOARD` s'appelait `SYSTEM`.

Une configuration combus est obtenue par superposition des layers :

```text
BASE
  +
SYSTEM
  +
BOARD
  =
ComBus final
```

Une couche ajoute ou complète des éléments mais ne remplace jamais entièrement une couche inférieure.

---

## Groupes fonctionnels

Les canaux sont regroupés par domaine fonctionnel.

Exemples :

- Motion
- Light
- Core
- Sound
- Battery
- Telemetry

Chaque groupe peut être activé ou non à la compilation selon les besoins d'une configuration.

L'objectif est de limiter les dépendances et de réduire la taille des configurations embarquées.

---

## Métadonnées transport

Chaque canal se doté des métadonnées inhérente à sa transission.

Les informations actuellement retenues sont :

- Direction (`uplink` / `downlink` / `both` / `none`, avec variantes `*_or`) -> diffusion ou non des valeurs en cas de module d'entrées/sortie (complémentaire au layering). Aujourd'hui la direction est un champ de donnée du canal : elle n'est lue ni à l'émission ni à la réception (voir `src/core/system/combus/protocol/frame/combus_frame_protocol.md` §6).
- Priorité (QOS réseau) -> service minal en fonction de la charge réseau

---

## ComBus Processors

Les ComBus Processors suivent la même logique de composition que l'organisation de canaux.

Chaque couche peut ajouter, remplacer ou désactiver des traitements.

Le comportement final résulte de l'assemblage de l'ensemble des processeurs actifs.

---

## ComBus Builder

Les canaux ne sont plus déclarés à la main : le ComBus Builder les lit dans des
fichiers YAML `.cb` et génère les identifiants, les tableaux de canaux, le bus
et les empreintes MD5. Il s'exécute au build, par un hook SCons
(`scripts/combus_scons_hook.py`).

- Fichier `.cb` (syntaxe, champs, diffusion entre cartes, validation MD5) :
  `doc/configuration/cb_syntax.md`.
- Fonctionnement interne du builder (pipeline, modules, diffusion, MD5, sorties,
  limites) : `scripts/combus_builder/README.md`.
- Trame binaire et ordre des canaux sur le fil :
  `src/core/system/combus/protocol/frame/combus_frame_protocol.md`.

État : le mode mono-carte est celui utilisé par `platformio.ini`. Le mode
multi-cartes (nœud avec plusieurs cartes) est implémenté dans le builder mais
n'a pas encore été exercé en build réel. Les alias d'en-têtes par carte et
l'inversion automatique de `direction` côté télécommande ne sont pas
implémentés.

---

# Phases d'implémentation

## Phase 1 — Layering (ChanLayer)

### Objectif
Ajouter une hiérarchie au niveau des canaux ComBus pour rendre plus flexible leur définition et gestion au runtime.

Les 3 layers définis :
- **BASE** : Données synchronisées entre plusieurs nœuds
  - Définition : Fichier "core" d'un type de machine
  - Portée : Inter-node
- **SYSTEM** : Données partagées au niveau de la machine
  - Définition : Fichier "machine" spécifique
  - Portée : Intra-node
- **BOARD** : Données privées au firmware d'une carte
  - Définition : Fichier "core" d'un module d'exécution
  - Portée : Intra-device

### Implémentation
1. **Modification de la structure principale ComBus** pour permettre l'inclusion multisource
   - L'API doit être capable de déterminer à quel layer appartient un canal ComBus (ownership)
   - Fusionner les différents canaux pour retrouver une hiérarchie plate (abstraction du layering)

2. **Modification des structures de configuration actuelles**
   - Remote core à réduire (déjà partiellement fait)
   - Local à créer/étendre
   - System à créer

3. **Fichiers et sections à modifier** :
   - `src/core/system/combus/combus_defs.h` → Structure `ComBus` et définition de l'enum `ChanLayer` (le champ `layer` est porté par chaque canal)
   - `src/core/system/combus/combus_access.cpp` → Règles d'accès `_layer_ok()`
   - `combus.cpp` généré par le ComBus Builder (dossier `combus_generated`) → Configuration des layers par canal (l'ancien `combus.cpp` écrit à la main par machine n'existe plus)

### Point d'attention
1. **Conflits entre layers** :
   - Option A : Le layer de poids fort (BASE) est prioritaire
   - Option B : Erreur en cas de conflit (préférable car plus franc)
   - **À discuter** : Stratégie de résolution des conflits

2. **Contrôle d'accès** :
   - Évaluer son utilité dans le nouveau modèle
   - Si toujours utile : utiliser l'ownership des canaux pour une identification automatique des layers
   - **À discuter** : Règles d'accès `callerLayer >= channelLayer` (BOARD ≥ SYSTEM ≥ BASE)

3. **État actuel** :
   - ✅ `ChanLayer` déjà défini (BOARD, SYSTEM, BASE, UNDEFINED)
   - ✅ Règles d'accès `_layer_ok()` implémentées dans `combus_access.cpp`
   - ✅ Configuration des layers par canal dans `combus.cpp`
   - ⚠️ Structure multisource à finaliser
   - ⚠️ Fusion des layers à implémenter

### Questions ouvertes
1. **Ordre de superposition** : BASE → SYSTEM → BOARD ou BOARD → SYSTEM → BASE ?
2. **Résolution conflits** : Priorité hiérarchique ou validation stricte ?
3. **Contrôle d'accès** : Conserver les règles actuelles ou simplifier ?
4. **Fusion runtime** : Comment gérer la fusion des canaux de différents layers ?

### Validation
- [ ] Vérifier que tous les appels `combus_set_*()` passent un `ChanLayer` valide
- [ ] Tester les règles d'accès avec différents scénarios
- [ ] Valider la configuration des layers dans les fichiers `combus.cpp`
- [ ] Tester la fusion de canaux avec conflits

### Prochaine étape
Une fois cette phase validée, passer à la **Phase 2 — Groupes thématiques**.

## Phase 1 — Structure multisource (Layering) - Implémentation détaillée

### Objectif
Implémenter l'architecture à 3 pointeurs pour la superposition de layers :
- **BASE** : Configuration générique type machine
- **SYSTEM** : Configuration spécifique machine  
- **BOARD** : Configuration firmware carte

### Implémentation
1. **Structures de données** :
   - `ComBusV2` avec 3 pointeurs `core_remote`, `local_machine`, `system`
   - `ComBusConfig` contenant canaux + métadonnées source
   - Registre fusionné unique `merged_channels`

2. **Algorithme de fusion** :
   - Séquentiel : BASE → SYSTEM → BOARD
   - Conflits VERBOTEN (erreur fatale à l'init)
   - Métadonnées de fusion pour debug/tracing

3. **Énumérations séparées** :
   - Namespace `RemoteChannels`, `LocalChannels`, `SystemChannels`
   - Plages d'ID distinctes : 0x0000-0x3FFF, 0x4000-0x7FFF, 0x8000-0xBFFF
   - Validation compile-time des plages

4. **Mécanisme d'enregistrement** :
   - Enum `ComBusLayerEnum` avec `LAYER_COUNT = 3`
   - Tableau `g_layer_configs[LAYER_COUNT]`
   - Bouclage à l'init sur tous les layers

### Point d'attention
1. **Conflits** : Stratégie VERBOTEN - erreur fatale si même ID dans plusieurs layers
2. **Performance mémoire** : Pas de duplication - registre fusionné unique
3. **Initialisation ordre** : Séquentiel strict BASE → SYSTEM → BOARD
4. **Hot-plug modules** : Vérification des canaux requis à l'init
5. **ComBusPatch** : Rejeté pour MVP (trop complexe)

### Fichiers et sections à modifier
**Nouveaux fichiers** :
- `src/core/system/combus/combus_v2_struct.h` → Structures `ComBusV2`, `ComBusConfig`
- `src/core/system/combus/combus_v2_init.cpp` → Initialisation et enregistrement
- `src/core/system/combus/combus_v2_fusion.cpp` → Algorithme de fusion
- `src/core/system/combus/combus_v2_conflict.cpp` → Détection conflits VERBOTEN
- `include/struct/combus_v2_layers.h` → Énumérations par layer

**Modifications existantes** :
- `src/core/system/combus/combus_defs.h` → Ajout types v2, adaptation `ChanLayer`
- `src/core/system/combus/combus_access.cpp` → Adaptation règles d'accès pour v2
- `src/core/config/machines/*/combus/` → Réorganisation configs par layer

### Validation
- [ ] Tests unitaires fusion avec/sans conflits
- [ ] Benchmarks performance initialisation
- [ ] Tests mémoire (pas de duplication)
- [ ] Validation compile-time plages d'ID
- [ ] Tests intégration configs existantes

### Documentation associée
- `combus_v2_layering_code.md` → Code d'implémentation détaillé *(fichier absent du dépôt)*
- `doc/combus_V2.md` → Cette section *(ce document s'appelle aujourd'hui `doc/~combus_v2.md`)*
- Référence : Architecture 3 pointeurs + fusion runtime

### Questions résolues
1. **Structure multisource** : 3 pointeurs + registre fusionné
2. **Conflits** : VERBOTEN (erreur fatale)
3. **Énumérations** : Séparées par layer avec plages distinctes
4. **ComBusModule** : Supprimé (pas nécessaire)
5. **ComBusPatch** : Rejeté pour MVP

### Prochaine étape
Une fois le layering implémenté et validé, passer à la **Phase 2 — Groupes thématiques**.

## Direction (uplink / downlink) et propagation

### Contrat de propagation

Un channel déclaré `BASE` représente une donnée échangée entre nœuds.

Sa `direction` décrit **exclusivement** sa circulation sur le wire inter-node :

- `uplink` : le nœud local publie la valeur vers le nœud distant ;
- `downlink` : le nœud local reçoit la valeur depuis le nœud distant ;
- `both` : la valeur circule dans les deux directions.

Un channel `BASE` est **également présent** dans la représentation ComBus locale (`SYSTEM`) de chaque board appartenant au nœud.

Cette propagation intra-node est **indépendante de `direction`** :

- elle ne constitue **pas** une transmission wire ;
- elle ne modifie **pas** la direction BASE déclarée ;
- elle doit rendre le channel disponible de manière cohérente sur les boards du même nœud.

La représentation `SYSTEM` issue d'un channel `BASE` est donc implicitement disponible pour les échanges entre boards du nœud, **sans que cette propagation soit exprimée comme une seconde `direction` dans le `.cb`**.

La question d'un éventuel `owner` du channel ou d'une direction spécifique liée au module producteur est volontairement hors de ce contrat et reste à décider ultérieurement.

### Principe (wire)

`uplink` / `downlink` indiquent le sens de transmission sur le fil, par rapport au nœud de référence (machine). Ils ne définissent pas le nœud : ils définissent le sens du canal dans le contrat remote.

### Format

```yaml
scope: BASE
direction:
  - uplink
  - downlink
```

Une définition peut éventuellement être bidirectionnelle si elle porte les deux flags.

### Exemples

- `FAILSAFE_VBAT` (contributeur Failsafe) : `direction: [uplink]` — le module vbat envoie le signal en amont vers le core failsafe.
- `FAILSAFE` (agrégateur) : `direction: [downlink]` — le core failsafe publie l'état agrégé en aval vers les consommateurs.

### Notes d'implémentation

- La présence d'un channel BASE dans la vue `SYSTEM` est produite par le générateur : la vue `combus_system` contient les channels BASE et SYSTEM (`scripts/combus_builder/generator.py:401-419`), avec les mêmes identifiants que la vue complète.
- Aucun champ additionnel n'est nécessaire dans le `.cb` pour exprimer cette propagation.

