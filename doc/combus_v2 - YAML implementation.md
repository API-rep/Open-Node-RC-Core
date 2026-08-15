# ComBus v2 — YAML implementation

> Note d'architecture — référence pour le remplacement de la composition ComBus par `.inc` par une génération build-time à partir de définitions YAML.
>
> **Statut** : validée — `GO conditionnel`.
>
> Les trois conditions bloquantes (§16) doivent être levées avant le démarrage du prototype.

## 1. Intention

Le rework ComBus a mis en évidence une limite structurelle du mécanisme actuel basé sur des `.inc` et des `#include` conditionnels.

Les `.inc` servent aujourd'hui à déclarer les canaux, sélectionner leur présence selon les flags, assembler les contributions, déterminer indirectement l'ordre des IDs, alimenter les tableaux runtime et fournir au script `combus_md5.py` une matière à reconstruire.

Cette utilisation du préprocesseur crée un couplage croissant entre modules, umbrellas, configurations machine, flags PlatformIO, enums, tableaux et wire contract.

L'objectif de v2 est de déplacer la **composition déclarative** hors du préprocesseur C++.

Un générateur Python exécuté au build :
1. scanne le projet ;
2. découvre les définitions `.cb` et `.cbch` ;
3. les valide et les parse ;
4. récupère les flags réellement actifs de l'environnement PlatformIO ;
5. sélectionne les définitions actives ;
6. les ordonne de manière déterministe ;
7. génère les enums, tableaux et chaînes consommés par le C++ ;
8. place les artefacts dans `.pio/build/<env>/generated/combus/`.

Le runtime ComBus reste la base existante. Le changement porte principalement sur son **mécanisme de composition**.

## 2. Pourquoi changer de mécanisme ?

Les `.inc` sont utilisés comme système de composition alors qu'ils ne décrivent pas naturellement la notion de module ComBus.

Ils imposent notamment des fragments distincts pour les IDs et les tableaux, ainsi que des umbrellas qui portent les `#ifdef` et les `#include`.

Le problème n'est donc pas le préprocesseur en lui-même, mais son utilisation comme registre et assembleur architectural.

Le générateur fournit un lieu explicite pour :
- la découverte ;
- la validation ;
- la résolution des flags ;
- la composition ;
- la canonisation ;
- la génération ;
- le calcul du wire contract.

## 3. Format `.cb`

Le format retenu est :

```text
.cb
```

`cb` = ComBus definition.

Le concept `.cbpkg` est abandonné : plusieurs `.cb` localisés dans l'arborescence suffisent et évitent de recréer une notion artificielle de package/umbrella.

Un `.cb` peut contenir plusieurs canaux cohérents.

Exemple :

```text
src/core/system/vbat/
    vbat.cpp
    vbat.h
    vbat.cb
```

ou :

```text
src/machines/config/machines/volvo_A60H_bruder/
    combus/
        machine.cb
```

Le générateur découvre les fichiers indépendamment de leur localisation.

### Plusieurs channels par `.cb`

Un fichier peut déclarer plusieurs contributions :

```yaml
module: vbat

channels:
  - id: BATTERY_VOLTAGE
    type: analog
    scope: LOCAL
    theme: vbat

  - id: BATTERY_LOW
    type: digital
    scope: LOCAL
    theme: vbat

  - id: FAILSAFE_VBAT
    type: digital
    scope: REMOTE
    theme: failsafe
```

Le tri est **global** après parsing : l'ordre des fichiers n'a aucune valeur protocolaire. Le tri par fichier (avant fusion) créerait un ordre non-canonique et masquerait les divergences avec le legacy.

## 4. Modèle de données

Le modèle cible comprend notamment :

| Champ | Rôle |
|---|---|
| `id` | identifiant logique stable |
| `type` | `analog` / `digital` |
| `scope` | `REMOTE` / `LOCAL` / `SYSTEM` |
| `theme` | regroupement logique |
| `uplink` / `downlink` | direction de diffusion wire |
| `requires` | flags d'activation |
| `info_name` | nom humain/debug |
| `default` | valeur/polarité par défaut si applicable |
| `layer` | couche ComBus |

Les trois scopes `REMOTE`, `LOCAL` et `SYSTEM` font partie du modèle dès le départ.

