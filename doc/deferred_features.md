# Deferred Features — Winter 2026 Backlog

## 1. [DEPRECATED] SimDev — ComBus inertia processor

**Objectif initial :** Extraire la FSM d'inertie ESC (actuellement dans `sound_module/main.cpp`) vers un processor dédié `SimDev`, séparant comportement et binding matériel.

**Statut :** Deprecated / Superseded

Le concept a depuis été remplacé par une architecture plus générale. Cette proposition ne doit plus servir de base à de nouveaux développements.

**Actions restantes :**

* Nettoyer les derniers reliquats `SimDev` dans les structures et configurations.
* Refactorer les derniers fragments de logique hérités de l'ancienne implémentation ESC.
* Vérifier qu'aucun comportement actif ne dépend encore du design `SimDev`.
* Mettre à jour la documentation pour supprimer `SimDev` des évolutions futures.

---

## 2. [DEPRECATED] `TRACTION_BUS`

`TRACTION_BUS` a initialement été introduit pour regrouper les états de traction dans une valeur unique :

```text
bit15   : BRAKE
bit14   : FWD
bit13-0 : magnitude
```

Les travaux sur les SimProc ont montré que cette approche mélangeait une grandeur analogique et des états métier.

`TRACTION_BUS` est donc déprécié. Les états discrets (`BRAKE`, validations, modes, etc.) migrent vers des canaux combus booléens dédiés.

Le principe `DIR + magnitude` est toutefois conservé et généralisé par ComBus v2 pour l'ensemble des canaux analogiques (voir section combus v2).

Conséquence : certains états aujourd'hui locaux devront être reclassés selon leur portée réelle. GEAR_SHIFTING, actuellement machine-local, devra notamment être promu en canal inter-nœud afin que le nœud son puisse l'exploiter directement, supprimant ainsi la détection heuristique actuellement présente dans la FSM son.

Voir @todo existant dans combus_ids.h → GEAR_SHIFTING.

---

## 3. DynConfig structure implementation

**Objectif :** Finaliser l'architecture DynConfig conformément au document de référence.

**Travaux identifiés :**

* Scinder les structures existantes en trois sous-ensembles :

  * `config` : configuration persistante ;
  * `dynconfig` : configuration dynamique ;
  * `state` : état d'exécution.
* Généraliser l'utilisation des variantes allégées de `dynconfig` afin de réduire l'empreinte RAM :

  * suppression des métadonnées inutiles à l'exécution ;
  * conservation des seules informations nécessaires au runtime.
* Adapter les consommateurs existants :

  * ComBus processors ;
  * autres modules utilisant directement les structures de configuration.
* Vérifier l'alignement entre la documentation DynConfig et les implémentations actuelles.

**Statut :** Roadmap / Technical debt

Cette évolution vise principalement à clarifier la séparation entre configuration et état runtime tout en réduisant la consommation mémoire.

---

## 4. ComBus v2 — architecture en couches

L'usage du ComBus a progressivement dépassé son rôle initial de simple liaison entre télécommande et machine.

ComBus v2 formalise les différents niveaux d'utilisation déjà introduits dans l'architecture actuelle :

```text
Inter-node    ← échanges entre nœuds distincts
               (télécommande ↔ machine, machine ↔ son)

Inter-device  ← échanges entre cartes d'un même système
               (carte mère ↔ extensions)

Intra-device  ← données locales partagées au sein d'un firmware
               (VBAT, RunLevel, états simulés, valeurs intermédiaires...)
```

**Règle dure :** chaque canal appartient explicitement à l'une de ces couches,
qui détermine sa portée et ses mécanismes de propagation.

### Conséquences

Cette séparation devra se refléter dans les structures ComBus, le transport des
trames et la configuration :

* les données inter-node définiront les échanges externes ;
* les données inter-device structureront les extensions matérielles ;
* les données intra-device resteront privées au firmware.

Aucune décision n'est prise à ce stade concernant un typage plus fort des
canaux. La simplicité du ComBus reste une priorité ; l'expérience de
`TRACTION_BUS` montre qu'une abstraction trop riche devient rapidement
contre-productive.

### Typed channel groups (piste d'évolution)

Une évolution possible consisterait à regrouper les canaux par domaines
fonctionnels au lieu de les maintenir dans un espace plat unique :

```cpp
MotionComBus motion;
LightComBus  light;
CoreComBus   core;
```

