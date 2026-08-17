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
    scope: LOCAL
    theme: failsafe
```

Le tri est **global** après parsing : l'ordre des fichiers n'a aucune valeur protocolaire. Le tri par fichier (avant fusion) créerait un ordre non-canonique et masquerait les divergences avec le legacy.

### L'extension ne détermine pas le contenu

L'extension (`.cb`, `.cbch`) sert uniquement à la **découverte** et au
**périmètre de collecte**. Elle n'impose pas une sémantique exclusive
au fichier.

Un même fichier peut contenir plusieurs sections (`channels`, `chains`,
`processors`, etc.) :

```yaml
module: vbat

channels:
  - id: BATTERY_VOLTAGE
    type: analog
    scope: LOCAL
    theme: vbat

chains:
  - name: vbat_alert
    processors: [vbat_low_check, vbat_alert_publish]
```

A3 expose la représentation brute du document (toutes sections
présentes). A5 sélectionne ensuite les sections qui relèvent de son
domaine (`channels:` pour A5, `chains:` pour la Phase C).

**Ne pas implémenter de règle du type « `.cb` → channels uniquement »**.

### Frontière A3 / A5 (verrouillée)

Le mot « validation » ne désigne pas la même chose en A3 et en A5.
Cette distinction est verrouillée explicitement :

**A3 = acquisition / collecte** :

> trouver → collecter → vérifier l'existence / validité minimale → stocker brut

A3 doit notamment :
- découvrir les fichiers de configuration à partir du buildroot ;
- collecter les fichiers ;
- vérifier qu'ils existent et sont lisibles ;
- parser le YAML suffisamment pour savoir qu'il s'agit d'un document
  exploitable (top-level mapping, non vide) ;
- vérifier uniquement la structure minimale nécessaire pour pouvoir
  stocker / exposer le document ;
- conserver le contenu brut / la représentation brute nécessaire aux
  étapes suivantes.

**A3 ne valide pas la sémantique ComBus.** Il ne doit notamment pas
décider :
- si un `id` est autorisé ;
- si `scope` / `type` / `theme` forment une combinaison valide ;
- si `direction` est obligatoire ;
- si deux définitions sont en conflit ;
- si deux fichiers définissent le même canal ;
- comment fusionner plusieurs définitions ;
- quel ordre canonique appliquer.

**A5 = interprétation métier / fusion / canonisation** :

> interpréter les sections pertinentes → valider les définitions ComBus → fusionner → détecter les conflits → canoniser / trier

A5 prend les données collectées par A3 et effectue le travail logique.
A5 sélectionne les sections qui relèvent de son domaine (par exemple
`channels:` pour A5, `chains:` pour la Phase C) sans dépendre de
l'extension du fichier.

Cette distinction est respectée dans le code **et dans la documentation**.

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

L'ordre de discovery est brut : la discovery ne trie pas les chemins et
ne leur attribue aucune signification protocolaire. Le tri canonique est
réalisé en aval par l'étape de canonisation (A5).

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
2. capturer cet ordre legacy comme **référence** dans la logique de la canonisation (A5) ;
3. produire l'ordre v2 (via tri canonique) ;
4. comparer ;
5. **échouer explicitement** en cas de divergence.

### Unicité des IDs (verrouillée)

> **`id` est unique globalement, tous types confondus.**

Donc :

```yaml
channels:
  - id: FOO
    type: digital
```

et ailleurs :

```yaml
channels:
  - id: FOO
    type: analog