La direction wire est orthogonale au scope :

```yaml
scope: REMOTE
uplink: true
downlink: false
```

Les règles précises de compatibilité scope/transport sont validées par le générateur.

Dans le modèle retenu, `FAILSAFE` est `REMOTE`.

## 5. Périmètre du `.cb`

Le `.cb` décrit **ce qui existe et comment c'est exposé** :
- channels ;
- type ;
- scope ;
- thème ;
- transport uplink/downlink ;
- flags d'activation ;
- métadonnées ;
- références nécessaires au câblage déclaratif.

Il ne contient pas la logique métier.

Par exemple, il peut déclarer `FAILSAFE_VBAT` et `requires: [HAS_VBAT_SENSE]`, mais ne décrit pas comment `vbat_is_low()` fonctionne.

## 6. Ce qui reste en C++

Le C++ reste responsable de :
- logique algorithmique des processors ;
- seuils et timeouts ;
- transformations ;
- calculs ;
- conditions métier complexes ;
- code inline ;
- macros/templates ;
- comportement runtime ;
- traitement des données.

Règle :

> Si c'est « qu'est-ce qui existe et comment c'est branché ? », `.cb` / `.cbch`.
>
> Si c'est « comment ça fonctionne ? », C++.

## 7. Définition des chaînes : `.cbch`

Les `CbProcChain` peuvent être décrites dans des fichiers :

```text
.cbch
```

Le `.cbch` contient le câblage déclaratif, pas l'implémentation des processors.

Exemple :

```yaml
chain: failsafe

processors:
  - reset_failsafe
  - failsafe_vbat
  - failsafe_combus
```

Il peut également déclarer le wiring statique, par exemple `in` / `out`.

Les processors et leur logique restent dans leurs fichiers C++.

### Validation

Chaque processor référencé par un `.cbch` doit exister côté C++.

Une référence inexistante doit faire échouer le build avec un diagnostic explicite.

Principe :

> `.cbch` décrit qui est branché sur qui ; C++ décrit ce que fait le processor.

## 8. Résolution des flags PlatformIO

C'est un invariant architectural majeur.

Le générateur ne doit jamais reparcourir `platformio.ini` ni réimplémenter :
- `extends` ;
- `build_flags` ;
- héritages ;
- options board ;
- autres mécanismes PlatformIO.

La source de vérité doit être l'environnement de build effectivement utilisé pour compiler le C++.

La cible est donc d'utiliser :

```python
env["CPPDEFINES"]
```

ou l'équivalent SCons au moment où les définitions sont effectivement résolues.

### Contrainte PlatformIO 6.x

Les essais précédents (étape 5h du rework Failsafe) ont montré que `CPPDEFINES` peut être **vide** dans un script `pre:`, car PlatformIO 6.x résout les `extends` et les `build_flags` **après** l'exécution des extra_scripts `pre:`.

**Solution retenue** : le générateur est exécuté en `post:` (ou hook équivalent) pour que `CPPDEFINES` soit peuplé au moment de l'exécution.

Le hack actuel de récursion `extends` dans `combus_md5.py` (via `GetProjectOption` + chaîne `extends`) est **transitoire** et ne doit pas devenir l'architecture définitive. Il sera supprimé dès que le générateur sera en `post:`.

Invariant :

> Le générateur doit recevoir les mêmes définitions que celles utilisées pour compiler le C++.

Un **test de cohérence** est ajouté : le générateur vérifie que `CPPDEFINES` est non-vide et contient au moins les flags structurels. Sinon, il échoue explicitement.

## 9. Pipeline du générateur

```text
DISCOVERY
    ↓
PARSE / VALIDATION
    ↓
FLAGS ACTIFS
    ↓
RESOLUTION
    ↓
CANONISATION
    ↓
GENERATION
    ↓
MD5 / WIRE CONTRACT
```

### Discovery

Recherche de :

```text
*.cb
*.cbch
```

Le scan est trié pour les diagnostics, mais son ordre n'est jamais utilisé comme ordre protocolaire.

### Parse / validation

Les fichiers sont chargés indépendamment des flags.

Une définition invalide provoque une erreur.

