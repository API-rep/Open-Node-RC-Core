# Le fichier `.cb` — référence de syntaxe

> Documentation utilisateur des fichiers `.cb`, référencée depuis
> `doc/~combus_v2.md`. Ne décrit que ce qui est implémenté et vérifié sur le
> code. Les points encore à venir sont marqués comme tels. Fonctionnement
> interne du builder : `scripts/combus_builder/README.md`.

## Principe

Un fichier `.cb` est un fichier YAML qui déclare un ou plusieurs channels
ComBus. Au build, le générateur Python (`combus_builder`) lit les `.cb`
applicables à l'environnement compilé et produit les headers/sources ComBus
de chaque board.

```yaml
channels:
  - id: BAT_LOW
    type: digital
    scope: SYSTEM
    theme: vbat
    direction: uplink
    requires: [HAS_VBAT_FAILSAFE]
    infoName: "battery low"
```

## Quels `.cb` sont pris en compte ?

**Extension et recherche.** Le builder cherche, récursivement sous `src/`, les
fichiers d'extension `.cb` et `.cbch` (en pratique, le dossier source du
projet PlatformIO ; `src/` par défaut). Les dossiers `.pio`, `.git`,
`__pycache__` et `node_modules` sont ignorés. Un fichier est lu comme YAML et
doit être un mapping à la racine ; seule la section `channels:` est exploitée,
quelle que soit l'extension. Les autres clés de premier niveau (par exemple
`module:`) sont tolérées et ignorées ; une clé inconnue **dans un channel**
(par exemple `module:`) est une erreur de build.

Un `.cb` peut être placé n'importe où sous `src/`, à côté du module qu'il
décrit ou dans un dossier de configuration machine. Tous ne sont pas générés
pour autant : seuls ceux qui font partie du **buildpath de l'environnement**
le sont.

- Les `.cb` hors de `core/config/machines/<type>/` (modules du `core`,
  configuration machine) sont toujours pris en compte.
- Les `.cb` sous `core/config/machines/<type>/` ne sont pris en compte que si
  `<type>` correspond au type de machine de l'environnement (flag
  `MACHINE_TYPE_*`, ex. `MACHINE_TYPE_DUMPER_TRUCK` → `dumper_truck`). Ceux
  des autres types sont ignorés. Les flags reconnus sont
  `MACHINE_TYPE_DUMPER_TRUCK`, `MACHINE_TYPE_EXCAVATOR` et
  `MACHINE_TYPE_LOADER` ; en ajouter un demande de modifier le builder
  (`machine_type.py`).
- Un environnement sans flag `MACHINE_TYPE_*` n'est filtré sur aucun type. C'est le cas de tous les environnements actuels : `MACHINE_TYPE_*` est défini dans un en-tête de machine et non passé en `-D`, donc le filtre est inactif pour l'instant.
- Deux flags `MACHINE_TYPE_*` dans un même environnement, ou deux boards d'un
  même nœud qui résolvent des types différents, sont une erreur de build.

Le filtre est appliqué une seule fois pour tout le nœud, avant la diffusion
entre boards décrite plus bas.

L'ordre des fichiers et des channels n'a aucune incidence : le générateur
trie les channels de façon canonique (`scope`, `type`, `theme`, `id`).

## Champs d'un channel

