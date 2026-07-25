# Document de référence pour l'implémentation ComBus v2

## Contexte et objectifs

Le ComBus actuel utilise une architecture plate qui atteint ses limites au niveau gestion et utilisation.

Dans sa future version, ComBus v2 devrait implémenter :

### Un système de **Layering** des canauximplémenté avec 3 niveaux :
  - `SYSTEM` (Intra-device) → Données privées firmware
  - `LOCAL` (Intra-node) → Fonctionnalités véhicule  
  - `REMOTE` (Inter-node) → Commandes universelles
- **API processors simplifiée** (4 → 3 paramètres)
- **Code réduit** de ~150 lignes, complexité réduite de 50%

### **Organiser les canaux en groupes thématiques** — activation par compile flag pour optimisation

### **Permettre la superposition de couches** — fusion (pas écrasement) des configurations Base + Machine + Accessoire

### **Ajouter des métadonnées fonctionnelles** — portée, priorité, RX/TX pour les règles de diffusion

### **Adapter le transport** — encodage/décodage avec notion de couche et priorité (QoS)

### **Migrer les fonctions dans la structure** — approche "mini-classe" en C

### **Adapter les processeurs** — même dynamique de fusion que les données

## Architecture cible

```
┌─────────────────────────────────────────────────────────────┐
│                    ComBus v2 (structure unique)             │
├─────────────────────────────────────────────────────────────┤
│  Métadonnées globales :                                     │
│  - Version du ComBus Set                                   │
│  - Nombre de couches fusionnées                           │
│  - Taille totale                                          │
├─────────────────────────────────────────────────────────────┤
│  Groupes thématiques (activation par compile flag) :       │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐        │
│  │ Motion  │ │ Light   │ │ Core    │ │ Sound   │  ...    │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘        │
├─────────────────────────────────────────────────────────────┤
│  Métadonnées par canal :                                   │
│  - ChanLayer (SYSTEM/LOCAL/REMOTE)                       │
│  - Priorité (QoS)                                        │
│  - Direction (RX/TX/RW)                                 │
│  - Couche d'origine (base/machine/accessoire)           │
│  - Normé (nom réservé ou non)                           │
└─────────────────────────────────────────────────────────────┘
```

## Principe des couches — Fusion

Les couches se fusionnent (union), pas d'écrasement.

```
┌─────────────────────────────────────────────────────────────┐
│                    ComBus final (fusion)                    │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Couche Accessoire : ajoute des canaux spécifiques  │   │
│  │  (prise de force, pompe, faucheuse)                │   │
│  ├─────────────────────────────────────────────────────┤   │
│  │  Couche Machine : ajoute des canaux machine         │   │
│  │  (modèles de tracteur, configurations)             │   │
│  ├─────────────────────────────────────────────────────┤   │
│  │  Couche Base : définition générique du ComBus Set   │   │
│  │  (canaux standards de la famille)                  │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## ComBus v2 — Plan de refonte
### Ce qu'on refait et pourquoi

### Problèmes actuels
| Problème | Impact |
|----------|--------|
| Espace plat unique | Tous les canaux mélangés, pas de modularité |
| ✅ Ownership surfait | ✅ **RÉSOLU** : Complexité inutile supprimée |
| Couches mélangées | On ne distingue pas Inter-node / Inter-device / Intra-device |
| Configuration rigide | Une machine ne peut pas hériter ou spécialiser un Set |
| Processeurs liés au firmware | Pas de superposition possible |

### Objectifs v2

1. ✅ **Supprimer l'ownership** → **ACCOMPLI**
2. **Organiser les canaux en groupes** (Motion, Light, Core, Sound, Telemetry, Battery)
3. **Activer/désactiver les groupes par compile flag**
4. **Permettre la superposition de 3 couches** : Base → Machine → Accessoire (fusion, pas écrasement)
5. **Ajouter des métadonnées** : ChanLayer, priorité, RX/TX
6. **Les processeurs suivent la même logique de couches**

## Les concepts

### Groupes thématiques

Les canaux sont rangés par domaine. Chaque groupe peut être activé ou non à la compilation.

| Groupe | Contenu |
|--------|---------|
| Motion | throttle, steering, brake, direction, PTO... |
| Light | headlight, blinkers, hazard, work_light... |
| Core | runlevel, vbat, error_flags, mode... |
| Sound | horn, melody, volume... |
| Telemetry | latitude, longitude, speed, heading... |
| Battery | voltage, current, level, temperature... |

### Couches superposables

3 couches qui se fusionnent (union). Pas d'écrasement.

```
Accessoire : ajoute ses propres canaux (ex: pompe, faucheuse)
     ↓