Le YAML reste un format de données : pas d'anchors, includes, multi-documents ou logique conditionnelle complexe dans la première version.

### Résolution

Une définition :

```yaml
requires:
  - HAS_VBAT_SENSE
```

est active uniquement si le flag est présent.

La sémantique initiale est :

```text
requires = AND
```

Pas de langage d'expressions OR/NOT dans la première version.

### Canonisation

Tous les items actifs sont regroupés globalement puis triés par :

```text
(scope, type, theme, id)
```

La clé intègre `SYSTEM` dès maintenant.

Le résultat ne dépend ni du chemin, ni de l'ordre de découverte, ni du filesystem, ni de l'OS.

## 10. Ordre des IDs / wire contract

C'est le principal invariant protocolaire.

Les valeurs numériques des enums peuvent être utilisées directement dans le wire contract. Changer leur ordre peut donc changer le protocole sans changer les noms.

### Détection de divergence

Avant migration :
1. analyser l'ordre legacy (depuis les `.inc` actuels) et déterminer son importance dans le code existant (notamment `wire_end` et toute autre dépendance d'ordre dans le C++) ;
2. capturer cet ordre legacy comme **référence** dans la logique du parser ;
3. produire l'ordre v2 (via tri canonique) ;
4. comparer ;
5. **échouer explicitement** en cas de divergence.