```

→ **erreur de conflit**.

Le `type` ne fait pas partie de l'espace d'unicité de l'ID. La détection
de collision porte sur `id` globalement.

**Aucune stratégie « last one wins ».** Deux définitions concurrentes
du même `id` doivent provoquer une erreur explicite avec suffisamment
de contexte pour identifier les fichiers concernés.

### Direction et defaults (verrouillée)

```text
SYSTEM → direction implicite = none
LOCAL  → direction obligatoire
REMOTE → direction obligatoire
```

Il n'y a **aucun default implicite** pour `LOCAL` ou `REMOTE`. Raison :
`scope` et `direction` sont deux axes indépendants. Un défaut caché
transformerait implicitement le scope en choix de direction wire.

Donc :

```yaml
scope: SYSTEM
```

sans `direction` peut être normalisé vers `direction = none`.

Mais :

```yaml
scope: LOCAL
```

sans `direction` → erreur de validation.

```yaml
scope: REMOTE
```

sans `direction` → erreur de validation.

### Combinaisons `scope × type × theme` (ambiguïté ouverte)

La documentation évoque des contraintes entre ces axes (cf. §18 :
« combinaison scope/type/theme interdite → erreur »), mais **la table
complète des combinaisons autorisées / interdites n'est pas
explicitement spécifiée** dans la note d'architecture actuelle.

A5 implémente uniquement les contraintes confirmées :
- `type ∈ {analog, digital}` (énuméré §4) ;
- `scope ∈ {REMOTE, LOCAL, SYSTEM}` (énuméré §4) ;
- `theme` est une chaîne libre (pas d'énumération fermée) ;
- `direction` est obligatoire pour `LOCAL` et `REMOTE`, implicite
  `none` pour `SYSTEM` (cf. paragraphe précédent).

Les combinaisons `scope × type × theme` plus fines (par exemple
« SYSTEM × analog interdit ? ») sont **à spécifier dans une passe
ultérieure**. A5 les ignore pour l'instant et les traite comme
autorisées par défaut. Toute combinaison supplémentaire doit être
ajoutée avec un test dédié.

L'étape de canonisation (A5) doit donc **préserver l'ordre wire_end** lorsqu'elle reconstruit la liste des channels actifs : si un ordre legacy est documenté comme important (par exemple via `wire_end` ou tout autre invariant d'ordre dans le code C++), cet ordre est intégré comme clé de tri secondaire ou comme override explicite, et non comme simple tri canonique `(scope, type, theme, id)`. La discovery (A3) et le parsing restent neutres sur cette dimension.

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

### A6.2 — Trois vues générées : `combus`, `combus_local`, `combus_remote`

Le générateur émet **trois vues** partageant le même espace d'IDs :

| Vue | Contenu | Artefacts générés |
|---|---|---|
| `combus` | `scope ∈ {REMOTE, LOCAL, SYSTEM}` | `combus_ids.h`, `combus.h`, `combus.cpp` |
| `combus_local` | `scope ∈ {REMOTE, LOCAL}` (A6.2) | `combus_local_ids.h`, `combus_local.h`, `combus_local.cpp` |
| `combus_remote` | `scope ∈ {REMOTE}` | `combus_remote_ids.h`, `combus_remote.h`, `combus_remote.cpp` |

**Rôle de `combus_local` (A6.2)** : préparer un futur MD5 d'alignement
inter-nœud insensible aux différences `SYSTEM` entre cartes d'un même
nœud. Deux cartes d'un même nœud peuvent instancier des channels
`SYSTEM` différents (répartition de modules entre cartes) ; un MD5
calculé sur `combus` (REMOTE+LOCAL+SYSTEM) diverge donc entre cartes
légitimement configurées. `combus_local` (REMOTE+LOCAL, sans SYSTEM)
sera la base d'un MD5 de vérification d'alignement inter-nœud plus
fiable. Le calcul du MD5 lui-même reste **A7**, hors scope ici.

**Règle de non-renumérotation (verrouillée)** : un channel REMOTE ou
LOCAL doit avoir **exactement le même ID numérique** dans `combus` et
dans `combus_local`. Cette propriété est garantie par construction :
`_select_view()` utilise la même clé de tri canonique
`(scope priority, type, theme, id)` pour les deux vues, et le préfixe
REMOTE+LOCAL de `combus` est contigu. `combus_local` n'a donc pas
besoin d'un sentinel `WireEnd` séparé — `combus.WireEnd` est
directement réutilisable.

**Implémentation** : `combus_local` réutilise exactement le même code
que `combus_remote` (qui prend déjà `allowed_scopes` en paramètre).
Aucun format allégé, aucun chemin de code dédié. Le triplet complet
est généré (mêmes structures `AnalogComBus` / `DigitalComBus`, mêmes
champs `infoName`, `value`, `layer`, `direction`).

**Note explicite pour A7** : le futur calcul MD5 doit porter sur la
**définition complète** de chaque channel
(`id`, `type`, `scope`, `theme`, `direction`, `infoName`), pas
uniquement sur les IDs numériques. Un changement de `direction` sans
changement d'ID doit être détecté — sinon deux compilations avec le
même ensemble de channels mais des `direction` différentes seraient
considérées compatibles à tort.

## 13. MD5 / handshake

Le handshake doit représenter le **contrat ComBus généré**.

Principe (option B retenue, A7) :

```text
MD5(canal canonique des channels d'une vue)
+ constantes partagées (projectVersion, payload len)
```

**projectVersion n'est pas haché.** Il est émis comme constante C++
séparée (`kProjectVersionMajor`, `kProjectVersionMinor`,
`kCombusHandshakeWirePayloadLen = 18u` = 16 md5 + 2 version).
Avantage : un mismatch détecte précisément lequel des deux axes
diverge et produit un diagnostic utile.

**machineType n'est plus émis** (A7.1 puis A7.2 — le handshake consumer
ne le consomme pas).

### Trois vues hashées (A7.2)

A7.2 restaure les **trois** empreintes :

| Vue | Contenu | Symbole C++ | Header | Portée |
|-----|---------|-------------|--------|--------|
| `combus_remote` | REMOTE only | `kCombusRemoteComBusMd5[16]` | `combus_remote_md5.h` | strict wire |
| `combus_local` | REMOTE + LOCAL | `kCombusLocalComBusMd5[16]` | `combus_local_md5.h` | alignement inter-nœud |
| `combus` | REMOTE + LOCAL + SYSTEM | `kCombusComBusMd5[16]` | `combus_md5.h` | full picture (référence locale) |

Chaque vue a son propre MD5 — les changements de scope se propagent
selement aux vues qui contiennent le canal modifié :

* un changement en **REMOTE** modifie les **trois** hashes ;
* un changement en **LOCAL** modifie `combus_local` et `combus`,
  mais **pas** `combus_remote` ;
* un changement en **SYSTEM** modifie **seulement** `combus`.

(Validé par `test_local_influences_local_and_full_not_remote`,
`test_system_influences_only_full`,
`test_remote_influences_all_three_hashes` — `test_md5.py`.)

Le calcul, les inputs (`id`, `type`, `scope`, `theme`, `direction`
triée, `infoName`), la sérialisation (`sort_keys=True`) et l'exclusion
de `requires` restent **strictement ceux validés en A7.1**.
**La valeur de `combus_remote` est inchangée** vs A7.1 (test
`test_combus_remote_hash_matches_a71_value`).

### Header commun (A7.2)

Les constantes réellement communes (`kProjectVersionMajor`,
`kProjectVersionMinor`, `kCombusHandshakeWirePayloadLen`) vivent dans
**un header partagé** `combus_wire_common.h`. Chaque header de vue
inclut `combus_wire_common.h`. Avantage : les trois headers (plus le
partagé) peuvent être inclus ensemble dans la même TU sans
redéfinition — validé par `test_four_headers_compile_together`.

Convention d'émission (A7.2) :

* `combus_wire_common.h` est écrit **en premier** (test
  `test_generate_md5_artifacts_emits_remote_first`) ;
* `combus_remote_md5.h`, `combus_local_md5.h`, `combus_md5.h`
  contiennent **uniquement** leur propre `k<View>ComBusMd5[16]` et
  `k<View>ComBusMd5Hex`, puis `#include "combus_wire_common.h"` ;
* les noms `kCombusRemoteComBusMd5`, `kCombusLocalComBusMd5`,
  `kCombusComBusMd5` sont distincts → pas de collision de symboles.

### Représentation canonique (A7.1, inchangée en A7.2)

- JSON avec `sort_keys=True` ;
- top-level `{ "view": <nom>, "channels": [...] }` ;
- par channel : `{id, type, scope, theme, direction (sorted), infoName}` ;
- channels dans l'ordre canonique `(scope priority, type, theme, id)`
  déjà utilisé par `_select_view()` (A5.1) — **même clé**, donc même ordre.

Le résultat ne dépend ni du chemin, ni des commentaires, ni de l'ordre
YAML, ni du formatage, ni de la découverte. `direction` est sérialisé
comme **liste triée** pour absorber une inversion cosmétique.
`infoName` **fait partie** du hash (renommer un label de debug change
le hash — voulu). `requires` **ne fait pas partie** du hash : son
effet est déjà capturé par la présence/absence du channel dans la vue
après résolution (A6.1).

### Artefacts générés (A7.2)

Pour chaque vue hashée, le générateur émet un header `<view>_md5.h`
dans `.pio/build/<env>/generated/combus/`, plus le header commun
`combus_wire_common.h` :

```cpp
// combus_wire_common.h
namespace combus {
namespace wire {
  static constexpr uint8_t kProjectVersionMajor = 0u;
  static constexpr uint8_t kProjectVersionMinor = 1u;
  static constexpr uint8_t kCombusHandshakeWirePayloadLen = 18u;
} }

// combus_remote_md5.h
#include "combus_wire_common.h"
namespace combus {
namespace wire {
  static constexpr uint8_t kCombusRemoteComBusMd5[16] = { 0x76u, 0x08u, ... };
  static constexpr const char* kCombusRemoteComBusMd5Hex = "76080c7114ce...";
} }

// combus_local_md5.h
#include "combus_wire_common.h"
namespace combus {
namespace wire {
  static constexpr uint8_t kCombusLocalComBusMd5[16] = { 0xa1u, 0x2bu, ... };
  static constexpr const char* kCombusLocalComBusMd5Hex = "a12b7c...";
} }

// combus_md5.h
#include "combus_wire_common.h"
namespace combus {
namespace wire {
  static constexpr uint8_t kCombusComBusMd5[16] = { 0xffu, 0x00u, ... };
  static constexpr const char* kCombusComBusMd5Hex = "ff00...";
} }
```

Raison du choix `static constexpr` (et non `inline constexpr`) : la
toolchain `xtensa-esp32-elf-g++` utilisée par PlatformIO rejette
`inline constexpr` au scope namespace dans certaines configurations.
`static constexpr` est link-safe et compile sur toutes les toolchains
supportées.

### Implémentation

- Module : `scripts/combus_builder/md5.py`.
- API publique :
  - `canonical_bytes(view) -> bytes`
  - `compute_view_hash(view) -> ViewHash`
  - `emit_view_header(hash, out_dir) -> Path` (un fichier par vue)
  - `emit_common_header(out_dir) -> Path` (`combus_wire_common.h`)
  - `generate_md5_artifacts(sel, out_dir) -> (dict, list[Path])`
- Appelé par `generator.generate()` après l'émission des trois vues.
- `projectVersion` est un placeholder `0u/1u` jusqu'à introduction
  d'un `project_version.h` (hors scope A7.2).

### Tests (224/224 passent, 1 skipped si pas de compilateur C++)

`scripts/combus_builder/tests/test_md5.py` couvre :

- déterminisme (re-runs identiques, ordre de discovery différent) ;
- insensibilité au formatage cosmétique (ordre des clés de la dict) ;
- sensibilité à un changement de `direction` (le cas qui a motivé A7) ;
- sensibilité à tout changement de champ ;
- format des artefacts C++ (`static constexpr`, namespace
  `combus::wire`, payload len = 18u, pas de `kMachineType`) ;
- matrice scope → vues (REMOTE → 3 hashes, LOCAL → 2 hashes, SYSTEM
  → 1 hash) ;
- `combus_remote` inchangé vs A7.1 (régression stricte) ;
- **compile-check effectif 4-en-1** : les 4 headers inclus ensemble
  dans la même TU (test `test_four_headers_compile_together`) ;
- déterminisme des 3 hashes à travers re-runs et discovery order.

Le MD5 ne doit plus reconstruire indirectement le comportement du
préprocesseur C++.

### Bilan A7.2 vs A7.1 vs `scripts/combus_md5.py`

A7.1 avait réduit par erreur le pipeline MD5 à 1 vue (`combus_remote`
uniquement). A7.2 restaure les 3 vues **sans rien casser** :

| Aspect | A7 (legacy main) | A7.1 | A7.2 | `combus-md5.py` (handshake) |
|--------|-------------------|------|------|----------------------------|
| Inputs hash | JSON canonique (in-memory) | idem | idem | version + raw .inc content |
| Vues hashées | 2 (local + remote) | **1 (remote)** | **3 (full + local + remote)** | 1 par paire d'`.inc` |
| `kMachineType` | présent | absent | absent | absent |
| Header partagé | non (hack : shared only in local) | non | **oui (`combus_wire_common.h`)** | n/a |
| Constantes partagées | dupliquées dans combus_local_md5.h | dupliquées dans combus_remote_md5.h | **1 seule fois dans `combus_wire_common.h`** | dupliquées par vue |
| Discovery | A3 + scope filter | idem | idem | scan `src/core/` |
| Header path | `combus_remote_md5.h` (build dir) | idem | **3 headers dans build dir** | `combus_ids_remote_md5.h` (à côté des `.inc`) |

Le MD5 lui-même (calcul, inputs, sérialisation, exclusion de
`requires`) n'a **pas bougé** depuis A7.1.

**Reste à faire pour un merge trivial de `combus-frame-handshake`** :

1. Mettre à jour l'include dans `combus_handshake.h` (sur
   `combus-frame-handshake`) pour pointer vers le header généré par
   A7.2 (chemin exact à fixer en Phase B/C quand l'intégration
   PlatformIO sera finalisée). Le consumer référence un unique
   `kCombusWireMd5[16]` qui correspond à `kCombusRemoteComBusMd5[16]`.
