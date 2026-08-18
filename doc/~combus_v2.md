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
| **REMOTE** | Données synchronisées entre plusieurs nœuds | Fichier de définiton "core" d'un type de machine |
| **LOCAL** | Données partagées au niveau de la machine | Fichier de définiton "machine" |
| **SYSTEM** | Données privées au firmware d'une carte | Fichier de définiton "core" d'un module d'execution |


Le layer d'un canal définit uniquement sa portée de diffusion et son emplacement de définition.

Une configuration combus est obtenue par superposition des layers :

```text
REMOTE
  +
LOCAL
  +
SYSTEM
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

- Direction (RX / TX / ALL) -> difusion ou non des valeurs en cas de module d'entrés/sortie (comùplémentaire au layering)
- Priorité (QOS réseau) -> service minal en fonction de la charge réseau

---

## ComBus Processors

Les ComBus Processors suivent la même logique de composition que l'organisation de canaux.

Chaque couche peut ajouter, remplacer ou désactiver des traitements.

Le comportement final résulte de l'assemblage de l'ensemble des processeurs actifs.

---

# Phases d'implémentation

## Phase 1 — Layering (ChanLayer)

### Objectif
Ajouter une hiérarchie au niveau des canaux ComBus pour rendre plus flexible leur définition et gestion au runtime.

Les 3 layers définis :
- **REMOTE** : Données synchronisées entre plusieurs nœuds
  - Définition : Fichier "core" d'un type de machine
  - Portée : Inter-node
- **LOCAL** : Données partagées au niveau de la machine
  - Définition : Fichier "machine" spécifique
  - Portée : Intra-node
- **SYSTEM** : Données privées au firmware d'une carte
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
   - `src/core/system/combus/combus_struct.h` → Structure `ComBus` avec champ `layer`
   - `src/core/system/combus/combus_access.cpp` → Règles d'accès `_layer_ok()`
   - `src/core/config/machines/*/combus/combus.cpp` → Configuration des layers par canal
   - `include/struct/combus_struct.h` → Définition `ChanLayer` enum

### Point d'attention
1. **Conflits entre layers** :
   - Option A : Le layer de poids fort (REMOTE) est prioritaire
   - Option B : Erreur en cas de conflit (préférable car plus franc)
   - **À discuter** : Stratégie de résolution des conflits

2. **Contrôle d'accès** :
   - Évaluer son utilité dans le nouveau modèle
   - Si toujours utile : utiliser l'ownership des canaux pour une identification automatique des layers
   - **À discuter** : Règles d'accès `callerLayer >= channelLayer` (SYSTEM ≥ LOCAL ≥ REMOTE)

3. **État actuel** :
   - ✅ `ChanLayer` déjà défini (SYSTEM, LOCAL, REMOTE, UNDEFINED)
   - ✅ Règles d'accès `_layer_ok()` implémentées dans `combus_access.cpp`
   - ✅ Configuration des layers par canal dans `combus.cpp`
   - ⚠️ Structure multisource à finaliser
   - ⚠️ Fusion des layers à implémenter

### Questions ouvertes
1. **Ordre de superposition** : REMOTE → LOCAL → SYSTEM ou SYSTEM → LOCAL → REMOTE ?
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
- **REMOTE** : Configuration générique type machine
- **LOCAL** : Configuration spécifique machine  
- **SYSTEM** : Configuration firmware carte

### Implémentation
1. **Structures de données** :
   - `ComBusV2` avec 3 pointeurs `core_remote`, `local_machine`, `system`
   - `ComBusConfig` contenant canaux + métadonnées source
   - Registre fusionné unique `merged_channels`

2. **Algorithme de fusion** :
   - Séquentiel : REMOTE → LOCAL → SYSTEM
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
3. **Initialisation ordre** : Séquentiel strict REMOTE → LOCAL → SYSTEM
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
- `include/struct/combus_struct.h` → Ajout types v2, adaptation `ChanLayer`
- `src/core/system/combus/combus_access.cpp` → Adaptation règles d'accès pour v2
- `src/core/config/machines/*/combus/` → Réorganisation configs par layer

### Validation
- [ ] Tests unitaires fusion avec/sans conflits
- [ ] Benchmarks performance initialisation
- [ ] Tests mémoire (pas de duplication)
- [ ] Validation compile-time plages d'ID
- [ ] Tests intégration configs existantes

### Documentation associée
- `combus_v2_layering_code.md` → Code d'implémentation détaillé
- `doc/combus_V2.md` → Cette section
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

Un channel déclaré `REMOTE` représente une donnée échangée entre nœuds.

Sa `direction` décrit **exclusivement** sa circulation sur le wire inter-node :

- `uplink` : le nœud local publie la valeur vers le nœud distant ;
- `downlink` : le nœud local reçoit la valeur depuis le nœud distant ;
- `both` : la valeur circule dans les deux directions.

Un channel `REMOTE` est **également présent** dans la représentation ComBus locale (`LOCAL`) de chaque board appartenant au nœud.

Cette propagation intra-node est **indépendante de `direction`** :

- elle ne constitue **pas** une transmission wire ;
- elle ne modifie **pas** la direction REMOTE déclarée ;
- elle doit rendre le channel disponible de manière cohérente sur les boards du même nœud.

La représentation `LOCAL` issue d'un channel `REMOTE` est donc implicitement disponible pour les échanges entre boards du nœud, **sans que cette propagation soit exprimée comme une seconde `direction` dans le `.cb`**.

La question d'un éventuel `owner` du channel ou d'une direction spécifique liée au module producteur est volontairement hors de ce contrat et reste à décider ultérieurement.

### Principe (wire)

`uplink` / `downlink` indiquent le sens de transmission sur le fil, par rapport au nœud de référence (machine). Ils ne définissent pas le nœud : ils définissent le sens du canal dans le contrat remote.

### Format

```yaml
scope: REMOTE
direction:
  - uplink
  - downlink
```

Une définition peut éventuellement être bidirectionnelle si elle porte les deux flags.

### Exemples

- `FAILSAFE_VBAT` (contributeur Failsafe) : `direction: [uplink]` — le module vbat envoie le signal en amont vers le core failsafe.
- `FAILSAFE` (agrégateur) : `direction: [downlink]` — le core failsafe publie l'état agrégé en aval vers les consommateurs.

### Notes d'implémentation

- La propagation intra-node est gérée par le runtime ComBus (cf. `include/struct/combus_struct.h` — la présence d'un channel dans la vue `LOCAL` découle de sa présence dans la vue `REMOTE`).
- Aucun champ additionnel n'est nécessaire dans le `.cb` pour exprimer cette propagation.

