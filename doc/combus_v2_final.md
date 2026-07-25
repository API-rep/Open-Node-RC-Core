# ComBus v2 — Architecture et Implémentation

## 1. Intention

**Problème** : Le ComBus actuel est un espace plat où tous les canaux sont mélangés, sans modularité ni organisation.

**Solution** : Transformer le ComBus en une **plateforme modulaire et extensible** qui permet :
- La **composition** par superposition de couches
- L'**organisation** par groupes thématiques  
- L'**extensibilité** plug-and-play pour accessoires
- L'**optimisation** par activation sélective

**Principe** : Conserver la simplicité du modèle actuel tout en ajoutant la modularité nécessaire pour l'évolution future.

## 2. Moyens

### 2.1. ChanLayer — Niveaux d'accès (✅ IMPLÉMENTÉ)
```
SYSTEM    → Données privées firmware (intra-device)
LOCAL     → Fonctionnalités véhicule (intra-node)  
REMOTE    → Commandes universelles (inter-node)
```

**Règle d'accès** : `callerLayer >= channelLayer` (SYSTEM ≥ LOCAL ≥ REMOTE)

### 2.2. Groupes thématiques — Organisation par domaine
```cpp
// Activation par compile flag
#define COMBUS_ENABLE_MOTION
#define COMBUS_ENABLE_LIGHT
#define COMBUS_ENABLE_SOUND

// Structure modulaire
struct ComBusGroup {
    const char* name;           // "Motion", "Light", "Sound"
    ComBusChannel* channels;    // nullptr si groupe désactivé
    uint8_t channelCount;
    uint32_t mask;              // Bit pour activation
};
```

**Groupes standards** :
- **Motion** : throttle, steering, brake, direction, PTO
- **Light** : headlight, blinkers, hazard, work_light
- **Core** : runlevel, vbat, error_flags, mode
- **Sound** : horn, melody, volume
- **Telemetry** : GPS, speed, heading
- **Battery** : voltage, current, temperature

### 2.3. Superposition de couches — Composition
```
ComBus Final
├── Layer ACCESSORY (dernier) ← Branchements externes
├── Layer MACHINE (milieu) ← Configuration spécifique
└── Layer BASE (premier) ← Configuration générique
```

**Règles de fusion** :
1. **Union** : Tous les canaux conservés
2. **Unicité** : Un ID unique par canal
3. **Compatibilité** : Pas de changement de type
4. **Métadonnées** : Fusion (priorité max, ChanLayer max)

### 2.4. Métadonnées transport — Optimisation
```cpp
struct ComBusMetadata {
    ChanLayer layer;     // SYSTEM/LOCAL/REMOTE
    uint8_t priority;    // 0-7 (QoS)
    Direction dir;       // RX/TX/RW
    LayerOrigin origin;  // BASE/MACHINE/ACCESSORY
};
```

### 2.5. Processeurs modulaires — Comportement
```cpp
struct CbProcV2 {
    ComBusGroupID targetGroup;  // Motion, Light, etc.
    ComBusLayer targetLayer;    // BASE, MACHINE, ACCESSORY
    CbProcFn fn;
    bool enabled;               // Désactivation par couche supérieure
};
```

## 3. Structures

### 3.1. Architecture globale
```cpp
// ComBus v2 — Structure unique
struct ComBusV2 {
    // Métadonnées globales
    uint8_t version;
    uint8_t layerCount;
    uint16_t totalSize;
    
    // Groupes activables
    ComBusGroup* groups[MAX_GROUPS];
    
    // Canaux fusionnés
    ComBusChannel* channels;
    uint16_t channelCount;
    
    // Processeurs
    CbProcV2* processors;
    uint16_t procCount;
};
```

### 3.2. Initialisation conditionnelle
```cpp
// Configuration par compile flags
#if defined(COMBUS_ENABLE_MOTION)
    static ComBusGroup motionGroup = {
        .name = "Motion",
        .channels = motionChannels,
        .channelCount = MOTION_CH_COUNT,
        .mask = 1 << GROUP_MOTION
    };
#endif

// Initialisation dynamique
void combus_init(ComBusV2* bus, uint32_t enabledGroupsMask) {
    for (int i = 0; i < MAX_GROUPS; i++) {
        if (groups[i] && (enabledGroupsMask & groups[i]->mask)) {
            bus->groups[i] = groups[i];
        } else {
            bus->groups[i] = nullptr;  // Groupe désactivé
        }
    }
}
```

### 3.3. Fusion des couches
```cpp
// Chargement séquentiel
ComBusV2* combus_build() {
    ComBusV2* bus = combus_create_empty();
    
    // 1. Fusion BASE (core générique)
    combus_fuse_layer(bus, &baseLayer);
    
    // 2. Fusion MACHINE (spécifique)
    combus_fuse_layer(bus, &machineLayer);
    
    // 3. Fusion ACCESSORY (branchements)
    combus_fuse_layer(bus, &accessoryLayer);
    
    return bus;
}

// Stratégies de fusion configurables
enum FusionStrategy {
    UNION,           // Tous les canaux conservés
    PRIORITY_MAX,    // Métadonnées avec priorité max
    LAYER_HIERARCHY  // ACCESSORY > MACHINE > BASE
};
```