Le parser doit donc **préserver l'ordre wire_end** lorsqu'il reconstruit la liste des channels actifs : si un ordre legacy est documenté comme important (par exemple via `wire_end` ou tout autre invariant d'ordre dans le code C++), cet ordre est intégré comme clé de tri secondaire ou comme override explicite, et non comme simple tri canonique `(scope, type, theme, id)`.

Si le tri canonique diverge du legacy :
- conserver l'ordre legacy (mode compatibilité), ou
- accepter explicitement un breaking change protocolaire (avec bump de version majeur).

**Aucun changement silencieux d'ID numérique n'est accepté.**

Après tri, les valeurs sont assignées par index, séparément pour `analog` et `digital`, conformément au runtime actuel.

## 11. Génération C++

Le générateur doit produire des headers structurellement compatibles avec le runtime actuel :
- `enum class` ;
- valeurs numériques ;
- tableaux ;
- structures existantes ;
- `ChanLayer` ;
- conventions de taille ;
- interfaces consommées par le C++.

Le runtime ne doit pas savoir que les headers viennent d'un générateur.

**Séparation analog / digital** : la génération produit **deux pipelines parallèles** :

- **Pipeline analog** :
  - `enum class AnalogComBusID { ... }` ;
  - `kAnalogChannels[]` (tableau runtime) ;
  - `kAnalogChannelCount` ;
  - tout artefact spécifique aux channels `type: analog`.

- **Pipeline digital** :
  - `enum class DigitalComBusID { ... }` ;
  - `kDigitalChannels[]` (tableau runtime) ;
  - `kDigitalChannelCount` ;
  - tout artefact spécifique aux channels `type: digital`.

Les deux pipelines sont indépendants : un channel `analog` ne peut pas apparaître dans un enum ou tableau `digital`, et inversement. Cette séparation est imposée par le validateur du schéma (§18) et reflète le runtime actuel.

## 12. Artefacts générés

Les sorties vont dans :

```text
.pio/build/<env>/generated/combus/
```

Elles ne sont pas commités.

Le générateur doit :
- ajouter ce répertoire aux include paths ;
- déclarer les dépendances SCons sur les `.cb` / `.cbch` ;
- régénérer lorsqu'une définition change ;
- isoler chaque environnement.

Aucun artefact généré ne doit être placé dans `src/`.

## 13. MD5 / handshake

Le handshake doit représenter le **contrat ComBus généré**.

Principe :

```text
MD5(
    schema_version,
    generator_version,
    représentation canonique des flags pertinents,
    items actifs triés
)
```

La représentation canonique ne dépend ni :
- du chemin ;
- des commentaires ;
- de l'ordre YAML ;
- du formatage.

Une sérialisation JSON canonique à clés triées peut servir de représentation intermédiaire.

Le MD5 ne doit plus reconstruire indirectement le comportement du préprocesseur C++.

## 14. Compatibilité legacy

La migration vise d'abord une représentation structurellement identique :

```text
legacy .inc
    ↓
ordre / contenu de référence
    ↓
.cb
    ↓
générateur
    ↓
output équivalent
```

Le runtime, le protocole et les structures ne doivent pas changer simultanément avec le mécanisme de définition.

Une fois la migration validée, les `.inc` correspondants pourront être supprimés.

## 15. Prototype minimal

### Étape 1 — un thème

Commencer par un thème simple. Deux options viables :
- **`vbat`** : 2-3 channels réels, déjà bien compris, peu de dépendances.
- **`core`** : encore plus simple, permet de valider le mécanisme sans logique métier.

Recommandation : **`vbat`** en premier (réaliste), puis `core` (trivial) pour valider le déterminisme.

### Étape 2 — générateur

Implémenter :
- discovery ;
- parsing ;
- validation minimale ;
- flags ;
- `requires` ;
- tri canonique ;
- génération d'un header.

### Étape 3 — comparaison legacy

Comparer le résultat au ComBus actuel.

Critère :

```text
diff = zéro
```

ou diagnostic explicite de toute divergence.

### Étape 4 — PlatformIO

Intégrer dans un environnement de test et vérifier :
- génération ;
- include path ;
- compilation ;
- linkage.

### Étape 5 — déterminisme

Deux générations identiques doivent donner :

```text
output identique
MD5 identique
```

Tester également un ordre de scan volontairement différent (fichiers renommés, ordre de découverte modifié).

### Étape 6 — flags

Tester :
- flag absent ;
- flag présent ;
- flags hérités (`extends`).

Le résultat doit correspondre à ce que compile réellement le C++.

### Étape 7 — `.cbch`

Introduire une première chaîne et vérifier :
- processor C++ existant → accepté ;
- processor inexistant → build refusé avec diagnostic explicite.

## 16. Conditions bloquantes

Trois points sont bloquants avant migration :

1. **Ordre protocolaire** : compatibilité démontrée avec le legacy ou breaking change explicitement accepté. Le générateur doit **échouer explicitement** sur toute divergence, pas la masquer.
2. **Flags** : récupération des définitions réellement actives via `env["CPPDEFINES"]` en hook `post:`, sans réimplémentation de PlatformIO.
3. **`.cbch`** : validation des processors référencés au build (lookup par nom dans les `CbProc` enregistrés).

## 17. Validation du schéma

Une validation stricte est souhaitable dès le départ.

L'outil reste ouvert :
- JSON Schema ;
- Pydantic ;
- autre validateur strict.

L'objectif :
- champs connus ;
- types connus ;
- enums valides ;
- erreurs explicites ;
- aucune résolution silencieuse.

## 18. Règles de validation

| Cas | Traitement |
|---|---|
| ID en conflit dans un même espace applicable | erreur bloquante |
| scope inconnu | erreur |
| type inconnu | erreur |
| thème invalide | erreur |
| combinaison scope/type/theme interdite | erreur |
| processor `.cbch` inexistant | erreur |
| définition mal formée | erreur |
| conflit structurel | erreur |
| flag requis absent | item inactif |
| ordre ambigu | erreur |
| divergence ordre canonique / legacy | erreur (sauf override explicite) |

Principe général :

> Aucun conflit structurel ne doit être résolu silencieusement.

## 19. Impact sur le rework Failsafe

Le rework Failsafe a révélé une limite importante de l'architecture `.inc` : un module `core` (comme `failsafe`) devait connaître une valeur d'enum (`DigitalComBusID::FAILSAFE`) définie dans une configuration machine-specific (`combus_ids.h`). Ce couplage a empêché le câblage propre du reset processor.

Avec `.cb`, le canal est une définition déclarative. Le générateur attribue l'ID final et produit le vocabulaire nécessaire.

Le processor C++ ne dépend donc plus de la manière dont l'ID a été attribué.

Cela permet de conserver :
- `FAILSAFE` dans le ComBus ;
- processors `CbProc` normaux ;
- `CbChain` standard ;
- logique métier dans les modules ;
- câblage déclaratif (`.cbch`).

Dans le modèle retenu, `FAILSAFE` est `REMOTE`.

## 20. Cohérence avec l'architecture Failsafe

Le modèle Failsafe reste compatible avec le générateur :

```text
module propriétaire
      │
      ├── réarme FAILSAFE_X
      ▼
FAILSAFE_X
      │
      ▼
processor C++
      │
      ▼
FAILSAFE
```

Chaque `FAILSAFE_X` constitue une preuve de vie cyclique :
- le module propriétaire le réarme lorsqu'il fonctionne ;
- le processor le consomme ;
- l'absence de réarmement laisse le canal dans son état de fault par défaut.

La logique métier reste en C++ ; `.cb` et `.cbch` décrivent les éléments et leur câblage.

## 21. Runtime

Le runtime existant reste la base :
- `ComBus` ;
- `CbProc` ;
- `CbChain` ;
- `proc_chain_update()` ;
- enums ;
- tableaux ;
- `ChanLayer` ;
- protocole existant.

Le générateur doit produire ce que cette infrastructure attend.

Le but est de simplifier la définition, pas de remplacer le moteur ComBus.

## 22. Roadmap

### Phase A — preuve
- schéma minimal `.cb` ;
- parser ;
- résolution des flags ;
- génération d'un thème ;
- comparaison legacy ;
- déterminisme ;
- MD5.

### Phase B — intégration
- intégration PlatformIO (hook `post:`) ;
- génération dans `.pio/build/<env>/generated/combus/` ;
- dépendances SCons ;
- suppression des premiers `.inc`.

### Phase C — chaînes
- schéma `.cbch` ;
- validation processors ;
- génération du wiring ;
- migration d'une chaîne simple.

### Phase D — migration ComBus

Pour chaque thème :

```text
legacy
  ↓
.cb
  ↓
génération
  ↓
comparaison
  ↓
validation
  ↓
suppression des .inc correspondants
```

### Phase E — suppression legacy

À terme :
- suppression des umbrellas ;
- suppression des fragments `.inc` ;
- simplification de `combus_ids.*` ;
- simplification de `combus.*` ;
- simplification/intégration de `combus_md5.py` selon le nouveau générateur.

## 23. Versionnement

Le schéma peut être versionné :

```yaml
schema_version: 1
```

Cette version concerne le format de définition.

Le générateur peut également être versionné.

Les versions ayant un impact sémantique sur le contrat généré peuvent entrer dans le handshake.

Objectifs :
- évolution contrôlée ;
- incompatibilités détectables ;
- pas de changement silencieux du contrat.

## 24. Futures évolutions

Hors prototype :
- mode `--check` / validation CI ;
- visualisation des chains et du ComBus ;
- conditions plus riches si un besoin réel apparaît ;
- génération de documentation/debug/outils ;
- métadonnées transport supplémentaires.

Ces fonctionnalités ne doivent pas être introduites préventivement.

## 25. Principes à préserver

1. Une définition ComBus appartient à son module.
2. Un `.cb` peut contenir plusieurs channels.
3. Il n'y a pas de `.cbpkg`.
4. Le générateur découvre les définitions ; les umbrellas ne les assemblent plus.
5. Le YAML reste déclaratif.
6. La logique métier reste en C++.
7. Les processors restent en C++.
8. `.cbch` décrit le wiring des chaînes.
9. Les flags viennent de l'environnement de build réel (sans réimplémentation de PlatformIO).
10. L'ordre des IDs est déterministe et constitue un contrat protocolaire.
11. `REMOTE`, `LOCAL` et `SYSTEM` existent dans le modèle dès v2.
12. `uplink` / `downlink` sont intégrés dès maintenant.
13. `FAILSAFE` est `REMOTE`.
14. Les artefacts générés vivent dans `.pio`.
15. Aucun artefact généré n'est commit.
16. Aucune divergence silencieuse n'est acceptée.
17. Le runtime ComBus reste inchangé autant que possible.
18. La migration est prouvée par comparaison avec le legacy avant généralisation.
19. Le tri canonique est **global** (après parsing de tous les `.cb`), jamais par fichier.
20. Le générateur tourne en hook `post:` pour voir `CPPDEFINES` résolu.

## 26. Critère de réussite

Le système cible est :

```text
modules
   │
   ├── *.cb
   ├── *.cbch
   └── *.cpp
        │
        ▼
combus_builder.py
        │
        ▼
.pio/build/<env>/generated/combus/
        │
        ▼
ComBus runtime existant
```

sans :
- umbrellas `.inc` utilisés pour composer le ComBus ;
- duplication de la résolution des flags entre Python et PlatformIO ;
- dépendances artificielles entre modules core et enums machine-specific (problème résolu du câblage Failsafe) ;
- ordre dépendant du filesystem ;
- artefacts stale dans `src/` ;
- double implémentation de la logique métier ;
- divergence silencieuse entre ordre canonique et wire contract legacy.

---

## Annexe — Revue architecturale (résumé)

### Points validés
- Passage `.inc` → `.cb` + générateur Python justifié.
- YAML simple et déclaratif (sous réserve de discipline).
- Séparation `.cb` / `.cbch` / C++ saine.
- REMOTE / LOCAL / SYSTEM + uplink/downlink cohérent.
- Plusieurs channels par `.cb` possible sans recréer d'umbrella.
- Prototype sur un thème simple recommandé.

### Points bloquants
1. **Ordre canonique vs legacy** : divergence = breaking change wire. Détection explicite obligatoire.
2. **Timing `pre:` vs `post:`** : `env["CPPDEFINES"]` vide en `pre:`. Passage en `post:` requis.
3. **Validation des processors référencés** dans `.cbch` au build.

### Risques non bloquants (améliorations futures)
- Support de plusieurs `.cb` par thème (déjà prévu).
- Conditions d'activation plus riches (déjà prévu : « essentiellement présence d'un flag »).
- Métadonnées transport supplémentaires.
- Outils de visualisation / debug.