| Champ | Obligatoire | Valeurs | Rôle |
|---|---|---|---|
| `id` | oui | chaîne non vide, ex. `BAT_LOW`, `STEERING` | Identifiant unique du channel (devient un nom d'énumération C++) |
| `type` | oui | `digital` \| `analog` | Famille du channel |
| `scope` | oui | `BASE` \| `SYSTEM` \| `BOARD` | Portée de diffusion |
| `theme` | oui | `core` \| `failsafe` \| `vbat` \| `light` \| `input` | Regroupement fonctionnel (liste fermée) |
| `direction` | oui pour SYSTEM et BASE | `uplink` \| `downlink` \| `both` \| `none` \| `uplink_or` \| `downlink_or` \| `both_or` | Sens de circulation, avec ou sans fusion OR |
| `requires` | non | liste de flags de compilation | Activation conditionnelle |
| `value` | non | token statique ou entier | Valeur par défaut |
| `infoName` | oui | chaîne non vide | Nom lisible pour le debug, inclus dans le hash MD5 |

### `id`

Un `id` est unique pour tout le projet, tous types et scopes confondus. Une
même déclaration en double est une erreur de build.

### `theme`

`theme` regroupe les channels par fonction. Le générateur n'accepte que les
valeurs de la liste fermée `VALID_THEMES` (`canon/channels.py`) :
`core`, `failsafe`, `vbat`, `light`, `input`, `com`. Toute autre valeur est une erreur
de build. Pour ajouter un thème, il faut l'ajouter à cette liste dans le
builder. Le thème fait partie du tri canonique et du hash MD5.

### `scope`

| Scope | Portée | Description |
|---|---|---|
| `BASE` | Inter-nœuds | Frame normalisée d'un **type de machine** |
| `SYSTEM` | Intra-nœud | Partagé entre toutes les boards d'un même nœud |
| `BOARD` | Intra-board | Privé au firmware d'une board, jamais partagé |

Anciens noms (avant le 2026-10-18) : `BASE` s'appelait `REMOTE`, `SYSTEM` s'appelait `LOCAL` et `BOARD` s'appelait `SYSTEM`. Les anciens noms `REMOTE` et `LOCAL` sont maintenant refusés ; `SYSTEM` reste valide mais désigne l'ancien `LOCAL`.

**BASE.** Les channels BASE définissent la frame d'un type de machine
(ex. `dumper_truck`). Ils sont déclarés une seule fois dans la définition du
type et font automatiquement partie du ComBus de tout nœud de ce type
(machine, télécommande...). C'est une frame fixe : `requires` y est interdit.
L'inversion automatique de `direction` côté télécommande n'est pas encore
implémentée.

**SYSTEM.** Un channel SYSTEM déclaré sur une seule board est automatiquement
diffusé aux autres boards du nœud (voir `direction`).

**BOARD.** Jamais diffusé. `direction` est facultatif et vaut `none` par
défaut.

### `direction`

`direction` s'écrit en **string** et du point de vue de la board qui déclare
le channel.

| Valeur | Sens |
|---|---|
| `uplink` | la board publie la valeur |
| `downlink` | la board reçoit la valeur |
| `both` | la valeur circule dans les deux sens |
| `uplink_or` | comme `uplink`, avec fusion OR des sources qui écrivent le channel |
| `downlink_or` | comme `downlink`, avec fusion OR |
| `both_or` | comme `both`, avec fusion OR |
| `none` | pas de circulation |

**Variantes `*_or` (fusion OR).** Les trois variantes `uplink_or`,
`downlink_or` et `both_or` ajoutent une politique de fusion OR logique au sens
de circulation : quand plusieurs sources écrivent le même channel dans le même
sens, le résultat est le OR de leurs valeurs au lieu du dernier écrit.

- Réservées aux channels de type `digital` (le OR n'a pas de sens sur une
  valeur analogique).
- Acceptées sur `SYSTEM` et `BASE`, refusées sur `BOARD` (un seul écrivain
  possible, la fusion n'a pas de sens).
- Exemple d'usage pour `both_or` : deux boards qui se synchronisent sur un
  même channel peuvent faire osciller la valeur (A met `true`, B resynchronise
  avec `false`). Avec la fusion OR, une valeur à `true` n'est plus écrasée par
  une resynchronisation.
- **`both_or` est câblé** (2026-10-22) : sur chaque carte, la valeur est le OU
  de la valeur écrite par la carte et des valeurs reçues des liens vivants, et ce
  qu'une carte envoie sur un lien n'inclut jamais ce que ce lien lui a envoyé
  (anti ping-pong). Détail : `src/core/system/combus/protocol/both_or.md`.
- **`uplink_or` et `downlink_or` restent déclaratifs** : le canon valide et
  mémorise la variante, sans traitement OR côté C++ (voir « Non implémenté »).

**Diffusion entre boards (SYSTEM).** Il suffit de déclarer le channel sur une
board : les autres boards du nœud reçoivent automatiquement le channel avec
la direction inverse. La politique OR est locale à la board qui la déclare :
elle n'est pas recopiée, sauf pour `both_or`.

| Déclaré | Reçu par les boards absentes |
|---|---|
| `uplink` ou `uplink_or` | `downlink` |
| `both` | `both` |
| `both_or` | `both_or` |
| `none` | rien : le channel reste absent des autres boards |

La copie diffusée reprend les champs de la déclaration d'origine (`theme`,
`type`, `infoName`, `value`...) mais **pas son `requires`** : une board qui
reçoit le channel par diffusion l'a toujours, même si les flags exigés par la
board source n'y sont pas définis.

Si plusieurs boards déclarent explicitement le même `id`, les variantes `*_or`
suivent la règle de leur famille (`uplink`/`uplink_or`, `downlink`/`downlink_or`,
`both`/`both_or`) :

| Cas | Résultat |
|---|---|
| un seul `uplink`/`uplink_or`, le reste `downlink`/`downlink_or` ou absent | OK, les absentes deviennent `downlink` |
| 2 `uplink`/`uplink_or` ou plus | erreur de build (deux producteurs) |
| `downlink`/`downlink_or` seulement, aucun `uplink` | warning « canal mort », les absentes reçoivent `none` |
| `both` ou `both_or` seuls | OK |
| `both` + `both_or` | OK, warning (politique de fusion divergente) |
| `both`/`both_or` mélangé avec une famille `uplink`/`downlink` | erreur de build |
| `none` avec n'importe quoi | jamais un conflit |

### `requires`

Liste de flags de compilation (tous requis : sémantique ET, pas de OU ni de
NON). Elle décide si le channel existe pour une board :

- **board d'un nœud** (environnement racine + sous-environnements) : la
  déclaration est ignorée pour une board dont l'env ne définit pas tous les
  flags ;
- **environnement unique, sans sous-environnement** (cas actuel de
  `platformio.ini`) : même règle, le channel n'est pas généré quand un flag
  manque. Il est signalé dans le journal du build
  (`[combus_builder] requires: channel 'X' not generated (flag not defined: ...)`),
  donc jamais écarté en silence. Avant le 2026-10-22, un flag manquant était
  une erreur de build (`RequiresResolutionError`).

Sert à activer des fonctions
(`HAS_FAILSAFE`, `HAS_VBAT_FAILSAFE`...), pas à désigner un type de board.
Exemple : `RUNLEVEL` (`runlevel.cb`) n'existe que sur la board qui définit
`HAS_RUNLEVEL_MANAGER`, par convention la `mainboard` (une seule par machine).
Ce channel est **obligatoire sur la carte principale** : `main.cpp` en dépend
sans garde, donc en environnement unique `HAS_RUNLEVEL_MANAGER` doit être
défini ; les autres cartes d'un nœud le reçoivent par diffusion. `FAILSAFE`
(`HAS_FAILSAFE`) est dans le même cas en pratique (décision à confirmer).
Ce n'est pas l'env racine (voir plus bas : l'env racine n'est pas une board).
Interdit sur `scope: BASE`. Un `requires` vide (`[]`) est accepté sur BASE.

### `value` et `infoName`

`value` est copié tel quel lors d'une diffusion vers une autre board.
`infoName` est un libellé de debug inclus dans le hash MD5.

## Validation MD5

Chaque board génère un MD5 par vue (`combus_base`, `combus_system`,
`combus`), que le handshake compare à celui du pair. Le détail du calcul est
dans `scripts/combus_builder/README.md`. Ce qu'il faut retenir en écrivant un
`.cb` :

- **Ce qui change le hash** : `id`, `type`, `scope`, `theme`, `direction` (y
  compris la variante OR) et `infoName`, ainsi que l'apparition ou la
  disparition d'un channel dans une vue (par `requires`).
- **Ce qui ne le change pas** : `value`, `requires` en tant que tel, l'ordre
  des fichiers ou des channels, les commentaires YAML.
- **Vue SYSTEM** : `uplink`, `downlink`, `uplink_or` et `downlink_or` comptent
  pour un même état. Deux boards d'un nœud peuvent donc avoir des rôles
  opposés sur un channel sans désaccord. `both`, `both_or` et `none` restent
  distincts.
- **Vue BASE** : pas d'équivalence ; `uplink` et `uplink_or` donnent des
  hash différents.
- **En cas de désaccord entre deux boards (ou deux nœuds)** : comparer, pour
  chaque côté, les `.cb` réellement pris en compte (filtre de type de machine,
  `requires`), puis `id`, `scope`, `theme`, `direction` et `infoName` des
  channels concernés. Une faute de frappe dans `infoName` suffit à changer le
  hash.

## Environnements multi-boards

Un nœud est décrit par un environnement racine (appelé « mère » dans le code) et des sous-environnements par
board. Le builder détecte un nœud par le nom : un env `X_<id>` est la board
`<id>` de l'env `X` (exemple de convention : `volvo_A60H_bruder` pour l'env racine,
`volvo_A60H_bruder_mainboard` pour une board). Sans sous-environnement, le
build est en mode mono-board, inchangé.