2. Supprimer `scripts/combus_md5.py` legacy une fois le consumer
   migré.
3. Vérifier que `combus_handshake.cpp` n'utilise pas `kMachineType`
   (effectué en A7.1 : pas trouvé).
4. **Hors scope A7.2** : implémenter le futur flag de portée wire
   qui décidera *à l'exécution* quelle vue (remote/local/full) sert
   au handshake. Tâche séparée, côté handshake, hors A7.2.

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

Tester également un ordre de scan volontairement différent (fichiers renommés, ordre de découverte modifié) : la discovery (A3) doit retourner une collecte indépendante de l'ordre du filesystem, et la canonisation (A5) doit produire un ordre canonique identique quel que soit l'ordre de discovery reçu.

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
13. `FAILSAFE` est `LOCAL`.
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

-> a mettre à jour a partir de sources
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
| `FAILSAFE_VBAT` (vbat_failsafe.cb) | LOCAL | `uplink` | Le module vbat (machine) envoie le signal FAILSAFE_VBAT en amont vers le core failsafe. Pas de downlink (le core n'écrit pas dans ce channel). |
| `FAILSAFE` (failsafe.cb) | LOCAL | `downlink` | Le core failsafe publie l'état agrégé FAILSAFE en aval vers les consommateurs (machine, sound node, etc.). Pas d'uplink (le core ne consomme pas directement les contributeurs — il passe par l'agrégateur). |



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



### A4 — Acquisition du buildroot et résolution des flags (implémentation)

**Fichiers créés** :

- `scripts/combus_builder/flags.py` — module A4.
- `scripts/combus_builder/tests/test_flags.py` — 20 tests unitaires.

**API principale** (`flags.py`) :

```python
acquire_build_context(env=None, project_root=None, override_cppdefines=None,
                      require_non_empty=True) -> BuildContext
```

Retourne un `BuildContext` (frozen dataclass) avec :

- `buildroot: Path` — src_dir résolu.
- `defines: frozenset[str]` — defines sans valeur.
- `defines_with_value: dict[str, str]` — defines avec valeur (stringifiée).
- `cppdefines_source: str` — `'env' | 'build_flags' | 'override' | 'none'`.
- `raw_cppdefines_sample: str | None` — extrait pour debug.

Helpers : `has(flag)`, `value_of(flag)`, `all_names()`, `to_dict()`.

**Stratégie d'acquisition** (ordre de fiabilité) :

1. `env["PROJECT_SRC_DIR"]` (PIO 6.x canonique) → `env["PROJECT_DIR"] + "/src"` :
   réutilise `parser.resolve_buildroot` (A3) pour éviter une logique parallèle.

2. `env["CPPDEFINES"]` (forme SCons dict/list/tuple) — la plus fiable
   car déjà résolue par SCons. Normalise les 3 formes observées :
   - `{"FOO": 1, "BAR": None}` (dict SCons)
   - `["FOO", "BAR=42", "QUX"]` (list of strings)
   - `[("FOO", 1), ("BAR",)]` (list of tuples)

3. Fallback `env.GetProjectOption("build_flags", "")` + regex `_RE_D_FLAG`
   (héritage `combus_md5.py` marqué transitoire dans la doc §8).

4. `override_cppdefines` — pour tests / CLI.

Erreurs explicites (`BuildContextError`) :
- buildroot introuvable ;
- `CPPDEFINES` inaccessible ET vide (sauf si `require_non_empty=False`).

**Observations réelles sur PlatformIO 6.x** (recopiées du rework Failsafe) :

- `env["CPPDEFINES"]` peut être **vide** en hook `pre:` car PIO 6.x résout
  `extends` et `build_flags` **après** l'exécution des extra_scripts `pre:`.
- En hook `post:`, `CPPDEFINES` est peuplé mais le script ne peut plus
  modifier le build SCons.
- L'option `extends` n'est pas traversée par `GetProjectOption("build_flags")`
  en `pre:` — c'est précisément le cas pathologique que `combus_md5.py`
  contourne par récursion manuelle (transitoire, à supprimer) ;
  A4 ne duplique pas cette logique et se contente de la documenter.

**Solution retenue pour A4** :

- A4 essaie systematic `env["CPPDEFINES"]` **puis** tombe sur `build_flags`
  **puis** accepte `override_cppdefines`.
- A4 ne réimplémente pas la récursion `extends`. Si `pre:` ne donne rien,
  l'utilisateur passe en `post:` ou fournit l'override.
- `BuildContextError` rend le cas pathologique visible au build (pas de
  fallback silencieux).

**Tests** (35/35 passent — 2 skipped sans `COMBUS_A4_INTEGRATION=1`) :

```
python scripts/combus_builder/tests/test_flags.py
# Pour activer les tests d'intégration PlatformIO réels :
COMBUS_A4_INTEGRATION=1 python scripts/combus_builder/tests/test_flags.py
```

Couvre :
- extraction build_flags (simple/valeur/mixte/sans flag/valeur avec `=`) ;
- extraction env CPPDEFINES (dict/list-strings/list-tuples/None) ;
- **STRICT** : entrées CPPDEFINES inconnues (objet exotique) → `BuildContextError`
  au lieu d'un silent skip ;
- **STRICT** : container CPPDEFINES inconnu (ni dict, ni list/tuple) → `BuildContextError` ;
- acquisition (override, env, fallback build_flags, no-defines-raises,
  no-defines-allowed, bad-buildroot, standalone no-env) ;
- helpers BuildContext (all_names, has, value_of, to_dict, frozen) ;
- parser idedata (`_idedata_to_defines`) : list, vide, entries non-string ;
- **intégration PlatformIO** (skip par défaut) :
  - `test_integration_real_build_dump_then_compare_idedata` —
    vrai `pio run -e <env>` → JSON → comparaison.
  - `test_integration_remote_env_real_build` — idem sur un env remote.
- **test structurel** : `test_validation_architecture_does_not_round_trip_idedata`
  ferme le piège du round-trip cité en review (un refactor futur qui
  réintroduirait la dépendance idedata→ctx ferait échouer ce test).

**Validation against ground truth (`pio run -t idedata`)** :

L'architecture précédente (un `validate_against_idedata` qui construisait
son propre `override_cppdefines` à partir d'idedata) faisait un round-trip
texte et était structurellement incapable de détecter une divergence
entre le chemin prod (`env["CPPDEFINES"]` réel) et idedata. Corrigé
en deux pièces indépendantes :

**Pièce 1 — prod path** :
`dump_ctx_to_json(env, out_path)` (helper de `flags.py`) écrit
`ctx.to_dict()` dans un fichier. Appellé par un **post:** hook SCons
dédié (`scripts/combus_builder_dump_ctx.py`) qui s'enregistre via
`env.AddPostAction("$PROG_PATH", ...)`. Sortie :
`.pio/build/<env>/combus_ctx.json`.

**Pièce 2 — comparaison** :
`compare_context_to_idedata(ctx, idedata, env_name, strict=True)` est
une fonction pure. Elle prend un ctx déjà construit (par n'importe quel
chemin) et un payload idedata déjà parsé, et lève
`ContextDivergenceError` à la moindre divergence.

**Le test d'intégration** (`test_integration_real_build_dump_then_compare_idedata`) :
1. lance un **vrai** `pio run -e <env>` (qui fire le post: hook et
   produit `combus_ctx.json` depuis le vrai `env["CPPDEFINES"]`) ;
2. lance un **autre** `pio run -t idedata -e <env>` (ground truth) ;
3. charge `combus_ctx.json` et appelle `compare_context_to_idedata(...)`.

Les deux sources sont produites par **deux invocations pio distinctes**.
Aucune n'est dérivée de l'autre. C'est la seule séquence qui ferme
réellement la question « est-ce que le chemin prod est le même que
idedata ? ».

**Test structurel** :
`test_validation_architecture_does_not_round_trip_idedata` vérifie par
introspection du source de `flags.py` que :
- `dump_ctx_to_json` ne référence ni `read_idedata` ni `override_cppdefines` ;
- `compare_context_to_idedata` ne référence ni `acquire_build_context`
  ni `read_idedata` ;
- `validate_against_idedata` (l'API avec round-trip) n'existe plus.

Ce test empêche une régression future qui réintroduirait le piège.

**Opt-in** : `COMBUS_A4_INTEGRATION=1` pour activer les tests d'intégration.
Skip par défaut (coût : vrai build + toolchain).

**Hors scope A4 (rappel)** : discovery .cb/.cbch (A3), fusion (A5),
tri canonique (A5), génération C++ (A6), MD5 (A7), validation processors
(Phase C).



**Invariant A4 (validé par review)** :

> `env["CPPDEFINES"]` = vérité.
> `env.GetProjectOption("build_flags")` = fallback de compatibilité / prototype,
> avec provenance explicitement signalée par `BuildContext.cppdefines_source`.

Cela colle avec l'invariant global de la note §8 :

> Le générateur doit recevoir les mêmes définitions que celles utilisées pour compiler le C++.

Le fallback `build_flags` est conservé pour le prototype (compat avec
`combus_md5.py` existant), mais **ne doit pas devenir silencieusement la
voie normale**. Tout usage du fallback doit laisser une trace diagnostique
(via `cppdefines_source='build_flags'` ou `BuildContextError` si vide).

Le moment exact d'intégration (`pre:` / `post:` / autre) est validé
séparément en A12, hors du scope A4.

**Intégration PlatformIO** (à finaliser en Phase A / B) :

```python
# scripts/combus_builder_dump_ctx.py (fourni)
# post: hook SCons — enregistre dump_ctx_to_json(env, ...) via AddPostAction.
# Sortie : .pio/build/<env>/combus_ctx.json
Import("env")  # SCons inject
from scripts.combus_builder.flags import (
    acquire_build_context,
    BuildContextError,
)
try:
    ctx = acquire_build_context(env)
except BuildContextError as e:
    print(f"[combus_builder] FATAL: {e}")
    env.Exit(1)

# Validation croisée (CI / sanity check) — déclenche un vrai build :
#   COMBUS_A4_INTEGRATION=1 python scripts/combus_builder/tests/test_flags.py
# Le test lance pio run -e <env> + pio run -t idedata -e <env> et
# compare les deux via compare_context_to_idedata(ctx, idedata, env_name).
```

### A5.1 — Durcissement de la canonisation `channels:`

**Fichiers créés / modifiés** :

- `scripts/combus_builder/canon/__init__.py` — orchestration A5.
- `scripts/combus_builder/canon/channels.py` — contrat `channels:`.
- `scripts/combus_builder/canon.py` — **supprimé** (ancien monolithique).
- `scripts/combus_builder/tests/test_canon.py` — 76 tests.

**Architecture** :

```text
scripts/combus_builder/
    canon/
        __init__.py     # orchestration (canonize, CanonResult)
        channels.py     # contrat et canonisation des `channels:`
```

Pas de sur-fragmentation : tout le vocabulaire et les règles du
format `channels:` sont regroupés dans `channels.py`. Pas de
`chains.py` tant que le format `chains:` n'a pas de contrat réel.

**Vocabulaires centralisés dans `channels.py`** (un seul endroit
à modifier pour étendre) :

| Constante | Rôle |
|---|---|
| `ALLOWED_FIELDS` | clés autorisées par channel |
| `VALID_TYPES` | `analog`, `digital` |
| `VALID_SCOPES` | `LOCAL`, `REMOTE`, `SYSTEM` |
| `VALID_THEMES` | `failsafe`, `vbat`, … (à étendre) |
| `SCOPE_ORDER` | ordre canonique : `LOCAL < REMOTE < SYSTEM` |
| `DIRECTION_TOKENS` | `uplink`, `downlink`, `both`, `none` |
| `DIRECTION_BY_SCOPE` | tokens autorisés par scope |

**Champs autorisés pour `channels:`** (strict) :

```yaml
id:
infoName:
type:
scope:
theme:
direction:
requires:
```

Toute clé inconnue → erreur explicite. Pas d'ignorance silencieuse.
Les anciens champs (`module`, `layer`, `default`, `info_name`) sont
rejetés.

**Règles de validation** :

| Champ | Règle |
|---|---|
| `id` | requis, string non vide, **unique globalement** (tous types/scopes/themes confondus) |
| `infoName` | **REQUIS**, string non vide (pas d'alias `info_name`) |
| `type` | ∈ {`analog`, `digital`} |
| `scope` | ∈ {`LOCAL`, `REMOTE`, `SYSTEM`} |
| `theme` | ∈ `VALID_THEMES` (cadré, extensible) |
| `direction` | contrat par scope (voir ci-dessous) |
| `requires` | optionnel, AND, doublons interdits |

**Contrat `direction` par scope** :

| Scope | Tokens autorisés | Résultat |
|---|---|---|
| `LOCAL` | `uplink`, `downlink`, `both`, `none` | `both` → `{uplink, downlink}`, `none` → `{}` |
| `REMOTE` | `uplink`, `downlink`, `both` | `both` → `{uplink, downlink}`, `none` interdit |
| `SYSTEM` | absent ou `[none]` | `{}` ; toute direction wire → erreur |

**Règles strictes** :

- `direction: []` rejeté pour `LOCAL`/`REMOTE` (pas assimilé à `none`).
- Doublons dans `direction` → erreur.
- Combinaisons incohérentes (`both` + `uplink`, `none` + `uplink`,
  `none` + `both`) → erreur.
- `SYSTEM` sans `direction` ou `direction: [none]` → `frozenset()`.
- Toute direction wire sur `SYSTEM` → erreur.

**Représentation interne de `direction`** :

```python
frozenset({"uplink", "downlink"})  # both
frozenset({"uplink"})              # uplink seul
frozenset({"downlink"})            # downlink seul
frozenset()                        # none / SYSTEM
```

Aucune trace de la syntaxe de surface (`both`, `none`, absence) dans
la sortie. A6 ne voit que `uplink`/`downlink`/vide.

**`requires`** :

- Absent ou `[]` → actif par défaut.
- Non vide → AND (tous les flags doivent être présents).
- Doublons → erreur.
- Résolution effective contre `BuildContext` → A8.

**Fusion** :

- Tous les fichiers découverts peuvent contribuer à `channels:`.
- L'ordre de découverte n'a aucune valeur.
- L'ordre des entrées YAML n'a aucune valeur.
- Conflit d'ID → `ChannelConflictError` avec les deux chemins.

**Ordre canonique** : `(scope, type, theme, id)`, avec
`SCOPE_ORDER = {LOCAL: 0, REMOTE: 1, SYSTEM: 2}`.

**Frontière A3 / A5 (verrouillée)** :

```
A3 = discovery → collecte → existence/validité structurelle minimale
     → parsing YAML → stockage brut
A5 = extraction de channels: → validation sémantique → defaults
     → normalisation → fusion → détection de conflits → canonisation
```

A3 ne décide pas de la sémantique des channels. A5 ne fait pas de
discovery.

**Extension indépendante du contenu** :

Un `.cb` peut contenir plusieurs sections (`channels:`, `chains:`,
etc.). Le traitement est spécifique à chaque format de section, pas
à l'extension du fichier. A5.1 sélectionne par CLÉ (`channels:`),
pas par extension.

**Tests** (76/76 passent) :

```
python scripts/combus_builder/tests/test_canon.py
```

Couvre :
- extraction de sections (extension-agnostique) ;
- rejet des clés inconnues (`typo`, `module`, `layer`, `default`,
  `info_name`) ;
- validation par champ (chaque chemin d'erreur) ;
- `infoName` requis (pas d'alias) ;
- `theme` cadré (rejet des thèmes non listés) ;
- matrice complète `direction` par scope (LOCAL/REMOTE/SYSTEM) ;
- normalisation `both` → `{uplink, downlink}`, `none` → `{}` ;
- rejet des doublons dans `direction` et `requires` ;
- rejet des combinaisons incohérentes ;
- rejet des directions wire sur `SYSTEM` ;
- unicité d'ID (à travers types/scopes/themes) ;
- détection de conflit (avec chemins dans l'erreur) ;
- fusion (multi-fichiers, multi-sections, ordre indépendant) ;
- tri canonique `(scope, type, theme, id)` ;
- déterminisme ;
- fichier package (`channels:` + `chains:`) ;
- test structurel : A5.1 ne branche pas sur `type_label` ;
- test structurel : vocabulaires centralisés dans `channels.py` ;
- **validation sur les vrais `.cb`** :
  - `src/core/system/failsafe/failsafe.cb` (FAILSAFE) ;
  - `src/core/system/vbat/vbat_failsafe.cb` (FAILSAFE_VBAT) ;
  - les deux ensemble (pas de conflit).

**Fichiers réels validés** :

| Fichier | Channel | Résultat |
|---|---|---|
| `src/core/system/failsafe/failsafe.cb` | `FAILSAFE` | OK |
| `src/core/system/vbat/vbat_failsafe.cb` | `FAILSAFE_VBAT` | OK |
| Les deux ensemble | `FAILSAFE`, `FAILSAFE_VBAT` | OK (tri : `FAILSAFE` < `FAILSAFE_VBAT`) |

**Incompatibilités découvertes** : aucune. Les deux fichiers réels
sont conformes au contrat strict.

**Tests A3 / A4 / A5** :

```
A3 : import OK (pas de test dédié, exercé via A5)
A4 : 35/35 tests passed
A5 : 76/76 tests passed
```

**Hors scope A5.1** (rappel) : A6 (génération C++), A7 (MD5), A8
(résolution `requires` contre CPPDEFINES), `.cbch`, `chains:`,
processors, wire contract legacy.

### A5.1 — Corrections de bugs de validation (post-implémentation)

Deux bugs de validation ont été identifiés lors de la review de code.
Ces corrections sont strictement locales et ne modifient pas le
contrat A5.1.

#### Bug 1 — `direction` : entrées non-string

**Avant** : `_normalize_direction` utilisait `set(raw_dir)` avant
d'avoir validé que chaque élément de `direction` est une `str`.

Entrée problématique :

```yaml
direction:
  - [uplink]
```

Avant correction : `TypeError: unhashable type: 'list'`.
Après correction : `ChannelValidationError` explicite.

**Correction** : validation du type de chaque entrée **avant** toute
opération nécessitant des valeurs hashables (`set()`, `dict keys`,
détection de doublons).

```python
bad_type = [d for d in raw_dir if not isinstance(d, str)]
if bad_type:
    raise ChannelValidationError(
        path, channel_id,
        f"`direction` entries must be strings, got {bad_type!r}",
    )
```

Le pattern est aligné sur `_validate_requires` (qui valide déjà le
type des éléments avant la détection de doublons).

**Tests ajoutés** : 4 (list, int, dict, mixed types).

#### Bug 2 — `channels:` : absent vs null vs `[]`

**Avant** : `extract_channels_sections` traitait `null` et la clé
absente de la même façon (silencieusement).

| Forme | Avant | Après |
|---|---|---|
| clé absente | ignorée | **ignorée** |
| `channels: null` | ignorée | **erreur explicite** |
| `channels: []` | acceptée | **acceptée** |
| `channels: [...]` | acceptée | **acceptée** |

**Correction** : la clé présente mais `null` lève maintenant
`ChannelValidationError` avec un message indiquant explicitement
le bon choix (`channels: []` pour une section vide, ou omettre la
clé pour ne pas déclarer de channels).

```python
if section is None:
    raise ChannelValidationError(
        path, None,
        "`channels:` is explicitly null; expected a list of channels "
        "(use `channels: []` for an empty section, or omit the key "
        "to declare no channels)",
    )
```

**Tests ajoutés** : 4 (no key, null, [], absent vs null).

#### Revue ciblée du pattern validation → opération

Pendant la correction, recherche des autres endroits où une
opération nécessitant des valeurs d'un type précis (`set`, `dict`
keys, hash, comparaison spécifique) pourrait être effectuée avant
validation du type des éléments YAML.

| Fonction | Statut |
|---|---|
| `_normalize_direction` | corrigé (bug 1) |
| `_validate_requires` | déjà OK (valide le type avant `set()`) |
| `extract_channels_sections` | corrigé (bug 2) |
| `validate_channel` | OK (chaque champ validé avant usage ; `set(raw.keys())` opère sur les clés qui sont toujours des strings) |
| `merge_channels` | OK (utilise `ch.id` déjà validé) |

Aucune autre correction nécessaire.

#### Tests

```
A5 : 84/84 tests passed (76 existants + 8 nouveaux)
A4 : 35/35 tests passed
A3 : import OK
```

Fichiers `.cb` réels validés : `failsafe.cb`, `vbat_failsafe.cb`
(toujours conformes).

### A6 — Audit préalable de ChanLayer (read-only)

**Périmètre** : audit ciblé du modèle runtime autour de `ChanLayer`,
`.layer`, `runLevelLayer`, `battLowLayer`, et tout accès à
`AnalogComBus`/`DigitalComBus`. Pas d'implémentation, pas de
changement de comportement.

**Contexte acquis** :
- `scope LOCAL/SYSTEM` → vue `combus` ; `scope REMOTE` → vue
  `combus_remote`. Pas de vue `combus_system` séparée.
- `direction` est une propriété runtime du channel, jamais un
  critère de sélection de vue. A5 normalise déjà
  `both` → `{uplink, downlink}`, `none` → `{}`.

#### Tableau champ / rôle / consommateurs / dépendance A6 / risque de suppression

| Champ | Rôle | Consommateurs | Dépendance A6 | Risque de suppression |
|---|---|---|---|---|
| `AnalogComBus::layer` | Garde d'écriture (`_layer_ok`) | `combus_access.cpp` (4 sites : `combus_set_analog`, `combus_set_digital`, `combus_set_runlevel`, `combus_set_battlow`) | **Oui** : si A6 supprime `layer`, la garde d'écriture disparaît | **Élevé** : la garde est la seule fonction runtime de `layer` |
| `DigitalComBus::layer` | Idem | Idem | **Oui** | **Élevé** : idem |
| `ComBus::runLevelLayer` | Garde d'écriture sur `runLevel` | `combus_access.cpp::combus_set_runlevel` | **Oui** | **Moyen** : utilisé uniquement par `combus_set_runlevel` |
| `ComBus::battLowLayer` | Garde d'écriture sur `batteryIsLow` | `combus_access.cpp::combus_set_battlow` | **Oui** | **Moyen** : utilisé uniquement par `combus_set_battlow` |
| `SimDev::chainLayer` | **Mort** : déclaré dans `simulation_struct.h` (archive A4), jamais instancié, jamais lu | Aucun | **Non** | **Aucun** : déjà mort |
| `ChanLayer` (enum) | Type des champs ci-dessus | `combus_struct.h`, `combus_access.h/.cpp`, `combus_frame.h/.cpp`, `simulation_struct.h` (archive), `cb_runlevel.h` (include), `input_update.cpp` | **Oui** | **Élevé** : type central |

#### Usages identifiés (lecture diagnostic vs dépendance fonctionnelle)

**Dépendance fonctionnelle (routage/sélection/autorisation)** :

1. `combus_access.cpp::_layer_ok(slot.layer, caller)` — **garde d'écriture**.
   C'est la **seule** dépendance fonctionnelle réelle de `layer`.
   Si `layer` disparaît, cette garde disparaît.

2. `combus_access.cpp::combus_set_runlevel` — utilise `bus.runLevelLayer`.
   Appelée par :
   - `main.cpp` (5 sites : `IDLE`, `SLEEPING`, `RUNNING`, `IDLE`, `SLEEPING`)
   - `init.cpp` (1 site : `DEF_RUNLEVEL`, `ChanLayer::LOCAL`)
   - `cb_runlevel.cpp` (2 sites : `activeLevel`, `defaultLevel`)
   - `combus_frame.cpp` (1 site : depuis frame RX)

3. `combus_access.cpp::combus_set_battlow` — utilise `bus.battLowLayer`.
   Appelée par :
   - `main.cpp` (1 site : `true` quand `vbat_is_low`)

4. `input_update.cpp` — appelle `combus_set_analog/digital` avec
   `ChanLayer::REMOTE` (3 sites). C'est l'**écriture depuis l'input
   manager** : les channels d'input sont marqués `REMOTE` dans les
   `.inc`, donc l'écriture passe la garde.

**Lecture diagnostic** :

- Aucun. `layer` n'est lu que par `_layer_ok` (garde d'écriture).
  Pas d'affichage, pas de log, pas de sérialisation.

#### Réponses aux points à trancher

**1. `layer` dans `AnalogComBus`/`DigitalComBus` : peut-il être supprimé ?**

**Non, pas sans alternative.** La garde `_layer_ok` est la seule
protection contre les écritures non autorisées (ex: un processor
SYSTEM qui essaierait d'écrire un channel LOCAL). Si `layer`
disparaît, il faut une autre garde.

**Alternative possible** : la distinction `LOCAL` vs `SYSTEM` peut
être portée par l'**adresse du tableau** (les channels SYSTEM
vivent dans `[MACHINE_END..CH_COUNT)`, les LOCAL dans
`[WIRE_END..MACHINE_END)`). Mais cela ne couvre pas la distinction
`REMOTE` (qui est dans `[0..WIRE_END)`).

**Conclusion** : `layer` ne peut pas être supprimé sans refonte
de la garde d'écriture. **À traiter dans A6, pas avant.**

**2. Si `layer` disparaît, où doit vivre la distinction SYSTEM/LOCAL ?**

**Pas de perte démontrée** : `layer` n'est pas supprimé dans cet
audit. Si A6 décide de le supprimer, la distinction SYSTEM/LOCAL
peut être portée par :
- l'adresse du tableau (`[MACHINE_END..CH_COUNT)` pour SYSTEM,
  `[WIRE_END..MACHINE_END)` pour LOCAL) ;
- un flag `isSystem` dans le descripteur de channel ;
- un namespace C++ séparé.

**Aucune de ces options n'est implémentée ni testée.** À
arbitrer en A6.

**3. `runLevelLayer`/`battLowLayer` restent-ils indépendants ?**

**Oui.** Ils sont indépendants de `layer` (sur les channels) :
- `runLevelLayer` est sur `ComBus` (pas sur un channel) ;
- `battLowLayer` est sur `ComBus` (pas sur un channel) ;
- ils protègent des champs uniques (`runLevel`, `batteryIsLow`),
  pas des channels.

Ils utilisent la même enum `ChanLayer` et la même fonction
`_layer_ok`, mais leur rôle est distinct. **Pas de couplage
fonctionnel avec `layer` (channels).**

**4. Régression fonctionnelle potentielle de la fusion LOCAL+SYSTEM ?**

**Oui, si la garde `_layer_ok` est supprimée.** Aujourd'hui :
- un channel `SYSTEM` ne peut être écrit que par un caller
  `SYSTEM` (ex: `cb_runlevel_fn` via l'overload sans `caller`) ;
- un channel `LOCAL` peut être écrit par `LOCAL` ou `SYSTEM` ;
- un channel `REMOTE` peut être écrit par n'importe qui.

Si LOCAL+SYSTEM sont fusionnés (un seul tableau, un seul layer),
la garde `_layer_ok` n'a plus de granularité. **Régression :
perte de la protection SYSTEM-only.**

**5. Adaptations nécessaires pour A6 ?**

**Aucune adaptation runtime.** A6 (génération C++) doit :
- générer les `.inc` (channels) à partir des `.cb` ;
- générer les tableaux `AnalogComBusArray`/`DigitalComBusArray` ;
- préserver le champ `layer` (valeur par défaut : `ChanLayer::LOCAL`
  pour les channels générés, sauf si le `.cb` spécifie autre chose) ;
- préserver `runLevelLayer`/`battLowLayer` (valeurs par défaut
  actuelles : `LOCAL`) ;
- préserver la garde `_layer_ok` (inchangée).

**A6 ne touche pas au runtime.** Le runtime continue d'utiliser
`layer` comme aujourd'hui.

#### Représentation C++ de `direction` (options pour A6)

**Pas de figeage.** Options viables :

| Option | Avantages | Inconvénients |
|---|---|---|
| `enum class Direction : uint8_t { None=0, Uplink=1, Downlink=2 }` + bitmask manuel | Simple, pas de dépendance | Pas d'opérateurs bitwise natifs |
| `enum class Direction : uint8_t { None=0, Uplink=1, Downlink=2, Both=Uplink\|Downlink }` + `operator\|`, `operator&` | Bitwise natif, expressif | `Both` doit être `Uplink\|Downlink` (3), pas 0 |
| Type dédié encapsulant le bitmask (`struct DirectionBits { uint8_t bits; }`) | Encapsulation, pas d'ambiguïté | Verbosité |
| `std::bitset<2>` | Standard, opérateurs intégrés | Overhead mémoire, pas de `enum class` |

**Contraintes pour A6** :
- doit représenter `{uplink}`, `{downlink}`, `{uplink, downlink}`,
  `{}` (4 états) ;
- doit être sérialisable pour debug/log ;
- doit être comparable (égalité, sous-ensemble) ;
- doit tenir sur 1 octet (cohérent avec `ChanLayer`).

**Recommandation (non figée)** : `enum class Direction : uint8_t`
avec `operator|`, `operator&`, `operator~`, et `Both = Uplink |
Downlink`. Simple, expressif, 1 octet.

#### Conclusion

- `layer` (channels) : **dépendance fonctionnelle réelle** (garde
  d'écriture). **Ne pas supprimer** sans alternative.
- `runLevelLayer`/`battLowLayer` : **indépendants** de `layer`
  (channels). **Pas de couplage**.
- `chainLayer` (SimDev) : **mort**. Déjà inerte.
- Fusion LOCAL+SYSTEM : **régression** de la garde `_layer_ok`.
- A6 : **aucune adaptation runtime**. Préserver `layer`,
  `runLevelLayer`, `battLowLayer`, `_layer_ok`.
- `direction` C++ : **4 options viables**, pas de figeage.

**Aucun commit nécessaire** (audit pur, read-only).

### Notes diverses

**Fichiers créés** :

- `scripts/combus_builder/canon.py` — module A5.
- `scripts/combus_builder/tests/test_canon.py` — 42 tests.

**API principale** (`canon.py`) :

```python
canonize(parsed: list[tuple[Path, str, Any]]) -> CanonResult
```

Retourne un `CanonResult` (frozen dataclass) avec :

- `active_definitions: list[ChannelDefinition]` — validé, sans conflit,
  ordre de découverte (pour diagnostic).
- `canonical_definitions: list[ChannelDefinition]` — même contenu,
  trié par `(scope, type, theme, id)`.

**Contrat A3 → A5** (verrouillé) :

- A3 retourne `list[tuple[Path, type_label, raw_dict]]`.
- A5 lit la clé `channels:` de chaque `raw_dict`.
- A5 **ignore** `type_label` (l'extension ne détermine pas le contenu).
  Un `.cbch` avec une section `channels:` est traité comme un `.cb`
  avec une section `channels:`.
- A5 ne consomme pas `BuildContext` (A4). Le champ `requires` est
  préservé tel quel ; sa résolution contre `CPPDEFINES` est le job
  de A8.

**Règles de validation implémentées** :

| Champ | Règle |
|---|---|
| `id` | requis, string non vide, **unique globalement** (tous types confondus) |
| `type` | requis, ∈ {`analog`, `digital`} |
| `scope` | requis, ∈ {`REMOTE`, `LOCAL`, `SYSTEM`} |
| `theme` | requis, string non vide (pas d'énum fermé) |
| `direction` | requis pour `LOCAL`/`REMOTE`, optionnel pour `SYSTEM` (défaut = `none`) |
| `requires` | optionnel, liste de strings non vides |
| `infoName` | optionnel, string |

**Stratégie de fusion** :

- Tous les fichiers collectés peuvent contribuer aux `channels:`.
- L'ordre de découverte n'a aucune valeur.
- L'ordre des entrées dans le YAML n'a aucune valeur.
- Conflit d'ID (même `id` dans deux fichiers) → `ChannelConflictError`
  avec les deux chemins et les deux définitions.

**Stratégie de détection des conflits** :

- Clé d'unicité : `id` (globalement, tous types confondus).
- Pas de « last one wins ».
- Pas de merge partiel.
- Erreur explicite avec contexte suffisant pour identifier les fichiers.

**Ordre canonique** :

```python
(scope, type, theme, id)
```

dans cet ordre exact. Le scope utilise un ordre alphabétique
(`LOCAL < REMOTE < SYSTEM`) défini dans `_SCOPE_ORDER` (un seul
endroit à modifier si l'ordre doit changer).

**Defaults `direction`** :

- `SYSTEM` sans `direction` → `frozenset()` (représente `none`).
- `LOCAL` sans `direction` → erreur.
- `REMOTE` sans `direction` → erreur.
- Doublons dans `direction` → déduplication silencieuse (pas une erreur).

**Tests** (42/42 passent) :

```
python scripts/combus_builder/tests/test_canon.py
```

Couvre :
- extraction de sections (extension-agnostique) ;
- validation par champ (chaque chemin d'erreur) ;
- defaults `direction` (SYSTEM → none, LOCAL/REMOTE → required) ;
- unicité d'ID (à travers les types) ;
- détection de conflit (avec chemins dans l'erreur) ;
- fusion (multi-fichiers, multi-sections, ordre indépendant) ;
- tri canonique `(scope, type, theme, id)` ;
- déterminisme (même set, ordre différent → même sortie) ;
- fichier package (`channels:` + `chains:` dans le même fichier) ;
- test structurel : A5 ne branche pas sur `type_label`.

**Ambiguïtés restantes** (à traiter dans une passe ultérieure) :

1. **Combinaisons `scope × type × theme`** : la table complète des
   combinaisons autorisées/interdites n'est pas explicitement
   spécifiée dans la note d'architecture. A5 implémente uniquement
   les contraintes par axe (énumérations de `type` et `scope`).
   Toute combinaison supplémentaire doit être ajoutée avec un test
   dédié.

2. **Ordre des scopes** : l'ordre canonique utilise l'ordre
   alphabétique (`LOCAL < REMOTE < SYSTEM`). Si un autre ordre est
   requis (par exemple `SYSTEM < LOCAL < REMOTE` pour matcher la
   narration de la doc), il suffit de modifier `_SCOPE_ORDER` dans
   `canon.py`.

**Modifications documentaires effectuées** :

- §3 : ajout de « L'extension ne détermine pas le contenu » et
  « Frontière A3 / A5 (verrouillée) ».
- §10 : ajout de « Unicité des IDs (verrouillée) », « Direction et
  defaults (verrouillée) », « Combinaisons `scope × type × theme`
  (ambiguïté ouverte) ».

**Hors scope A5 (rappel)** : génération C++ (A6), MD5 (A7),
résolution `requires` contre CPPDEFINES (A8), validation processors
(Phase C), wiring `.cbch` (Phase C).

### A5.1 — Corrections de bugs de validation (post-implémentation)

Deux bugs de validation ont été identifiés lors de la review de code.
Ces corrections sont strictement locales et ne modifient pas le
contrat A5.1.

#### Bug 1 — `direction` : entrées non-string

**Avant** : `_normalize_direction` utilisait `set(raw_dir)` avant
d'avoir validé que chaque élément de `direction` est une `str`.

Entrée problématique :

```yaml
direction:
  - [uplink]
```

Avant correction : `TypeError: unhashable type: 'list'`.
Après correction : `ChannelValidationError` explicite.

**Correction** : validation du type de chaque entrée **avant** toute
opération nécessitant des valeurs hashables (`set()`, `dict keys`,
détection de doublons).

```python
bad_type = [d for d in raw_dir if not isinstance(d, str)]
if bad_type:
    raise ChannelValidationError(
        path, channel_id,
        f"`direction` entries must be strings, got {bad_type!r}",
    )
```

Le pattern est aligné sur `_validate_requires` (qui valide déjà le
type des éléments avant la détection de doublons).

**Tests ajoutés** : 4 (list, int, dict, mixed types).

#### Bug 2 — `channels:` : absent vs null vs `[]`

**Avant** : `extract_channels_sections` traitait `null` et la clé
absente de la même façon (silencieusement).

| Forme | Avant | Après |
|---|---|---|
| clé absente | ignorée | **ignorée** |
| `channels: null` | ignorée | **erreur explicite** |
| `channels: []` | acceptée | **acceptée** |
| `channels: [...]` | acceptée | **acceptée** |

**Correction** : la clé présente mais `null` lève maintenant
`ChannelValidationError` avec un message indiquant explicitement
le bon choix (`channels: []` pour une section vide, ou omettre la
clé pour ne pas déclarer de channels).

```python
if section is None:
    raise ChannelValidationError(
        path, None,
        "`channels:` is explicitly null; expected a list of channels "
        "(use `channels: []` for an empty section, or omit the key "
        "to declare no channels)",
    )
```

**Tests ajoutés** : 4 (no key, null, [], absent vs null).

#### Revue ciblée du pattern validation → opération

Pendant la correction, recherche des autres endroits où une
opération nécessitant des valeurs d'un type précis (`set`, `dict`
keys, hash, comparaison spécifique) pourrait être effectuée avant
validation du type des éléments YAML.

| Fonction | Statut |
|---|---|
| `_normalize_direction` | corrigé (bug 1) |
| `_validate_requires` | déjà OK (valide le type avant `set()`) |
| `extract_channels_sections` | corrigé (bug 2) |
| `validate_channel` | OK (chaque champ validé avant usage ; `set(raw.keys())` opère sur les clés qui sont toujours des strings) |
| `merge_channels` | OK (utilise `ch.id` déjà validé) |

Aucune autre correction nécessaire.

#### Tests

```
A5 : 84/84 tests passed (76 existants + 8 nouveaux)
A4 : 35/35 tests passed
A3 : import OK
```

Fichiers `.cb` réels validés : `failsafe.cb`, `vbat_failsafe.cb`
(toujours conformes).

### A6 — Audit préalable de ChanLayer (read-only)

**Périmètre** : audit ciblé du modèle runtime autour de `ChanLayer`,
`.layer`, `runLevelLayer`, `battLowLayer`, et tout accès à
`AnalogComBus`/`DigitalComBus`. Pas d'implémentation, pas de
changement de comportement.

**Contexte acquis** :
- `scope LOCAL/SYSTEM` → vue `combus` ; `scope REMOTE` → vue
  `combus_remote`. Pas de vue `combus_system` séparée.
- `direction` est une propriété runtime du channel, jamais un
  critère de sélection de vue. A5 normalise déjà
  `both` → `{uplink, downlink}`, `none` → `{}`.

#### Tableau champ / rôle / consommateurs / dépendance A6 / risque de suppression

| Champ | Rôle | Consommateurs | Dépendance A6 | Risque de suppression |
|---|---|---|---|---|
| `AnalogComBus::layer` | Garde d'écriture (`_layer_ok`) | `combus_access.cpp` (4 sites : `combus_set_analog`, `combus_set_digital`, `combus_set_runlevel`, `combus_set_battlow`) | **Oui** : si A6 supprime `layer`, la garde d'écriture disparaît | **Élevé** : la garde est la seule fonction runtime de `layer` |
| `DigitalComBus::layer` | Idem | Idem | **Oui** | **Élevé** : idem |
| `ComBus::runLevelLayer` | Garde d'écriture sur `runLevel` | `combus_access.cpp::combus_set_runlevel` | **Oui** | **Moyen** : utilisé uniquement par `combus_set_runlevel` |
| `ComBus::battLowLayer` | Garde d'écriture sur `batteryIsLow` | `combus_access.cpp::combus_set_battlow` | **Oui** | **Moyen** : utilisé uniquement par `combus_set_battlow` |
| `SimDev::chainLayer` | **Mort** : déclaré dans `simulation_struct.h` (archive A4), jamais instancié, jamais lu | Aucun | **Non** | **Aucun** : déjà mort |
| `ChanLayer` (enum) | Type des champs ci-dessus | `combus_struct.h`, `combus_access.h/.cpp`, `combus_frame.h/.cpp`, `simulation_struct.h` (archive), `cb_runlevel.h` (include), `input_update.cpp` | **Oui** | **Élevé** : type central |

#### Usages identifiés (lecture diagnostic vs dépendance fonctionnelle)

**Dépendance fonctionnelle (routage/sélection/autorisation)** :

1. `combus_access.cpp::_layer_ok(slot.layer, caller)` — **garde d'écriture**.
   C'est la **seule** dépendance fonctionnelle réelle de `layer`.
   Si `layer` disparaît, cette garde disparaît.

2. `combus_access.cpp::combus_set_runlevel` — utilise `bus.runLevelLayer`.
   Appelée par :
   - `main.cpp` (5 sites : `IDLE`, `SLEEPING`, `RUNNING`, `IDLE`, `SLEEPING`)
   - `init.cpp` (1 site : `DEF_RUNLEVEL`, `ChanLayer::LOCAL`)
   - `cb_runlevel.cpp` (2 sites : `activeLevel`, `defaultLevel`)
   - `combus_frame.cpp` (1 site : depuis frame RX)

3. `combus_access.cpp::combus_set_battlow` — utilise `bus.battLowLayer`.
   Appelée par :
   - `main.cpp` (1 site : `true` quand `vbat_is_low`)

4. `input_update.cpp` — appelle `combus_set_analog/digital` avec
   `ChanLayer::REMOTE` (3 sites). C'est l'**écriture depuis l'input
   manager** : les channels d'input sont marqués `REMOTE` dans les
   `.inc`, donc l'écriture passe la garde.

**Lecture diagnostic** :

- Aucun. `layer` n'est lu que par `_layer_ok` (garde d'écriture).
  Pas d'affichage, pas de log, pas de sérialisation.

#### Réponses aux points à trancher

**1. `layer` dans `AnalogComBus`/`DigitalComBus` : peut-il être supprimé ?**

**Non, pas sans alternative.** La garde `_layer_ok` est la seule
protection contre les écritures non autorisées (ex: un processor
SYSTEM qui essaierait d'écrire un channel LOCAL). Si `layer`
disparaît, il faut une autre garde.

**Alternative possible** : la distinction `LOCAL` vs `SYSTEM` peut
être portée par l'**adresse du tableau** (les channels SYSTEM
vivent dans `[MACHINE_END..CH_COUNT)`, les LOCAL dans
`[WIRE_END..MACHINE_END)`). Mais cela ne couvre pas la distinction
`REMOTE` (qui est dans `[0..WIRE_END)`).

**Conclusion** : `layer` ne peut pas être supprimé sans refonte
de la garde d'écriture. **À traiter dans A6, pas avant.**

**2. Si `layer` disparaît, où doit vivre la distinction SYSTEM/LOCAL ?**

**Pas de perte démontrée** : `layer` n'est pas supprimé dans cet
audit. Si A6 décide de le supprimer, la distinction SYSTEM/LOCAL
peut être portée par :
- l'adresse du tableau (`[MACHINE_END..CH_COUNT)` pour SYSTEM,
  `[WIRE_END..MACHINE_END)` pour LOCAL) ;
- un flag `isSystem` dans le descripteur de channel ;
- un namespace C++ séparé.

**Aucune de ces options n'est implémentée ni testée.** À
arbitrer en A6.

**3. `runLevelLayer`/`battLowLayer` restent-ils indépendants ?**

**Oui.** Ils sont indépendants de `layer` (sur les channels) :
- `runLevelLayer` est sur `ComBus` (pas sur un channel) ;
- `battLowLayer` est sur `ComBus` (pas sur un channel) ;
- ils protègent des champs uniques (`runLevel`, `batteryIsLow`),
  pas des channels.

Ils utilisent la même enum `ChanLayer` et la même fonction
`_layer_ok`, mais leur rôle est distinct. **Pas de couplage
fonctionnel avec `layer` (channels).**

**4. Régression fonctionnelle potentielle de la fusion LOCAL+SYSTEM ?**

**Oui, si la garde `_layer_ok` est supprimée.** Aujourd'hui :
- un channel `SYSTEM` ne peut être écrit que par un caller
  `SYSTEM` (ex: `cb_runlevel_fn` via l'overload sans `caller`) ;
- un channel `LOCAL` peut être écrit par `LOCAL` ou `SYSTEM` ;
- un channel `REMOTE` peut être écrit par n'importe qui.

Si LOCAL+SYSTEM sont fusionnés (un seul tableau, un seul layer),
la garde `_layer_ok` n'a plus de granularité. **Régression :
perte de la protection SYSTEM-only.**

**5. Adaptations nécessaires pour A6 ?**

**Aucune adaptation runtime.** A6 (génération C++) doit :
- générer les `.inc` (channels) à partir des `.cb` ;
- générer les tableaux `AnalogComBusArray`/`DigitalComBusArray` ;
- préserver le champ `layer` (valeur par défaut : `ChanLayer::LOCAL`
  pour les channels générés, sauf si le `.cb` spécifie autre chose) ;
- préserver `runLevelLayer`/`battLowLayer` (valeurs par défaut
  actuelles : `LOCAL`) ;
- préserver la garde `_layer_ok` (inchangée).

**A6 ne touche pas au runtime.** Le runtime continue d'utiliser
`layer` comme aujourd'hui.

#### Représentation C++ de `direction` (options pour A6)

**Pas de figeage.** Options viables :

| Option | Avantages | Inconvénients |
|---|---|---|
| `enum class Direction : uint8_t { None=0, Uplink=1, Downlink=2 }` + bitmask manuel | Simple, pas de dépendance | Pas d'opérateurs bitwise natifs |
| `enum class Direction : uint8_t { None=0, Uplink=1, Downlink=2, Both=Uplink\|Downlink }` + `operator\|`, `operator&` | Bitwise natif, expressif | `Both` doit être `Uplink\|Downlink` (3), pas 0 |
| Type dédié encapsulant le bitmask (`struct DirectionBits { uint8_t bits; }`) | Encapsulation, pas d'ambiguïté | Verbosité |
| `std::bitset<2>` | Standard, opérateurs intégrés | Overhead mémoire, pas de `enum class` |

**Contraintes pour A6** :
- doit représenter `{uplink}`, `{downlink}`, `{uplink, downlink}`,
  `{}` (4 états) ;
- doit être sérialisable pour debug/log ;
- doit être comparable (égalité, sous-ensemble) ;
- doit tenir sur 1 octet (cohérent avec `ChanLayer`).

**Recommandation (non figée)** : `enum class Direction : uint8_t`
avec `operator|`, `operator&`, `operator~`, et `Both = Uplink |
Downlink`. Simple, expressif, 1 octet.

#### Conclusion

- `layer` (channels) : **dépendance fonctionnelle réelle** (garde
  d'écriture). **Ne pas supprimer** sans alternative.
- `runLevelLayer`/`battLowLayer` : **indépendants** de `layer`
  (channels). **Pas de couplage**.
- `chainLayer` (SimDev) : **mort**. Déjà inerte.
- Fusion LOCAL+SYSTEM : **régression** de la garde `_layer_ok`.
- A6 : **aucune adaptation runtime**. Préserver `layer`,
  `runLevelLayer`, `battLowLayer`, `_layer_ok`.
- `direction` C++ : **4 options viables**, pas de figeage.

**Aucun commit nécessaire** (audit pur, read-only).

### A7 — Calcul MD5 pour combus views (implémentation)

**Statut** : livré (37 tests passent, compilation effective
avec `xtensa-esp32-elf-g++`).

**Fichiers créés** :

- `scripts/combus_builder/md5.py` — module de calcul MD5 + rendu C++.
- `scripts/combus_builder/tests/test_md5.py` — 37 tests unitaires.

**API principale** (`md5.py`) :

```python
canonical_bytes(view: View) -> bytes
compute_view_hash(view: View) -> ViewHash
compute_hashes(views: Iterable[View]) -> dict[str, ViewHash]
emit_md5_header(view_name, hash_, out_dir, machine_type, ...) -> Path
generate_md5_artifacts(sel, out_dir, machine_type, ...) -> (hashes, written)
```

**Représentation canonique (A7.1)** :

- JSON `sort_keys=True` ;
- top-level `{"view": <nom>, "channels": [...]}` ;
- par channel : `{id, type, scope, theme, direction (sorted), infoName}` ;
- channels dans l'ordre canonique `(scope priority, type, theme, id)`.

**Vue hashée** : `combus_local` (REMOTE+LOCAL) et `combus_remote`
(REMOTE only). `combus` (full, REMOTE+LOCAL+SYSTEM) n'est **pas**
haché (cf. §12 / §13.2 sur l'alignement inter-nœud).

**machineType** : détecté via `BuildContext.has("MACHINE_TYPE_*")`
par `_detect_machine_type()`. Émis en string token pour log.

**projectVersion** : placeholder `0u/1u`. Hors scope A7.

**Convention d'émission (A7.1)** :

- Per-view (chaque `_md5.h`) : `static constexpr uint8_t k<Cap>ComBusMd5[16]`
  + `static constexpr const char* k<Cap>ComBusMd5Hex`.
- Constantes partagées (`kProjectVersionMajor/Minor`,
  `kCombusHandshakeWirePayloadLen`, `kMachineType`) émises dans
  `combus_local_md5.h` **uniquement**. `combus_remote_md5.h` ne les
  redéclare pas.

**Choix `static constexpr` (vs `inline constexpr`)** : la toolchain
`xtensa-esp32-elf-g++` utilisée par PlatformIO rejette `inline
constexpr` au scope namespace dans certaines configurations. Le
pattern `static constexpr` est link-safe et compile sur toutes les
toolchains supportées.

**Couverture de tests** (37/37) :

- `canonical_bytes()` shape, JSON valide, champs requis présents ;
- déterminisme (re-runs identiques, ordre de discovery différent) ;
- insensibilité au formatage cosmétique (ordre des clés YAML) ;
- sensibilité à un changement de `direction` (le cas qui a motivé A7) ;
- sensibilité à un changement de chaque champ (id, type, scope, infoName) ;
- format des artefacts C++ (k<Cap>ComBusMd5[16], hex, namespace combus::wire,
  payload len = 18u, machineType custom, version custom) ;
- compilation effective avec `xtensa-esp32-elf-g++` (incluant les
  deux `_md5.h` dans le même TU — régression redefinition fermée) ;
- `_detect_machine_type` (DUMPER_TRUCK / UNCONFIGURED / AMBIGUOUS / valeur) ;
- pipeline complet `generate()` émet bien 9 fichiers de vue + 2 fichiers md5.

**Bilan A7 vs les 10 points de review initiaux** :

1. `IS_MACHINE` : non imposé. Le générateur s'appuie sur `_detect_machine_type`
   qui lit le flag actif dans `BuildContext`. Aucun test de cohérence strict
   sur `IS_MACHINE` — c'est un invariant d'env, pas une dépendance générateur.
2. Ordre wire_end : préservé par construction — `wire_end` est calculé dans
   `_select_view()` et la même clé de tri est utilisée pour le hash. Pas de
   parser legacy : le générateur est la source de vérité.
3. Test d'ordre de scan : `test_md5_deterministic_across_file_discovery_order`
   vérifie l'invariant sans toucher au repo.
4. Runtime Failsafe inchangé : la note le dit explicitement. A7 ne touche
   pas au runtime ComBus ; il ajoute seulement deux nouveaux fichiers
   `_md5.h` qui sont consommés par un futur handshake (hors scope A7).
5. Suppression des `.inc` : déplacée hors d'A7 (Phase D / E).
6. Version majeure : non définie. A7 utilise un placeholder `0u/1u`.
7. Artefact stale `combus_ids_remote_md5.h` : nettoyé en parallèle
   (cf. note en tête du module md5).
8. `combus_ids.h` : généré par le générateur (déjà le cas depuis A6.1).
   Les `_md5.h` sont autonomes et n'incluent pas les `_ids.h`.
9. Séparation analog/digital : déjà en place via A6.1 (deux pipelines,
   deux enums, deux tableaux). A7 ne change rien côté canaux.
10. Processors C++ : hors scope A7. À traiter en Phase C.

**Hors scope A7** : `combus_handshake.h/.cpp` (consommateur du MD5),
`project_version.h`, définition d'une version majeure.

### Notes diverses

- `SYSTEM` est dans le modèle dès v2 (validé).
- `.cbch` est pour plus tard (structure spécifique, hors scope A1).