### Statut

**GO conditionnel.**

Les trois points à lever avant le prototype sont :
1. ordre legacy / wire contract (détection de divergence explicite) ;
2. moment correct de récupération des `CPPDEFINES` (hook `post:`) ;
3. validation des processors référencés par `.cbch`.

Le reste doit rester volontairement léger et évolutif pendant le prototype.

## 27. Notes de travail

> Section de travail pour le rework. Consigne uniquement les **réponses au roadmap** (ex : point A1 → structure du `.cb`). Pas de longs développements : c'est pour relecture lors de la rédaction de la doc finale.

### A1 — Schéma YAML minimal `.cb`

**Structure retenue** (basée sur la branche `failsafe-module` et `platformio.ini`) :

```yaml
# Fichier : src/core/system/failsafe/failsafe.cb
module: failsafe

channels:
  - id: FAILSAFE
    infoName: "Failsafe aggregator"
    type: digital
    scope: REMOTE
    theme: failsafe
    requires: [HAS_FAILSAFE]

  - id: FAILSAFE_VBAT
    infoName: "VBAT failsafe contributor"
    type: digital
    scope: REMOTE
    theme: failsafe
    requires: [HAS_FAILSAFE, HAS_VBAT_FAILSAFE]
```

**Flags observés dans `platformio.ini` (branche `failsafe-module`)** :

