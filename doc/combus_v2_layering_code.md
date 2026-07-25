# ComBus v2 — Code d'implémentation du Layering

## 1. Architecture à 3 pointeurs

### Structure principale
```cpp
// combus_struct.h
struct ComBusConfig {
    const ComBusChannel* channels;
    uint16_t channel_count;
    const char* source_name;  // "core_remote", "local_machine", "system"
};

struct ComBusV2 {
    // 3 pointeurs vers configurations sources
    const ComBusConfig* core_remote;   // Configuration générique type machine
    const ComBusConfig* local_machine;  // Configuration spécifique machine
    const ComBusConfig* system;        // Configuration firmware carte
    
    // Registre fusionné (runtime) - résultat final
    ComBusChannel* merged_channels;
    uint16_t merged_count;
    
    // Métadonnées de fusion (pour debug/tracing)
    struct {
        uint8_t source_layer[256];  // Pour chaque canal, layer d'origine
        const char* source_name[256]; // Nom de la source d'origine
    } fusion_meta;
    
    // Flags d'état
    bool initialized;
    bool fusion_complete;
};
```

### Initialisation séquentielle
```cpp
// combus_init.cpp
ComBusV2* combus_v2_init() {
    ComBusV2* bus = (ComBusV2*)malloc(sizeof(ComBusV2));
    if (!bus) return nullptr;
    
    memset(bus, 0, sizeof(ComBusV2));
    
    // 1. Chargement REMOTE (core générique)
    bus->core_remote = &g_core_remote_config;
    
    // 2. Chargement LOCAL (machine spécifique)
    bus->local_machine = &g_local_machine_config;
    
    // 3. Chargement SYSTEM (firmware carte)
    bus->system = &g_system_config;
    
    // 4. Fusion séquentielle
    combus_perform_fusion(bus);
    
    bus->initialized = true;
    return bus;
}
```

## 2. Style de codage du projet

### Conventions à respecter
1. **Nommage** : `snake_case` pour variables/fonctions, `PascalCase` pour types
2. **Const-correctness** : `const` partout où possible
3. **Documentation Doxygen** : `/** @brief ... */` pour fonctions publiques
4. **Fichiers séparés** : Un fichier par responsabilité
5. **Validation compile-time** : `static_assert` pour invariants

### Exemple d'implémentation
```cpp
// combus_fusion.cpp
/**
 * @brief Fusionne les 3 layers en un registre unique
 * @details Algorithme séquentiel : REMOTE → LOCAL → SYSTEM
 *          Conflits : erreur fatale à l'initialisation
 * @param bus Instance ComBusV2 à fusionner
 * @return true si fusion réussie, false en cas de conflit
 */
bool combus_perform_fusion(ComBusV2* bus) {
    if (!bus || bus->fusion_complete) return false;
    
    // Tableau temporaire pour fusion
    ComBusChannel temp_channels[256];
    uint16_t temp_count = 0;
    
    // Fusion séquentielle
    bool success = true;
    
    // 1. Fusion REMOTE
    success &= combus_fuse_layer(bus, bus->core_remote, 
                                 ChanLayer::REMOTE, temp_channels, &temp_count);
    
    // 2. Fusion LOCAL (inclut REMOTE)
    success &= combus_fuse_layer(bus, bus->local_machine, 
                                 ChanLayer::LOCAL, temp_channels, &temp_count);
    
    // 3. Fusion SYSTEM (inclut LOCAL + REMOTE)
    success &= combus_fuse_layer(bus, bus->system, 
                                 ChanLayer::SYSTEM, temp_channels, &temp_count);
    
    if (!success) {
        sys_log_error("[COMBUS] Fusion failed due to conflicts");
        return false;
    }
    
    // Copie finale
    bus->merged_channels = (ComBusChannel*)malloc(temp_count * sizeof(ComBusChannel));
    if (!bus->merged_channels) return false;
    
    memcpy(bus->merged_channels, temp_channels, temp_count * sizeof(ComBusChannel));
    bus->merged_count = temp_count;
    bus->fusion_complete = true;
    
    sys_log_info("[COMBUS] Fusion complete: %d channels", temp_count);
    return true;
}
```

## 3. Mécanisme d'enregistrement

### Approche dynamique avec enum
```cpp
// combus_layers.h
enum ComBusLayerEnum {
    LAYER_REMOTE = 0,
    LAYER_LOCAL  = 1,
    LAYER_SYSTEM = 2,
    LAYER_COUNT  = 3  // n_max de l'enum
};

// Tableau de configurations par layer
static const ComBusConfig* g_layer_configs[LAYER_COUNT] = {NULL};

// Enregistrement au combus_init()
void combus_register_layer(ComBusLayerEnum layer, const ComBusConfig* config) {
    if (layer >= LAYER_COUNT) return;
    g_layer_configs[layer] = config;
}
```