### 3.4. Canaux de branchement (accessoires)
```cpp
// Configuration accessoire
#define ENABLE_TRAILER
#define ENABLE_HYDRAULIC_PUMP

// Déclaration automatique
#ifdef ENABLE_TRAILER
static ComBusChannel trailerChannels[] = {
    { .name = "trailer_brake", .metadata = { .layer = REMOTE } },
    { .name = "trailer_lights", .metadata = { .layer = LOCAL } },
    { .name = "trailer_angle", .metadata = { .layer = SYSTEM } },
};
#endif
```

## 4. Implémentation par phases

### Phase 1 — Groupes thématiques (2-3 semaines)
**Objectif** : Organisation modulaire par domaine
- [ ] Définir API `ComBusGroup`
- [ ] Implémenter compile flags
- [ ] Tester avec Motion + Light
- [ ] Mesurer impact mémoire

### Phase 2 — Superposition de couches (3-4 semaines)
**Objectif** : Composition par fusion
- [ ] API `ComBusLayer` (Base/Machine/Accessory)
- [ ] Mécanisme de fusion
- [ ] Règles de résolution de conflits
- [ ] Validation à la compilation

### Phase 3 — Accessoires et branchements (4-5 semaines)
**Objectif** : Extensibilité plug-and-play
- [ ] Système de configuration accessoire
- [ ] Canaux de branchement automatiques
- [ ] Communication intégrée (UART/CAN/ESP-NOW)
- [ ] Hot-plug support

### Phase 4 — Métadonnées transport (2-3 semaines)
**Objectif** : Optimisation réseau
- [ ] Priorité (QoS) pour transport
- [ ] Direction (RX/TX/RW)
- [ ] Noms réservés et plages d'ID
- [ ] Validation sémantique

### Phase 5 — Transport adaptatif (3-4 semaines)
**Objectif** : Communication efficace
- [ ] Encodage/décodage avec métadonnées
- [ ] Compression sélective par priorité
- [ ] Frames versionnées
- [ ] Rétrocompatibilité v1→v2

### Phase 6 — Processeurs modulaires (2-3 semaines)
**Objectif** : Comportement configurable
- [ ] Processeurs conscients des groupes/layers
- [ ] Désactivation par couche supérieure
- [ ] Chaînage conditionnel
- [ ] Migration progressive depuis v1

## 5. Décisions critiques

### 5.1. Ordre des couches
**Proposition** : Hiérarchie fixe `ACCESSORY > MACHINE > BASE`
**Avantage** : Prédictible, facile à déboguer
**Alternative** : Configuration dynamique (plus flexible mais complexe)

### 5.2. Activation des groupes
**Proposition** : Compile flags pour optimisation + runtime pour flexibilité
**Implémentation** : Pointeurs `NULL` pour groupes désactivés
**Avantage** : Builds légers possibles, activation dynamique optionnelle

### 5.3. Règles de fusion
**Proposition** : Stratégies configurables par application
**Exemples** : `PRIORITY_MAX`, `LAYER_HIERARCHY`, `UNION`
**Validation** : Tests unitaires exhaustifs

### 5.4. Désactivation processeurs
**Proposition** : Namespace hiérarchique (`pto.control`, `pto.pump`)
**Mécanisme** : Désactivation par couche supérieure
**Avantage** : Extensibilité sans conflits

### 5.5. Format de trame
**Proposition** : Header versionné avec backward compatibility
**Migration** : Support temporaire v1/v2
**Avantage** : Transition progressive possible

## 6. Risques et mitigation

| Risque | Impact | Probabilité | Mitigation |
|--------|--------|-------------|------------|
| Rétrocompatibilité | Élevé | Moyenne | Migration progressive, adapters |
| Performance | Moyen | Faible | Benchmarks réguliers |
| Complexité | Moyen | Moyenne | Documentation rigoureuse |
| Bugs fusion | Élevé | Moyenne | Validation exhaustive |

## 7. Validation

### 7.1. Tests unitaires
- [ ] Fusion de couches basiques
- [ ] Résolution de conflits
- [ ] Activation/désactivation groupes
- [ ] Règles d'accès ChanLayer

### 7.2. Benchmarks
- [ ] Impact mémoire (avec/sans groupes)
- [ ] Temps d'initialisation
- [ ] Performance runtime

### 7.3. Intégration
- [ ] Migration progressive depuis v1
- [ ] Compatibilité avec code existant
- [ ] Documentation API évolutive

## 8. Prochaines actions

### Immédiates (semaine 1)
1. **Valider cette architecture** avec l'équipe
2. **Définir API ComBusGroup** (structures détaillées)
3. **Prototyper fusion simple** (2 layers)

### Court terme (semaines 2-4)
4. **Implémenter Phase 1** (groupes thématiques)
5. **Évaluer impact** sur builds actuels
6. **Documenter migration** depuis v1

### Moyen terme (semaines 5-12)
7. **Implémenter Phases 2-3** (superposition + accessoires)
8. **Valider avec cas réels** (tracteur + accessoires)
9. **Optimiser performance** basée sur retours

## 9. Conclusion

**État actuel** : ✅ Rework ComBus terminé (ChanLayer implémenté, ownership supprimé)
**Prochaine étape** : 🚀 Phase 1 — Groupes thématiques

**Avantages attendus** :
- ✅ **Modularité** : Builds sur mesure par domaine
- ✅ **Extensibilité** : Accessoires plug-and-play
- ✅ **Maintenance** : Isolation par couches
- ✅ **Performance** : Code mort éliminé
- ✅ **Collaboration** : Développement parallèle par couches

**Document créé** : Juillet 2026 (après rework ComBus)
**Prochaine révision** : Après validation Phase 1