| Flag | Rôle | Source |
|---|---|---|
| `IS_MACHINE` | Active les sections machine-only dans les headers ComBus | `[env:machines]` |
| `HAS_FAILSAFE` | Active l'umbrella Failsafe dans les agrégateurs ComBus | `[env:machines]` |
| `HAS_VBAT_FAILSAFE` | Active le contributeur VBAT dans Failsafe | À définir par env |
| `MACHINE_VOLVO_A60_H_BRUDER` | Identité machine partagée machine + sound node | `[volvo_A60H_id]` |
| `IS_MAINBOARD` / `IS_EXT_BOARD` / `IS_REMOTE` / `SOUND_NODE` | Type d'env | par env |

**Champs validés** : `id`, `infoName`, `type`, `scope`, `theme`, `requires`, `uplink`/`downlink`/aucun.

**Validateur** : à choisir (JSON Schema, Pydantic, Cerberus) — point ouvert.

### A2 — Premier `.cb` : `failsafe.cb`

Cible : `src/core/system/failsafe/failsafe.cb` (équivalent du `combus_ids_digital_failsafe.inc` actuel).

**Mapping legacy → `.cb`** :

| Legacy (`.inc`) | `.cb` |
|---|---|
| `FAILSAFE` (dans `combus_ids_digital_failsafe.inc`) | channel `id: FAILSAFE`, `requires: [HAS_FAILSAFE]` |
| `FAILSAFE_VBAT` (dans `combus_ids_digital_vbat_failsafe.inc`) | channel `id: FAILSAFE_VBAT`, `requires: [HAS_FAILSAFE, HAS_VBAT_FAILSAFE]` |
| `FAILSAFE_END` (range marker) | **À traiter** : marker de fin de groupe, pas un channel. Options : (a) générateur le déduit, (b) champ `group_end: true`, (c) ignoré. **À décider**. |