### Initialisation avec bouclage
```cpp
void combus_init_layers() {
    // Bouclage sur tous les layers définis
    for (int i = 0; i < LAYER_COUNT; i++) {
        if (g_layer_configs[i]) {
            sys_log_info("[COMBUS] Registering layer %d: %s", 
                        i, g_layer_configs[i]->source_name);
        }
    }
}
```

## 4. Gestion des conflits - VERBOTEN (bloquant)

### Algorithme de détection
```cpp
// combus_conflict.cpp
/**
 * @brief Détecte les conflits entre layers
 * @details Un conflit = même ID dans plusieurs layers
 *          Stratégie : REMOTE < LOCAL < SYSTEM (priorité croissante)
 *          Conflit = erreur fatale
 */
bool combus_detect_conflicts(const ComBusChannel* existing, uint16_t existing_count,
                             const ComBusConfig* new_layer, ChanLayer new_layer_type) {
    
    for (uint16_t i = 0; i < new_layer->channel_count; i++) {
        const ComBusChannel* new_channel = &new_layer->channels[i];
        
        // Recherche du canal existant
        for (uint16_t j = 0; j < existing_count; j++) {
            const ComBusChannel* existing_channel = &existing[j];
            
            if (existing_channel->id == new_channel->id) {
                // CONFLIT DÉTECTÉ
                sys_log_error("[COMBUS] Conflict detected: channel 0x%04X", 
                            new_channel->id);
                sys_log_error("[COMBUS]   Existing: layer=%d, name=%s",
                            existing_channel->layer, existing_channel->name);
                sys_log_error("[COMBUS]   New: layer=%d, name=%s",
                            new_layer_type, new_channel->name);
                
                // Stratégie VERBOTEN : erreur fatale
                return false;
            }
        }
    }
    
    return true;
}
```

### Ordre d'empilage validé
```
REMOTE seul
LOCAL = LOCAL + REMOTE (vérification conflits)
SYSTEM = SYSTEM + LOCAL + REMOTE (vérification conflits)
```

## 5. Énumérations séparées par layer

### Structure recommandée
```cpp
// Définition séparée par layer
namespace RemoteChannels {
    enum Enum {
        THROTTLE     = 0x0001,
        STEERING     = 0x0002,
        BRAKE        = 0x0003,
        // ... autres canaux REMOTE
    };
}

namespace LocalChannels {
    enum Enum {
        GEAR         = 0x4001,
        RPM          = 0x4002,
        BATTERY      = 0x4003,
        // ... autres canaux LOCAL
    };
}

namespace SystemChannels {
    enum Enum {
        RUNLEVEL     = 0x8001,
        ERROR_FLAGS  = 0x8002,
        DEBUG_DATA   = 0x8003,
        // ... autres canaux SYSTEM
    };
}
```

### Configuration par layer
```cpp
// Fichier séparé par layer
// config_remote.cpp
static const ComBusChannel g_remote_channels[] = {
    { .id = RemoteChannels::THROTTLE, .layer = ChanLayer::REMOTE, .name = "throttle" },
    { .id = RemoteChannels::STEERING, .layer = ChanLayer::REMOTE, .name = "steering" },
    // ...
};

const ComBusConfig g_core_remote_config = {
    .channels = g_remote_channels,
    .channel_count = sizeof(g_remote_channels) / sizeof(ComBusChannel),
    .source_name = "core_remote"
};
```

## 6. Points d'attention résolus

### A. Pas trois copies - Fusion en une seule structure
**Solution** : Registre fusionné unique, pointeurs vers configs originales
```cpp
// Après fusion
bus->merged_channels = [throttle, steering, brake, gear, rpm, runlevel...]
// Références aux configs originales conservées pour debug
```

### B. ComBusModule supprimé
**Décision** : Pas nécessaire pour MVP. Init séquentiel suffisant.
```cpp
// Simple et efficace
combus_init() {
    load_remote();
    load_local();   // Vérifie conflits avec remote
    load_system();  // Vérifie conflits avec remote+local
    perform_fusion();
}
```

### C. Hot Plug Module (plug and play)
**Approche** : Canaux prédéfinis + vérification à l'init
```cpp
bool combus_check_module_requirements(const char* module_name,
                                     const uint16_t* required_channels,
                                     uint8_t required_count) {
    
    for (uint8_t i = 0; i < required_count; i++) {
        if (!combus_channel_exists(bus, required_channels[i])) {
            sys_log_warn("[COMBUS] Module %s requires channel 0x%04X (missing)",
                        module_name, required_channels[i]);
            return false;
        }
    }
    
    return true;
}
```