Machine : spécialise le Set de base
     ↓
Base : définition générique du ComBus Set
```

**Règle** : un même identifiant ne peut apparaître qu'une seule fois. Si un canal est défini dans plusieurs couches, les métadonnées sont fusionnées (priorité max, ChanLayer max...).

**Exemple PTO** :
- Base définit `pto_enable` (ID standard)
- Machine ajoute une priorité élevée (sécurité)
- Accessoire utilise le même canal
- **Résultat** : le bouton PTO active toujours le même canal, seul le traitement change

### Métadonnées

Chaque canal a des métadonnées pour guider la diffusion :

| Champ | Rôle |
|-------|------|
| **ChanLayer** | Niveau d'accès (SYSTEM/LOCAL/REMOTE) - **IMPLÉMENTÉ** |
| Priorité | Importance pour l'envoi (0 = bas, 7 = critique) |
| RX/TX | Peut être reçu/transmis |


### Processeurs

Même logique de couches que les données.

- Un processeur a un nom normé (ex: "pto.control")
- Une couche supérieure peut désactiver un processeur de même nom
- Ordre d'exécution : Base → Machine → Accessoire

**Exemple** :
- Base : processeur "pto.control" (commande standard)
- Accessoire : processeur "pto.control" désactivé
- Accessoire : processeur "pto.pump" (commande pompe)

## Ce qu'on doit implémenter

### Activation

- **Groupes** : compile flags (`CONFIG_COMBUS_MOTION`, etc.). Si absent → groupe désactivé (pointeur `NULL`).
- **Couches** : chargement séparé Base / Machine / Accessoire. Fusion à la finalisation.

### Règles de fusion

| Règle | Description |
|-------|-------------|
| Union | Tous les canaux de toutes les couches sont présents |
| Unicité | Un même ID ne peut apparaître qu'une seule fois |
| Types | Un canal ne peut pas changer de type |
| Métadonnées | Fusion : priorité max, ChanLayer max, RX/TX = OR |

## Phases d'implémentation révisées

### **Phase 0 : Rework ComBus** - ✅ **TERMINÉ** (Juillet 2026)
- Suppression ChanOwner/ComBusOwner/makeChanOwner
- Implémentation ChanLayer (SYSTEM/LOCAL/REMOTE)
- Simplification API processors
- Validation compilation

### **Phase 1 : Groupes thématiques** (2-3 semaines)
1. Définir API `ComBusGroup` avec activation conditionnelle
2. Implémenter compile flags (`-D COMBUS_MOTION`, etc.)
3. Tester avec 2 groupes (Motion, Light)
4. Mesurer impact mémoire/performance

### **Phase 2 : Superposition de layers** (3-4 semaines)
1. API `ComBusLayer` (Base, Machine, Accessory)
2. Mécanisme de fusion (union, pas écrasement)
3. Règles de résolution de conflits
4. Validation à la compilation

### **Phase 3 : Accessoires et branchements** (4-5 semaines)
1. Système de configuration accessoire
2. Canaux de branchement automatiques
3. Communication intégrée (UART, CAN, ESP-NOW)
4. Hot-plug support (détection dynamique)

### **Phase 4 : Métadonnées avancées** (2-3 semaines)
1. Priorité (QoS) pour transport
2. Direction (RX/TX/RW)
3. Noms réservés et plages d'ID
4. Validation sémantique

### **Phase 5 : Transport adaptatif** (3-4 semaines)
1. Encodage/décodage avec métadonnées
2. Compression sélective par priorité
3. Frames versionnées
4. Rétrocompatibilité v1 → v2

### **Phase 6 : Processeurs modulaires** (2-3 semaines)
1. Processeurs conscients des groupes/layers
2. Désactivation par couche supérieure
3. Chaînage conditionnel
4. Migration progressive depuis v1

## Points à trancher

### 1. **Ordre des couches** : Base → Machine → Accessoire ?
- **Proposition** : Hiérarchie fixe avec priorité ACCESSORY > MACHINE > BASE
- **Avantage** : Prédictible, facile à déboguer
- **Alternative** : Configuration dynamique

### 2. **Activation des groupes** : uniquement par compile flag ?
- **Proposition** : Compile flags pour optimisation, runtime pour flexibilité
- **Implémentation** : Pointeurs `NULL` pour groupes désactivés
- **Avantage** : Builds légers possibles

### 3. **Règles de fusion des métadonnées** : priorité max, ChanLayer max ?
- **Proposition** : Stratégies configurables par application
- **Exemples** : `PRIORITY_MAX`, `LAYER_HIERARCHY`, `UNION`
- **Validation** : Tests unitaires exhaustifs

### 4. **Désactivation des processeurs** : par nom identique ?
- **Proposition** : Namespace hiérarchique (`pto.control`, `pto.pump`)
- **Mécanisme** : Désactivation par couche supérieure
- **Avantage** : Extensibilité sans conflits

### 5. **Format de trame versionné ou non** ?
- **Proposition** : Header versionné avec backward compatibility
- **Migration** : Support temporaire v1/v2
- **Avantage** : Transition progressive possible

## Nouvelles propositions

### Canaux de branchement pour accessoires
```cpp
// Activation par compile flag
#define ENABLE_TRAILER
#define ENABLE_HYDRAULIC_PUMP