### A2 — Implémentation (rapport)

**Vérification préalable** : un combus VBAT existe-t-il déjà ?

- Recherche dans `src/core/system/vbat/` : aucun `.inc` ComBus, uniquement du code C++ (`vbat.cpp`, `vbat_sense.cpp`, `vbat_alert.cpp`).
- Recherche dans `src/core/config/vbat/` : aucun `.inc` ComBus, uniquement `config.h` (définit `HAS_VBAT_SENSING`).
- Recherche dans `src/core/config/machines/dumper_truck/combus/` : `BATTERY_LOW` est déclaré dans `combus_ids_remote_digital.inc` (ligne 5), mais c'est un channel **digital LOCAL** (écrit par vbat, lu par tous), pas un contributeur Failsafe.
- Recherche dans `src/core/system/combus/` : aucune mention de vbat/VBAT.

**Conclusion** : aucun combus VBAT-as-Failsafe-contributor n'existe dans le working tree actuel. Le channel `FAILSAFE_VBAT` n'existe que sur la branche `failsafe-module` (cf. `src/core/config/vbat/combus_ids_digital_vbat_failsafe.inc`).

**Fichiers créés** :

1. `src/core/system/vbat/vbat_failsafe.cb` — déclare `FAILSAFE_VBAT` (contributeur Failsafe).
   - Équivalent legacy : `src/core/config/vbat/combus_ids_digital_vbat_failsafe.inc` (branche `failsafe-module`).
   - `requires: [HAS_FAILSAFE, HAS_VBAT_FAILSAFE]`.
   - `scope: REMOTE` (le core failsafe doit pouvoir le consommer sans couplage machine).

2. `src/core/system/failsafe/failsafe.cb` — déclare `FAILSAFE` (agrégateur).
   - Équivalent legacy : `src/core/system/failsafe/combus_ids_digital_failsafe.inc` (branche `failsafe-module`).
   - `requires: [HAS_FAILSAFE]`.
   - `scope: REMOTE`.
   - Note : `FAILSAFE_END` (range marker) n'est PAS déclaré — le générateur le déduit automatiquement.

**Vérification `src/core/system/failsafe/` (nouveau rep)** :

- Le répertoire `src/core/system/failsafe/` **n'existe pas** dans le working tree actuel (branche `main`).
- Il existe uniquement sur la branche `failsafe-module` (cf. `git ls-tree failsafe-module`).
- Le fichier `failsafe.cb` est donc créé dans un répertoire qui n'existe pas encore — il faudra créer le répertoire ou merger la branche `failsafe-module` d'abord.

**Recommandation** : merger la branche `failsafe-module` avant de continuer, pour avoir le répertoire `src/core/system/failsafe/` et les fichiers C++ associés (`failsafe.cpp`, `failsafe_chain.cpp`, `proc_failsafe_reset.cpp`, etc.). Sinon, le `.cb` est créé dans un répertoire orphelin.



### A2 — Convention `direction` (uplink / downlink)

**Principe** : `uplink` / `downlink` indiquent le sens de transmission sur le fil, par rapport au nœud de référence (machine). Ils ne définissent pas le nœud : ils définissent le sens du canal dans le contrat remote.

**Format** :

```yaml
scope: REMOTE
direction:
  - uplink
  - downlink
```

Une définition peut éventuellement être bidirectionnelle si elle porte les deux flags.

**Application aux deux `.cb`** :