Chaque domaine disposerait de ses propres identifiants et de son propre
stockage, permettant à un nœud de n'embarquer que les groupes réellement
nécessaires.

Les bénéfices potentiels seraient :

* groupes fonctionnels réellement optionnels ;
* dépendances plus explicites entre modules ;
* disparition des emplacements inutilisés dans certaines configurations.

Cette approche impliquerait toutefois une évolution du protocole de transport
afin de supporter plusieurs types de trames ou un mécanisme de multiplexage.

Aucune décision n'est prise à ce stade. Cette piste ne sera réévaluée que si
plusieurs modules indépendants nécessitent des ensembles de canaux réellement
distincts.

---

## 5. ComBus transport v2 — architecture dual-frame

### Objectif

Séparer les échanges temps réel des données de configuration à faible
fréquence.

Le modèle actuel transporte l'ensemble des informations dans une trame unique
émise à fréquence constante. Cette approche reste adaptée tant que le volume de
données demeure limité.

Une évolution future pourrait introduire deux classes de trafic :

```text
Control frame   ← commandes temps réel
                  (sticks, boutons, états critiques)

Config frame    ← paramètres peu fréquents
                  (luminosité, trims, réglages persistants...)
```

Cette approche est comparable à la séparation PDO/SDO utilisée dans CANopen.

### Bénéfices potentiels

* réduction de la charge sur les trames temps réel ;
* transport de paramètres occasionnels sans augmenter la taille des trames de contrôle ;
* meilleure évolutivité pour les futurs nœuds spécialisés.

### Implémentation envisagée

La solution la plus simple consisterait à utiliser un multiplexage périodique :

```text
Frame 1  → Control
Frame 2  → Control
Frame 3  → Control
Frame 4  → Config
```

Cette approche pourrait être réalisée sans modification majeure du protocole
actuel, via un compteur de trames côté émission.

### Statut

Aucun besoin concret ne justifie actuellement cette complexité.

Cette évolution ne sera envisagée qu'en présence d'un cas d'usage réel,
par exemple le transport de paramètres utilisateur vers des nœuds spécialisés
sans surcharge permanente des trames de contrôle.

---

## 6. SoundDevice — moteur audio table-driven

**Objectif :** remplacer le monolithe `main.cpp` (≈1600 lignes, `pulseWidth[]`, `#ifdef`, logique dispersée) par une architecture déclarative basée sur des descripteurs de périphériques.

**Décision arrêtée :**

```cpp
struct SoundCfgHdr { int8_t gateDevID; };  // offset 0 obligatoire

struct SoundDevice {
    const int8_t                   ID;
    const char*                    infoName;
    DevUsage                       usage;
    std::optional<AnalogComBusID>  analogChan;
    std::optional<DigitalComBusID> digitalChan;
    SoundBehaviorFn                behaviorFn;
    const void*                    cfg;
    SoundDevState*                 state;
};
```

Chaque comportement audio devient un `SoundDevice` indépendant, décrit par sa configuration, son état et sa fonction de comportement.

Cette approche vise à :

* supprimer les logiques conditionnelles dispersées ;
* rendre les configurations extensibles ;
* faciliter l'ajout de nouveaux comportements sans modifier le moteur central ;
* préparer l'introduction d'un mécanisme équivalent pour la lumière.

---

## 7. Light module v2 — architecture table-driven

**Objectif :** remplacer `light_core.cpp` (port legacy) par une architecture déclarative similaire à celle du projet.

**Décisions arrêtées :**

* Une config `LedDescriptor` par canal avec séparation configuration (`constexpr`) / état runtime ;
* tableaux dimensionnés par enum (même principe que `DcDevID`) ;
* sous-configurations comportementales optionnelles (`BeaconCfg`, `XenonCfg`, `IndicatorCfg`) ;
* traitement générique par itération de table dans `light_core_update()` ;
* suppression des logiques centralisées basées sur `switch(lightsState)`.

### Gestion des RunLevels

Certaines fonctions lumineuses dépendent à la fois :

* d'une commande utilisateur (ComBus) ;
* de l'état global de la machine (`RunLevel`).

Chaque `LedDescriptor` pourra définir un masque d'activation dont chaque bit représente un RunLevel autorisé.

L'interprétation de ce masque sera réalisée par un CBProc dédié chargé de résoudre les conditions d'activation avant le traitement des effets lumineux.