> État : le `platformio.ini` actuel ne déclare aucun sous-environnement, donc
> le mode multi-boards n'est pas exercé en build réel. Le futur script YAML →
> `platformio.ini` est **prévu**, pas écrit.

**L'env racine n'est pas une board.** Il porte seulement la configuration
commune du nœud (flags partagés), il ne correspond à aucun circuit. La carte
principale de la machine est un sous-env (par exemple `..._mainboard`), donc une
vraie board. L'env racine n'est pas une board au sens ComBus :

- il ne participe pas à la diffusion SYSTEM entre boards (il ne compte ni
  comme producteur, ni comme récepteur) ;
- il ne génère pas de fichiers par board en mode multi-boards (le hook le
  saute).

Au build, le générateur résout les contextes de toutes les boards du nœud et
produit les fichiers de chacune : non préfixés pour la board compilée, préfixés
par le nom de la board pour les autres (`combus_<board>.h`, etc. ;
`combus_wire_common.h` est commun).

> Dans un nœud, la board compilée reçoit les noms **non préfixés** et les autres
> boards les noms préfixés. Compiler l'env racine est refusé (message d'erreur).
> Le dossier généré est partagé entre envs. Voir `scripts/combus_builder/README.md`.

## Non implémenté à ce jour

- Inversion/diffusion de `direction` pour le scope BASE côté télécommande.
- Fusion OR côté C++ pour `uplink_or` et `downlink_or`. `both_or` est câblé
  (voir `src/core/system/combus/protocol/both_or.md`). La fusion des contributeurs
  locaux d'un agrégat (par exemple `FAILSAFE_VBAT` → `FAILSAFE`) reste faite par une
  chaîne CbProc (`cb_reset_fn` en tête puis `cb_or_fn` :
  `src/core/system/failsafe/failsafe_chain.cpp`).

### Contrat prévu pour la fusion OR (non implémenté)

Repris de la note de conception de 2026-08-29 (variantes `*_or`). C'est une
**cible**, pas le comportement actuel :

- la fusion devrait être faite en un seul point, l'accesseur d'écriture
  (`combus_set_digital`), et jamais répliquée dans un module écrivain ;
- un cycle devrait respecter l'ordre : remise à zéro des channels `*_or` →
  écritures (réception, entrées locales, autres sources) → agrégation
  (chaînes de lecture) → émission. L'ordre entre réception et entrées locales
  n'a pas d'importance (le OU est commutatif) ;
- si la réception est déclenchée par interruption, un retard d'un cycle avant
  la prise en compte d'une écriture distante est considéré comme acceptable.

L'emplacement de la remise à zéro est aujourd'hui dans la chaîne elle-même
(processor `cb_reset_fn`), pas dans l'accesseur ni dans `sys_manager_update`.

Pour `both_or`, ce contrat est mis en œuvre depuis le 2026-10-22 : la fusion est
faite dans l'accesseur (`combus_set_digital`), comme prévu, avec en plus une
valeur de la carte gardée à part et une émission sans écho. Voir
`src/core/system/combus/protocol/both_or.md`.