| Channel | Scope | Direction | Raison |
|---|---|---|---|
| `FAILSAFE_VBAT` (vbat_failsafe.cb) | REMOTE | `uplink` | Le module vbat (machine) envoie le signal FAILSAFE_VBAT en amont vers le core failsafe. Pas de downlink (le core n'écrit pas dans ce channel). |
| `FAILSAFE` (failsafe.cb) | REMOTE | `downlink` | Le core failsafe publie l'état agrégé FAILSAFE en aval vers les consommateurs (machine, sound node, etc.). Pas d'uplink (le core ne consomme pas directement les contributeurs — il passe par l'agrégateur). |



### A3 — Discovery + parsing YAML minimal (implémentation)

**Fichiers créés** :

- `scripts/combus_builder/__init__.py` — package marker.
- `scripts/combus_builder/parser.py` — module A3 (discovery + parsing).

**API principale** (`parser.py`) :

```python
discover_and_parse(env=None, project_root=None) -> (buildroot, files, parsed)
```

- `resolve_buildroot(env, project_root)` : récupère le buildroot depuis
  `env["PROJECT_SRC_DIR"]` (PIO 6.x) → `env["PROJECT_DIR"] + "/src"` → override
  explicite → `cwd + "/src"`. Le buildroot n'est JAMAIS hardcodé à un node.
- `discover_config_files(buildroot)` : `os.walk` récursif, filtre par
  `CONFIG_EXTENSIONS = {".cb", ".cbch"}`. `.pio/` exclus explicitement (avec
  `.git/`, `__pycache__/`, `node_modules/`). Retour en ordre brut (pas de tri).
- `parse_yaml_file(path)` : `yaml.safe_load`, vérifie top-level dict, lève
  `ParseError` avec chemin + ligne/colonne PyYAML en cas d'erreur.
- `parse_all(files)` : itère, conserve l'ordre, lève à la première erreur.

**Sortie** : `(buildroot, [Path...], [(Path, type_label, raw_dict)...])` en
ordre de discovery. Aucune canonisation, aucune résolution de flags,
aucune génération de header.

**Tests effectués** (validés via CLI `python -m scripts.combus_builder.parser .`) :

```
[combus_builder] buildroot = C:\...\src
[combus_builder] discovered 2 config file(s):
  - src/core/system/failsafe/failsafe.cb
  - src/core/system/vbat/vbat_failsafe.cb
[combus_builder] parsed 2 file(s) successfully
```

Tests d'erreur OK :
- fichier vide → `ParseError: empty YAML document`
- top-level list → `ParseError: top-level YAML must be a mapping, got list`
- YAML cassé (flow sequence non terminée) → `ParseError: invalid YAML`
  avec ligne/colonne PyYAML.

**Hors scope A3 (rappel)** : canonisation, résolution `CPPDEFINES` métier,
génération C++, MD5, validation champs, lookup processors, wiring `.cbch`.
Tout cela est reporté aux étapes A4+.

**Difficultés / choix** :

- **Buildroot** : la roadmap demande d'éviter de dupliquer la logique de
  sélection du node. `PROJECT_SRC_DIR` est la variable canonique PIO 6.x.
  Fallback `PROJECT_DIR + "/src"` pour les versions qui ne définissent pas
  la première. Le parser ne fait AUCUNE hypothèse sur le node construit
  (machines/remotes/sound) — il scanne *toute* la `src/`.

- **Pré/`post:` hook** : A3 ne dépend pas du moment où `CPPDEFINES` est
  résolu (c'est un problème A4). Le parser peut donc tourner en `pre:` sans
  problème.

- **Ordre brut** : `os.walk` donne un ordre stable sur un FS donné mais
  pas portable. A5 (canonisation) pose une clé `(scope, type, theme, id)`
  qui rendra l'ordre indépendant du FS. A3 ne triche pas sur cet aspect.

- **Dépendance PyYAML** : le parser requiert `pyyaml`. PlatformIO installe
  `pyyaml` dans son env SCons, donc transparent en build. En CLI standalone,
  `pip install pyyaml` est nécessaire (msg d'erreur explicite si absent).

### Notes diverses

- `FAILSAFE` est `REMOTE` (validé).
- `SYSTEM` est dans le modèle dès v2 (validé).
- `.cbch` est pour plus tard (structure spécifique, hors scope A1).