// Déclaration automatique
#ifdef ENABLE_TRAILER
ComBusChannel trailerChannels[] = {
    { .name = "trailer_brake", .layer = ChanLayer::REMOTE },
    { .name = "trailer_lights", .layer = ChanLayer::LOCAL },
    { .name = "trailer_angle", .layer = ChanLayer::SYSTEM },
};
#endif
```

### Initialisation dynamique avec pointeurs
```cpp
// Structure modulaire
struct ComBusGroup {
    const char* name;
    ComBusChannel* channels;      // nullptr si groupe désactivé
    uint8_t channelCount;
    bool (*initFn)(void* context);
};

// Initialisation conditionnelle
void combus_init_groups(ComBus* bus, uint32_t enabledGroupsMask) {
    for each group {
        if (enabledGroupsMask & group.mask) {
            group.initFn(bus);
            bus->groups[group.id] = &group;
        } else {
            bus->groups[group.id] = nullptr;  // Groupe désactivé
        }
    }
}
```

### Fusion avancée avec résolution de conflits
```cpp
// Stratégies de fusion
enum FusionStrategy {
    UNION,          // Tous les canaux conservés
    PRIORITY_MAX,   // Métadonnées avec priorité max
    LAYER_HIERARCHY // ACCESSORY > MACHINE > BASE
};

// Validation à la compilation
static_assert(combus_validate_layers(&baseLayer, &machineLayer),
              "Layers incompatibles");
```

## Évaluation des risques

| Risque | Impact | Probabilité | Mitigation |
|--------|--------|-------------|------------|
| Rétrocompatibilité brisée | Élevé | Moyenne | Migration progressive, adapters |
| Performance dégradée | Moyen | Faible | Benchmarks réguliers, optimisation incrémentale |
| Complexité accrue | Moyen | Moyenne | Documentation rigoureuse, simplification continue |
| Bugs de fusion | Élevé | Moyenne | Validation exhaustive, tests unitaires complets |

## Recommandations

### 1. **Approche incrémentale**
- Implémenter par petits blocs testables
- Valider chaque phase avant suivante
- Mesurer impact à chaque étape

### 2. **Documentation parallèle**
- Mettre à jour ce document au fur et à mesure
- Créer guide de migration
- Documenter API évolutive

### 3. **Validation rigoureuse**
- Tests unitaires pour chaque composant
- Benchmarks performance/mémoire
- Validation cross-platform

## Prochaines actions immédiates

1. **Finaliser cette version du document** et la valider
2. **Définir API ComBusGroup** (structures, compile flags)
3. **Prototyper fusion simple** (2 layers basiques)
4. **Évaluer impact mémoire** sur build actuel
5. **Planifier Phase 1** (détails techniques, estimations)

---

**Conclusion** : Le rework ComBus a créé une base solide pour v2. La feuille de route est réaliste et les risques sont gérables. La prochaine étape logique est la **Phase 1 : Groupes thématiques**.

**Dernière mise à jour** : Juillet 2026 (après rework ComBus)