**Prérequis :** refactorisation de `statusLED` en classe dédiée.


---

## 8. SwitchDevice → EnvCfg

**Objectif :** séparer la description matérielle des entrées de leur rôle fonctionnel dans la machine.

**Décision arrêtée :**

```cpp
struct SwitchPortCfg {
    const char* infoName;
    int8_t      pin;
    bool        pullUp;
    bool        activeHigh;
    uint16_t    debounceMs;
};

struct SwitchDevice {
    const int8_t ID;
    const char*  infoName;
    DevUsage     usage;
    int8_t       parentID = -1;
};
```

`SwitchPortCfg` décrit exclusivement le matériel :

* broche ;
* polarité ;
* résistance de tirage ;
* anti-rebond.

`SwitchDevice` décrit exclusivement l'intégration dans l'environnement machine :

* rôle fonctionnel ;
* rattachement éventuel à un autre périphérique ;
* usage applicatif.

Cette séparation permet de réutiliser une même carte sur plusieurs machines sans dupliquer les configurations matérielles.

**Prérequis :** présence d'au moins deux modules switch simultanément afin de justifier l'infrastructure.


---

## 9. Unification des simulations motion / sound

**Objectif :** supprimer les modèles dynamiques parallèles actuellement présents dans les environnements machine et son.

### Constat

Les environnements motion et sound maintiennent actuellement leurs propres simulations des états moteur :

* régime moteur ;
* inerties ;
* rapports de transmission ;
* changements de vitesse ;
* états dynamiques associés.

Cette duplication augmente la charge de maintenance et crée un risque de divergence entre le comportement physique du véhicule et son rendu sonore.

### Orientation retenue

À terme, un seul environnement devra être responsable du calcul des états dynamiques.

Cette responsabilité revient naturellement à la carte principale du véhicule, déjà en charge de la simulation motion et des commandes moteur.

Le nœud son deviendra principalement un consommateur de données ComBus :

```text
Machine
  └─ SimProc
       ├─ RPM
       ├─ Gear
       ├─ Speed
       └─ Engine state
              │
              ▼
           ComBus

Sound
  └─ Audio engine
       └─ génération sonore
          à partir des états reçus
```

### Conséquences

* suppression progressive des simulations redondantes côté son ;
* partage d'un modèle dynamique unique ;
* réduction de la charge CPU du nœud son ;
* cohérence parfaite entre comportement physique et rendu sonore.

Cette évolution constitue l'une des dernières refontes majeures avant la convergence complète des environnements Machine et Sound.

**Prérequis :**

* saison 2026 terminée ;
* profils motion stabilisés ;
* extension du ComBus pour transporter les états nécessaires au moteur audio.


---

## Annexe — idées et améliorations différées

### ExtPort conflict detection

Pool `constexpr` de ports dans la configuration board.

```cpp
port_claim(pin, "owner");
```

Détection des doublons à l'initialisation avec assertion explicite.

**Prérequis :** ≥ 2 modules output actifs simultanément.

---

### Sound transport cap cross-check

Validation compile-time de la compatibilité entre débit ComBus et capacité de réception du nœud son.

```cpp
static_assert(ComBusUartTxHz <= SoundRxMaxHz);
```

Nécessite l'exposition de `SoundRxMaxHz` et `SoundRxMaxBaud` en `constexpr`.

---

### Debug serial opt-out

Option de compilation :

```text
-D NO_DEBUG_SERIAL
```

Suppression complète de :

```cpp
Serial.begin(...)
sys_log_info(...)
```

pour les plateformes mono-UART.

**Prérequis :** utilisation réelle sur une carte ne disposant que d'un seul UART.

---

### ComBus bidirectionnel

Ajout d'un helper :

```cpp
combus_link_init()
```

encapsulant :

```cpp
combus_tx_init();
combus_rx_init();
```

pour les futurs nœuds bidirectionnels.

**Prérequis :** premier cas d'usage concret.

---

### Preset hydraulique lent

Ajout d'un preset :

```cpp
kMotion_Hydraulic_Slow
```

dans `simulation_presets.h`.

Basé sur :

```cpp
MotionRamp {
    .rampTimeMs = 200u,
    ...
}
```

À créer lors de l'intégration du premier actionneur hydraulique dans la chaîne Motion.