## 7. ComBusPatch - Analyse Pros/Cons

### Structure ComBusPatch
```cpp
enum PatchOperation {
    PATCH_ADD,     // Ajouter un canal
    PATCH_MODIFY,  // Modifier métadonnées
    PATCH_REMOVE   // Supprimer un canal
};

struct ComBusPatch {
    PatchOperation op;
    uint16_t target_id;      // ID du canal cible (pour MODIFY/REMOVE)
    ComBusChannel channel;   // Données du canal (pour ADD/MODIFY)
    const char* reason;      // Justification du patch
    uint8_t priority;        // Priorité d'application (0-255)
};
```

### PROS (Avantages)
1. **Extensibilité** : Modification sans toucher aux configs de base
2. **Versioning** : Historique des modifications
3. **Audit trail** : Traçabilité des changements
4. **Hotfix** : Corrections runtime possibles
5. **Configuration conditionnelle** : Patches activables par flags

### CONS (Inconvénients)
1. **Complexité** : Système supplémentaire à maintenir
2. **Performance** : Application des patches à l'init
3. **Debug** : Plus difficile de comprendre l'état final
4. **Ordre d'application** : Gestion des dépendances entre patches
5. **Validation** : Vérification de cohérence plus complexe

### Scénarios d'utilisation
```cpp
// Exemple 1 : Correction de bug
static const ComBusPatch g_bugfix_patch = {
    .op = PATCH_MODIFY,
    .target_id = RemoteChannels::THROTTLE,
    .channel = { .deadband = 50 },  // Augmente deadband
    .reason = "Fix: throttle jitter on noisy input",
    .priority = 100
};

// Exemple 2 : Feature flag
#ifdef ENABLE_TRAILER
static const ComBusPatch g_trailer_patch = {
    .op = PATCH_ADD,
    .target_id = 0xC001,  // USER range
    .channel = { .id = 0xC001, .name = "trailer_brake", .layer = REMOTE },
    .reason = "Trailer support feature",
    .priority = 50
};
#endif
```

### Recommandation
**Pour MVP** : Éviter ComBusPatch, trop complexe
**Pour v2.1** : Implémenter si besoin réel apparaît
**Alternative** : Configs modulaires avec compile flags

## 8. Fichiers à créer/modifier

### Nouveaux fichiers
1. `src/core/system/combus/combus_v2_struct.h` → Structures principales
2. `src/core/system/combus/combus_v2_init.cpp` → Initialisation
3. `src/core/system/combus/combus_v2_fusion.cpp` → Algorithme de fusion
4. `src/core/system/combus/combus_v2_conflict.cpp` → Détection conflits
5. `include/struct/combus_v2_layers.h` → Énumérations par layer

### Modifications existantes
1. `include/struct/combus_struct.h` → Ajout types v2
2. `src/core/config/machines/*/combus/` → Nouvelle organisation
3. `src/core/system/combus/combus_access.cpp` → Adaptation règles d'accès

## 9. Plan d'implémentation

### Phase 1 - Structures de base (1 semaine)
1. Définir `ComBusV2`, `ComBusConfig`
2. Implémenter énumérations séparées par layer
3. Créer fichiers de configuration par layer

### Phase 2 - Fusion et conflits (1 semaine)
1. Implémenter `combus_perform_fusion()`
2. Implémenter détection conflits VERBOTEN
3. Tests unitaires fusion/conflits

### Phase 3 - Intégration (1 semaine)
1. Adapter `combus_access.cpp` pour v2
2. Mettre à jour configs existantes
3. Tests d'intégration complets

### Phase 4 - Validation (2-3 jours)
1. Benchmarks performance
2. Tests mémoire
3. Documentation finale

## 10. Questions ouvertes

1. **Validation compile-time** : Script Python pour vérifier conflits ?
2. **Hot-plug** : Nécessaire immédiatement ou plus tard ?
3. **Debug interface** : Outils pour inspecter la fusion ?
4. **Migration** : Comment migrer configs v1 → v2 ?

## 11. Conclusion

**Approche recommandée** :
1. ✅ 3 pointeurs + fusion runtime
2. ✅ Énumérations séparées par layer
3. ✅ Conflits VERBOTEN (erreur fatale)
4. ✅ Init séquentiel simple
5. ❌ ComBusPatch (trop complexe pour MVP)

**Prochaine étape** : Commencer Phase 1 - Structures de base