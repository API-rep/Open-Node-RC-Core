# ComBus Builder — documentation développeur

Document pour qui modifie le builder. Pour écrire un fichier `.cb`, voir
`doc/configuration/cb_syntax.md`. Pour la trame sur le fil, voir
`src/core/system/combus/protocol/frame/README.md`.

Les renvois au code donnent le fichier et la fonction, pas le numéro de ligne.

## 1. Objet

Le builder lit des fichiers YAML `.cb` et génère les en-têtes et sources C++
du ComBus : énumérations d'identifiants, tableaux de canaux, instance du bus,
empreintes MD5, tables des liaisons entre nœuds. Il remplace la déclaration
manuelle des canaux.

Deux points d'entrée, hors du paquet :

| Entrée | Rôle |
|---|---|
| `scripts/combus_scons_hook.py` | Hook PlatformIO, déclaré par `extra_scripts = pre:scripts/combus_scons_hook.py` dans la section `[env]` de `platformio.ini`. Chemin normal. |
| `scripts/combus_builder.py` | CLI autonome. Mono-carte uniquement : pas de détection de nœud ni de diffusion ; le filtre par type de machine suit le contexte, comme dans le hook. Options : `--project-root`, `--out-dir`, `--define` (répétable, forme `"-D FLAG"` ou `"-D CLE=VAL"` ; un jeton sans `-D` n'est pas lu). |

## 2. Pipeline

```text
platformio.ini (flags)            fichiers .cb / .cbch
        │                                  │
   flags.py (BuildContext)            parser.py (découverte + YAML)
        │                                  │
        │                       [mode nœud] diffusion.py (SYSTEM entre cartes)
        │                                  │
        │                           canon/ (validation, fusion, tri)
        └──────────────┬───────────────────┘
                  generator.py  (télécommande, coupleurs, requires, vues, IDs, C++)
                       │
              md5.py, links.py  (empreintes par vue, liaisons entre nœuds)
                       │
              dossier combus_generated/
```

Ordre exact dans le hook (`_generate_combus_action`) : découverte et parsing
une seule fois pour tout le nœud, diffusion, puis `canonize()` et `generate()`
carte par carte.

### Principes de conception

- **Découverte et validation sont séparées.** Le parser trouve les fichiers et
  vérifie seulement que le YAML est un mapping non vide (`parse_yaml_file`). La
  validation sémantique des channels est faite plus loin, dans
  `canon/channels.py`. L'extension ne décide pas du contenu : `canon` lit la
  clé `channels:` quelle que soit l'extension (`extract_channels_sections`).
- **Ce que décrit un `.cb`.** Un `.cb` dit ce qui existe et comment c'est
  exposé (identifiant, type, portée, direction, activation, valeur sûre). La
  logique métier (seuils, calculs, processors) reste en C++.
- **Les flags viennent du build réel.** Le builder ne réimplémente pas
  PlatformIO : il lit `env["CPPDEFINES"]`, avec `build_flags` en repli et un
  paramètre `override_cppdefines` pour les tests (`acquire_build_context`). La
  source utilisée est tracée dans `BuildContext.cppdefines_source`. Pour les
  cartes d'un nœud qu'on ne compile pas, `node_context.py` lit la configuration
  par l'API de PlatformIO (`ProjectConfig`).
- **Pas de résolution silencieuse.** Doublon d'`id`, clé inconnue,
  `channels: null`, conflit de direction : erreur explicite, jamais « le
  dernier gagne ».
- **Un `id` est unique dans tout le projet**, tous types, scopes et thèmes
  confondus (`ChannelConflictError`, `merge_channels`). Deux définitions du
  même `id` avec des `value` différentes lèvent `ChannelValueConflictError`.
- **Le tri est global**, après lecture de tous les fichiers : l'ordre des
  fichiers n'a aucune valeur.
- **La valeur par défaut est la valeur sûre.** Un `.cb` doit écrire `value:`
  pour chaque channel (`parse_yaml_file`) ; une alarme failsafe déclare
  `value: fault` ; un analogique qui n'est pas à `CbusNeutral` est signalé dans
  le journal du build.

### `requires` : le channel n'existe que si ses flags sont définis

- **Mode nœud** (diffusion) : une déclaration dont le `requires` n'est pas
  satisfait pour une carte est **écartée** pour cette carte
  (`_is_active_for_board`, `diffusion.py`).
- **Mode mono-carte** : `generate()` appelle `resolve_requires`
  (`generator.py`), qui applique la même règle : un channel dont un flag est
  absent n'est pas généré. Il est listé dans le journal du build
  (`[combus_builder] requires: channel 'X' not generated (flag not defined: ...)`),
  donc jamais écarté en silence. Tests : `test_generator.py`
  (`test_resolve_requires_*`, `test_generate_requires_missing_skips_and_logs`).
- Un channel `BASE` ne peut pas avoir de `requires` : la trame d'un type de
  machine est fixe.

## 3. Carte des modules

| Module | Rôle |
|---|---|
| `parser.py` | Cherche récursivement `.cb` et `.cbch` sous `PROJECT_SRC_DIR` (défaut `src/`), exclut `.pio`, `.git`, `__pycache__`, `node_modules` (`discover_config_files`). Charge le YAML (`safe_load`), exige un mapping en racine. N'entre jamais dans `core/config/couplers/`. |
| `machine_type.py` | Détecte le type de machine via un unique flag `MACHINE_TYPE_*` de `MACHINE_TYPE_FLAGS`. Deux flags : `AmbiguousMachineTypeError`. Filtre les `.cb` situés sous `core/config/machines/<autre_type>/`. |
| `flags.py` | `BuildContext` : flags `-D` vus par le compilateur (`defines`, `defines_with_value`). Sert à résoudre `requires`. |
| `node_context.py` | Détection du nœud par noms d'envs `<racine>_<board_id>` (`parse_env_hierarchy`). Résolution statique du contexte de chaque carte, sans compiler. |
| `diffusion.py` | Diffusion des canaux SYSTEM entre les cartes d'un nœud (§5). |
| `canon/channels.py` | Vocabulaire et validation des canaux, détection des doublons d'`id`, tri canonique. |
| `canon/__init__.py` | `canonize()` : extraction de `channels:`, fusion, tri. |
| `generator.py` | Vue de la télécommande, injection des coupleurs, résolution de `requires`, sélection des trois vues, attribution des IDs, rendu C++. |
| `md5.py` | Empreinte MD5 de chaque vue et en-têtes associés. |
| `links.py` | Liaisons entre nœuds : coupleurs, lien avec la télécommande, `combus_peer_links.h` (§6). |
| `coherence.py` | Vérification que le `BuildContext` correspond à `pio run -t idedata` (test d'intégration). |

Hors paquet : `scripts/combus_scons_hook.py`, `scripts/combus_builder.py`,
`scripts/combus_builder_dump_ctx.py`, `scripts/combus_test_add_generated_source.py`.

## 4. Modèle nœud, carte, env racine (« mère »)

- Un **nœud** est une machine ou une télécommande. Il porte une ou plusieurs
  **cartes**.
- Détection : un env `E` est l'env racine du nœud si au moins un env `E_<id>`
  existe ; un env `E_<id>` est une carte de `E`. Sans sous-env, le build est en
  mode mono-carte (`parse_env_hierarchy`).
- L'**env racine** (appelé « mère » dans le code, `board_id = None`) porte
  seulement la configuration commune du nœud (les `build_flags` partagés). Ce
  n'est **pas** une carte : aucun circuit ne lui correspond. La carte
  principale de la machine (par exemple `<racine>_mainboard`) est, elle, un
  sous-env, donc une vraie carte. L'env racine ne participe pas à la diffusion
  et ne produit aucun fichier en mode nœud : `diffuse_system_channels` l'exclut
  et le hook saute l'entrée `board_id is None`. Le compiler est refusé avec un
  message explicite (« is the root env of a node ... Build one of its boards
  instead »).
- En mono-carte, `board_id` vaut `None` : un seul jeu de fichiers, sans
  préfixe.
- Le flag `HAS_RUNLEVEL_MANAGER` désigne la carte propriétaire du canal
  `RUNLEVEL` (« mainboard par convention », `runlevel.cb`), pas l'env racine.

Dans `platformio.ini`, le nœud d'essai `test_node` (cartes
`test_node_mainboard` et `test_node_extboard`) exerce le mode nœud ; les autres
envs sont mono-carte.

## 5. Diffusion SYSTEM entre cartes

Entrée : toutes les déclarations SYSTEM actives par carte (une déclaration est
active si son `requires` est satisfait pour cette carte). Règles, par `id`
(docstring de `diffusion.py`), avec `n_up`, `n_down`, `n_both`, `n_none` le
nombre de cartes actives dans chaque sens :

| Cas | Résultat |
|---|---|
| rien d'actif | canal ignoré |
| `both` avec `uplink` ou `downlink` | `DiffusionConflictError` (fatal) |
| `both` seul | les cartes absentes reçoivent `both`, ou `both_or` si une carte l'a déclaré ; les cartes `none` restent `none` |
| `n_up >= 2` | `DiffusionConflictError` (fatal) |
| `n_up == 1` | les cartes absentes ou `downlink` deviennent/restent `downlink` |
| `n_up == 0`, `n_down >= 1` | avertissement « canal mort », absentes en `none` |
| seulement `none` | aucune diffusion, ni avertissement ni erreur |

Un même `id` déclaré deux fois sur la même carte lève
`DiffusionSystemConflictError` avant toute autre règle. Les variantes `*_or`
suivent la règle de leur famille (`uplink_or` compte comme `uplink`, etc.) ;
`both` et `both_or` sur le même `id` : accepté avec un avertissement
« divergence » ; `both_or` mélangé avec `uplink`/`downlink` : erreur fatale
(`_resolve_group`).

Les channels BASE ne sont pas diffusés entre cartes : leur sens dans le nœud
est donné par `downlink_pt` (§6).

Pourquoi diffuser : le ComBus SYSTEM doit être identique sur toutes les cartes
d'un même nœud. Un module consommateur sur la carte B doit pouvoir lire un
canal produit seulement par un module actif sur la carte A, sans activer ce
module sur chaque carte. Un channel qui doit rester privé à une carte se
déclare en `BOARD` : la diffusion ne traite que le scope SYSTEM, et les
déclarations BOARD ne sont reprises que sur les cartes où leur `requires` est
satisfait.

Chaque carte reçoit ses propres fichiers (§8), y compris les fichiers
d'identifiants : il n'y a pas « un seul jeu d'IDs par nœud ».

Pourquoi l'empreinte SYSTEM est canonisée (`ONE_WAY`) : la diffusion inverse
`uplink` et `downlink` entre cartes par construction. Sans canonisation, deux
cartes correctement configurées auraient des MD5 SYSTEM différents dès qu'un
channel asymétrique existe (faux désaccord garanti).

## 6. Portées, attribution des IDs et vues

Trois portées (champ `scope` d'un channel), de la plus large à la plus étroite :

| Scope | Rôle |
|---|---|
| `BASE` | Profil standard du type de machine : channels que tout nœud compatible partage, même sans liaison réseau. C'est le contrat que le handshake vérifie par MD5. |
| `SYSTEM` | Channels propres à ce nœud (une machine ou une télécommande), partagés entre ses cartes. |
| `BOARD` | Channels internes à une carte, jamais partagés. |

Les anciens commits et les stashes utilisent les anciens noms : `REMOTE` pour
`BASE`, `LOCAL` pour `SYSTEM`, `SYSTEM` pour `BOARD`.

Trois vues, avec le même espace d'IDs (`select_views`) :

| Vue | Contenu | Fichiers émis |
|---|---|---|
| `combus` | BASE + SYSTEM + BOARD | `combus.h`, `combus.cpp`, `combus_ids.h` |
| `combus_system` | BASE + SYSTEM | `combus_system_ids.h` seulement (`_NO_EMIT_RUNTIME_VIEWS`) |
| `combus_base` | BASE | `combus_base_ids.h` seulement |

Deux ordres distincts, à ne pas confondre :

- `_VIEW_SCOPE_ORDER` (`generator.py`) : BASE 0, SYSTEM 1, BOARD 2. Sert à
  **numéroter les IDs**, dans `_select_view` uniquement.
- `SCOPE_ORDER` (`canon/channels.py`) : SYSTEM 0, BASE 1, BOARD 2. Sert au
  **tri canonique** `(scope, type, theme, id)` (`ChannelDefinition.sort_key`,
  `canonize_channels`).

Règle d'attribution (`_select_view`) : les canaux `analog` et `digital` sont
triés séparément, par `(scope, candidate, theme, id)` avec l'ordre
`_VIEW_SCOPE_ORDER`. Les candidats (`candidate: true`, `BASE` seulement)
viennent donc après les `BASE` stables de leur type : en ajouter un ne déplace
aucun ID `BASE` stable. Le `.cb` donne le nom définitif, et `validate_channel`
ajoute `_NEXT` (suffixe réservé, refusé s'il est écrit à la main) ;
`merge_channels` refuse un candidat dont le nom définitif est déjà pris.
Chaque type a son compteur, qui part de 0. Les BASE ont donc les plus petits
IDs de leur type, puis SYSTEM, puis BOARD. Un canal garde le même ID dans toutes
les vues où il apparaît. Les vues `combus_system` et `combus_base` sont ainsi
des préfixes de `combus`.

À quoi sert chaque vue (docstring de `md5.py`, `combus_link_start()` côté C++) :

- `combus_base` : le profil de base du type de machine (ce qui est validé entre
  nœuds par le handshake) ;
- `combus_system` : base d'alignement entre cartes d'un même nœud. Deux cartes
  peuvent porter des channels `BOARD` différents sans que leurs contrats
  SYSTEM divergent, car cette vue n'inclut pas BOARD ;
- `combus` : la vue complète de la carte (référence locale).

L'ordre des IDs est un contrat : changer l'ordre change le protocole sans
changer les noms. Le MD5 porte sur ce qui fait le contrat de chaque channel
(pas seulement l'ID), donc un changement de `direction` ou de valeur par
défaut à ID constant est détecté.

`CH_COUNT` termine chaque énumération. `View.wire_end` (nombre de canaux
BASE) existe côté Python seulement : côté C++, les dimensions de la trame sont
les `CH_COUNT` des énumérations de la vue BASE.

### Sens dans le nœud

Chaque channel généré porte deux sens, regroupés dans `.flow` (`ChanFlow`) :
`.betweenNodes` (sur un lien inter-nœud : celui du `.cb`, ou celui de la
diffusion pour un `SYSTEM`) et `.betweenBoards` (sur un lien intra-nœud),
calculé par `node_direction()` (`generator.py`). Pour un `BASE`, seul
`downlink_pt` circule dans le nœud : `uplink` sur la carte connectée à un autre
nœud (`is_connected_board()`, flags `INTER_NODE_FLAGS`), `downlink` ailleurs ;
`both_or` garde son sens. `check_single_connected_board()` refuse, en mode
nœud, plusieurs cartes connectées (appelé par le hook). Le drapeau
`passthrough` du canon n'entre pas dans le MD5.

### Liaisons entre nœuds : les coupleurs

Le contrat d'une liaison est un **coupleur**, la norme du lien, comme une prise
normalisée : `src/core/config/couplers/<nom>/*.cb`, des channels `BASE`
ordinaires. Le type de machine déclare les coupleurs qu'il supporte dans
`machines/<type>/<type>.mtype` (`load_machine_type_config`) : c'est la
condition de toute instance. Le coupleur est une **brique de la machine** : il
s'écrit **du point de vue de l'embase** (le côté qui porte, le noyau de
l'étoile), comme tout `.cb` de la machine (`downlink_pt` : reçu et relayé vers
la fiche, `downlink` : reçu de la fiche, `uplink` : sa propre valeur envoyée,
`both_or` : faute partagée), `value` = la valeur sûre. Une machine
(`IS_MACHINE`) déclare le **côté** de chacun de ses coupleurs
(`coupler_sides()`) : `-D HAS_COUPLER_SOCKET_<NOM>` (l'embase : elle porte, 0 à
n) ou `-D HAS_COUPLER_PLUG_<NOM>` (la fiche : elle est portée, 0 ou 1) ; le
rôle d'un nœud sur un lien est ce côté, jamais une nature du nœud. Un coupleur
des deux côtés, deux fiches, ou une télécommande (`IS_REMOTE`) avec un coupleur
sont **refusés explicitement** (`node_category()`), comme l'ancien
`-D COMBUS_PEER_<TYPE>`.

- **Télécommande** : un nœud `IS_REMOTE` pilote un type de machine
  (`-D MACHINE_TYPE_<TYPE>`) ; son ComBus est la vue BASE de ce type **en
  miroir** (`remote_channels()` : ce que la machine reçoit, la télécommande
  l'émet), sans les channels SYSTEM et BOARD du type ; sans `MACHINE_TYPE_*`,
  erreur. Le lien machine ↔ télécommande a sa propre table et son propre MD5
  (`compute_remote_link()`, `combus::peer::kRemoteLink`) : il ne porte que ce
  que la télécommande envoie (les entrées du type et les commandes que les
  coupleurs supportés relaient), le même avec ou sans coupleur. Comme tout
  lien, son MD5 porte le côté : la machine est l'embase, la télécommande la
  fiche.
- **Coupleur passif** : un coupleur sans channel de lien, seulement des
  channels locaux (un outil attelé à la main) : une instance locale, sans lien
  ni `ComPeerLink` (`is_passive_coupler()`, journal `set COUPLER <nom>
  (passive)`), en embase seulement.
- **Channels locaux** : un channel du coupleur qui exige le flag d'un de ses
  côtés (`requires: [HAS_COUPLER_SOCKET_<NOM>]` ou `[HAS_COUPLER_PLUG_<NOM>]`)
  appartient à ce côté seulement (`is_local_channel()`) : injecté tel quel,
  filtré par `requires`, jamais sur le lien (ni trame, ni MD5 de lien) ; il est
  BOARD (ou SYSTEM s'il vient d'une autre carte du nœud, jamais BASE), au thème
  `coupler`, et son nom commence par celui du coupleur (`FRONT_COUPLER_CMD` :
  plusieurs coupleurs sur une machine ne se heurtent pas). Une entrée de la
  télécommande (le bouton d'attelage) appartient au contrat d'entrées du type
  de machine, pas au coupleur. Le dossier du coupleur peut porter sa chaîne
  (`<nom>_chain.cpp`, gardée par le flag de son côté,
  `HAS_COUPLER_SOCKET_<NOM>`), branchée par `CouplingDev::chain`.
- **Lecture à part** (`load_coupler_channels()`) : la découverte des `.cb`
  n'entre jamais dans `core/config/couplers/` (`parser._is_coupler_path()`) ;
  seuls les coupleurs déclarés sont lus.
- **Injection** (`inject_coupler_channels()`, appelée par `generate()` avant
  les vues) : le ComBus du coupleur **se fond** dans celui du nœud ; un channel
  du coupleur inconnu du type de machine est ajouté à sa valeur sûre (une
  fonction absente reste sûre) : tel qu'écrit sur l'embase, et de son côté de
  la prise sur la fiche (`_plug_view()` : ce que l'embase émet, la fiche le
  reçoit, et l'inverse). Ajouter un channel à un coupleur change donc le
  ComBus de chaque nœud qui le porte : changement **cassant**.
- **Ensemble de la liaison** (`compute_coupler_link()`) : **tout** le coupleur
  (contrat complet). Un channel de même nom dans le type de machine doit avoir
  le même type, la même valeur et le même `zeroAt`, et **circuler** : deux
  émetteurs, ou personne qui émet, est une erreur. Sur l'embase, `downlink_pt`
  relaie (il émet vers la fiche, ou reçoit là où la fiche émet) ; une source
  par channel sur le nœud (`check_single_source()`, `both_or` excepté).
- **Trame commune** : analogiques puis numériques, stables avant candidats,
  puis le nom (jamais le thème).
- **MD5 du coupleur et du côté** (`link_md5()`) : il ne porte que le coupleur
  (le même pour toutes les machines qui le portent) et un côté ; chaque côté
  annonce MD5(coupleur + son côté) et attend MD5(coupleur + l'autre côté)
  (`peer=True`), si bien que deux embases, ou deux fiches, reliées par erreur
  se refusent.
- `combus_peer_links.h` (toujours écrit, vide sans coupleur ni télécommande) :
  par lien, les MD5 annoncés (`kAnnouncedMd5`, `kAnnouncedMd5Next`) et attendus
  (`kExpectedMd5`, `kExpectedMd5Next`), les nombres de channels, la **table**
  position sur le fil → ID local (`kAnalog[]`, `kDigital[]`) et ce que ce nœud
  reçoit (`kAnalogRx[]`, `kDigitalRx[]`).

Le journal du build affiche une ligne par liaison :
`[combus_builder] set LINK <coupleur> (<côté>)  md5=<32 hex>  A3+D2`. Le
coupleur d'essai `test_coupler` et les types d'essai `test_tractor` et
`test_trailer` (channels préfixés `TEST_`) servent aux tests
(`test_couplers.py`, `test_remote_contract.py`).

**Filtre par type de machine** : avec un type, on charge le cœur et le profil
de ce type ; **sans type, aucun profil de type** : des types voisins partagent
volontairement des noms, comme `BRAKE_BUS` chez un tracteur et sa remorque, et
entreraient en conflit (`filter_files_by_machine_type`).

## 7. MD5

- Une empreinte par vue : MD5 d'un JSON canonique (clés triées, séparateurs
  compacts) (`canonical_bytes`, `compute_view_hash`).
- Entrées par canal, dans l'ordre du fil (l'ordre des IDs) : `id`, `type`,
  `scope`, `direction`, `direction_or`, `value` sous la forme du littéral C++
  émis (une valeur absente et sa valeur par défaut explicite donnent le même
  hash), et `zeroAt` quand il vaut `neutral` (`_view_channel_canonical`).
  `theme` et `infoName` n'y sont pas : un libellé de debug, ou un thème qui ne
  déplace pas le channel sur le fil, ne casse pas le contrat. `requires` n'y
  est pas (son effet est la présence du channel dans la vue).
- Le journal du build affiche une ligne par ensemble :
  `[combus_builder] set BASE    md5=<32 hex>  A9+D9`, puis `set NEXT` (avec le
  nombre de candidats) et `set SYSTEM`, suivies de `[<carte>]` en mode nœud
  (`_print_set_hashes`).
- Le code du builder (`scripts/combus_builder/**/*.py` hors tests) et le hook
  font partie des sources de la génération : les modifier régénère les
  fichiers, comme un `.cb`.
- Vue `combus_system` seulement : canonisation `ONE_WAY`. `uplink`, `downlink`,
  `uplink_or` et `downlink_or` donnent le même état ; `both`, `both_or` et
  `none` restent distincts (`_canonical_direction_for_hash`). C'est ce qui
  permet à deux cartes d'un nœud d'avoir des rôles opposés sans désaccord.
- Vues `combus_base` et `combus` : directions brutes, `uplink` et `uplink_or`
  donnent des empreintes différentes.
- `_canonical_direction_for_hash` est réservée au hash, jamais à la
  génération.
- Ensemble `BASE` : deux empreintes. **Stable** : la vue sans ses candidats
  (`stable_view`), émise en `kCombusBaseComBusMd5`. **Next** : la vue entière
  (stables puis candidats), émise en `kCombusBaseComBusMd5Next`. Sans candidat,
  elles sont égales (le drapeau `candidate` n'est pas haché). Le même en-tête
  émet le nombre de channels de l'ensemble stable par type
  (`kCombusBaseStableAnalogCount`, `kCombusBaseStableDigitalCount`) : taille des
  trames d'un lien `BASE` hors du mode next. La vue `combus_system` contient
  les candidats : deux cartes d'un nœud aux candidats différents n'ont pas le
  même MD5 `SYSTEM`.
- Le hash change donc quand un de ces champs change, quand un canal entre ou
  sort d'une vue (via `requires`), ou quand l'ordre d'attribution des IDs change.
- Propagation selon le scope du channel modifié (conséquence de la composition
  des vues, §6) : un channel BASE change les trois empreintes ; un channel
  SYSTEM change `combus_system` et `combus`, pas `combus_base` ; un channel
  BOARD change seulement `combus`.
- En-têtes émis : `combus_wire_common.h` (constantes communes
  `kProjectVersionMajor`, `kProjectVersionMinor`, `kProjectVersionDev`,
  `kCombusHandshakeWirePayloadLen`) puis un en-tête par vue
  (`k<Vue>ComBusMd5[16]` et sa forme hexadécimale), qui inclut le premier. Les
  constantes sont `static constexpr` : la chaîne d'outils `xtensa-esp32-elf-g++`
  rejette `inline constexpr` à la portée d'espace de noms dans certaines
  configurations.
- Version du projet : elle n'entre pas dans le hash. Le builder lit
  `project_version.h` (racine du dépôt, `read_project_version()`) et écrit
  `MAJOR`, `MINOR` et `DEV` dans `combus_wire_common.h`, avec
  `kCombusHandshakeWirePayloadLen = 19` (16 octets de MD5 et les trois
  versions). Le fichier doit définir `PROJECT_VERSION_MAJOR`,
  `PROJECT_VERSION_MINOR` et `PROJECT_VERSION_DEV` sous la forme `= <n>u;`,
  chacun entre 0 et 255 ; sinon le build s'arrête (`FATAL`, pas de valeur par
  défaut silencieuse). Le hook déclare le fichier comme source : le modifier
  régénère les fichiers. Sans version fournie (appel direct de `generate()`
  dans les tests), `md5.py` garde 0, 1 et 0.

## 8. Sorties générées

Mono-carte : les 10 fichiers de `EXPECTED_ARTIFACTS` (`combus_scons_hook.py`) :
`combus.cpp`, `combus.h`, `combus_ids.h`, `combus_system_ids.h`,
`combus_system_md5.h`, `combus_md5.h`, `combus_base_ids.h`,
`combus_base_md5.h`, `combus_wire_common.h`, `combus_peer_links.h`.

Mode nœud : la carte compilée reçoit les noms **non préfixés** (ceux qu'inclut
le code C++) ; chaque **autre** carte du nœud reçoit neuf fichiers préfixés par
son `board_id` (`combus_<id>.h`, `combus_<id>.cpp`, `combus_<id>_ids.h`,
`combus_system_<id>_ids.h`, `combus_base_<id>_ids.h`, `combus_<id>_md5.h`,
`combus_system_<id>_md5.h`, `combus_base_<id>_md5.h`,
`combus_peer_links_<id>.h`) ; `combus_wire_common.h` n'est pas préfixé
(`_expected_artifacts_for_node`). Les identifiants C++ ne sont pas préfixés.
Avec deux cartes, `test_node_mainboard` publie ainsi 19 fichiers (10 non
préfixés et 9 pour `extboard`). L'env `test_node_extboard` n'est pas fait pour
être compilé (seulement résolu statiquement).

Dossier de sortie (`_default_out_dir`) :

- `Path(PROJECT_BUILD_DIR)/<env>/"combus_generated"` si `PROJECT_BUILD_DIR` est défini ;
- sinon `.pio/build/<env>/combus_generated`.

**Un dossier par env** : SCons décide de régénérer env par env, et deux envs
peuvent avoir des jeux de channels différents (via `requires`). Un dossier
partagé laisserait un env compiler avec les fichiers écrits par un autre. Le
dossier n'est pas nettoyé par `-t clean` : pour forcer la régénération d'un
env, supprimer `<build_dir>/<env>/combus_generated`.
`scripts/combus_test_add_generated_source.py` suit la même règle. Ne pas
surcharger `PLATFORMIO_BUILD_DIR` avec un chemin du projet : le « è » de
« Modèlisme » fait échouer l'édition de liens (`build_dir` de `platformio.ini`
place déjà le build hors projet pour cette raison).

Le CLI écrit par défaut dans `<racine>/out/combus_generated`.

## 9. Hook SCons

- Une seule `env.Command` différée : la génération a lieu au build, quand
  `CPPDEFINES` est complet. `varlist=["CPPDEFINES", "CPPPATH", "LIBDEPS"]`.
  Sources de la commande : les `.cb` et `.cbch` trouvés par `rglob` sous le
  dossier source, le code du builder et `project_version.h`.
- Pourquoi `pre:` et une commande différée : en `pre:`, `CPPDEFINES` n'est pas
  encore peuplé ; en `post:`, la liste des sources est déjà figée. En
  enregistrant seulement une commande SCons au chargement du hook, la
  génération s'exécute plus tard avec l'environnement complet, avant la
  compilation des fichiers qui incluent les en-têtes générés. Les variables du
  `varlist` entrent dans la signature SCons : un changement de flags relance la
  génération, un build identique ne la relance pas. Une action
  `AddPostAction` serait trop tardive pour générer des en-têtes
  (`scripts/combus_builder_dump_ctx.py` l'utilise légitimement, voir §11).
- La résolution du dossier source du parser suit cet ordre
  (`resolve_buildroot`) : `env["PROJECT_SRC_DIR"]`, puis
  `env["PROJECT_DIR"]/src`, puis `project_root/src` (mode CLI), puis `cwd/src`.
  Le hook passe `env`, le CLI passe `project_root`.
- Anti-périmé : écriture dans `<dossier>.new`, contrôle de complétude
  (`_check_staging_complete`), puis publication (`_publish`). Toute erreur
  renvoie un code non nul à SCons.
- `COMBUS_BUILDER_SKIP` (`1`, `true`, `yes`, `on`) : l'action devient un no-op
  qui vérifie seulement la présence des `EXPECTED_ARTIFACTS`, pas des fichiers
  par carte (`_skip_requested`, `_skip_action`). C'est un contournement
  explicite : il ne vérifie pas que les fichiers correspondent aux flags, aux
  `.cb` ni à la version du builder.
- En cas d'échec de génération, le build est interrompu avant toute
  compilation (code de retour non nul de l'action) ; les anciens artefacts
  restent sur disque mais ne sont pas utilisés par ce build. La publication
  `rmtree` puis `os.replace` (`_publish`) laisse une courte fenêtre sans
  dossier : si le processus est tué à cet instant, le build suivant échoue sur
  un en-tête introuvable plutôt que d'utiliser un jeu incohérent.
- Un type de machine est déduit une fois pour tout le nœud ; si une carte en
  résout un autre, le build s'arrête. Le hook journalise
  `machine-type filter active` quand un type est actif.
- Tout env d'une machine porte son `-D MACHINE_TYPE_*` dans `platformio.ini`.
  Les types `excavator` et `loader` sont déclarés dans `MACHINE_TYPE_FLAGS`
  mais n'ont aucun `.cb`.

## 10. Limites et non implémenté

- **Fusion OR côté C++** : faite pour `both_or`
  (`src/core/system/combus/protocol/both_or.md`), état dans
  `DigitalComBus.bothOr` (`BothOrMerge`) ; `uplink_or` et `downlink_or` restent
  déclaratifs.
- **Build réel à deux cartes physiques** : jamais testé.
- **Script YAML vers `platformio.ini`** : prévu, pas écrit. Il doit produire un
  env racine (configuration commune) et un sous-env par carte, mono-carte
  compris, avec `HAS_RUNLEVEL_MANAGER` sur une seule carte par machine.
- **`chains:`** : pas encore de canoniseur (`TODO(ComBus processors)` dans
  `canon/__init__.py`).
- Le code C++ inclut les fichiers générés directement : `#include <combus.h>`,
  `#include <combus_ids.h>`, `#include <combus_base_ids.h>`, ou
  `src/core/config/machines/combus_types.h` qui fait `#include <combus_ids.h>`
  (le hook ajoute le dossier généré au chemin d'inclusion). `combus.h` est
  unique dans le projet : l'API `combus_init()` est dans `combus_init.h`.

## 11. Lancer les tests

Console Windows cp1252 : toujours `PYTHONIOENCODING=utf-8`. Le dossier entier
se lance d'un coup, ou un fichier à la fois :

```text
set "PYTHONIOENCODING=utf-8"
python -m pytest scripts/combus_builder/tests
python -m pytest scripts/combus_builder/tests/test_nd2_diffusion.py
```

Fichiers de `scripts/combus_builder/tests/` :

| Fichier | Teste |
|---|---|
| `test_canon.py` | validation des channels, fusion, tri canonique |
| `test_value.py`, `test_safe_values.py`, `test_fault_value.py`, `test_neutral_value.py` | champ `value` : validation, rendu C++, valeur sûre, `fault`, `CbusNeutral` |
| `test_zero_at.py` | champ `zeroAt` d'un analogique |
| `test_flags.py`, `test_coherence.py` | `BuildContext`, cohérence avec `idedata` |
| `test_machine_type_filter.py` | filtre par type de machine |
| `test_signature_consistency.py` | signatures `env=` / `project_root=` de `discover_and_parse` |
| `test_nd1_integration.py`, `test_nd1_multiboard.py` | nœud, cartes, génération par carte |
| `test_nd2_diffusion.py`, `test_nd6_both_or.py` | diffusion SYSTEM, `both_or` |
| `test_mother_not_a_board.py` | l'env racine (« mère ») n'est pas une carte |
| `test_node_category.py` | catégorie du nœud (`IS_MACHINE`, `IS_REMOTE`) et côtés des coupleurs |
| `test_node_direction.py` | sens dans le nœud, `downlink_pt` |
| `test_generator.py` | vues, IDs, rendu C++, compilation |
| `test_md5.py` | empreintes, canonisation `ONE_WAY` |
| `test_candidates.py` | channels candidats, MD5 stable et next, champ `DEV` |
| `test_project_version.py` | lecture de `project_version.h` |
| `test_couplers.py` | coupleurs : ensemble, ordre, tables, MD5 par côté, channels locaux, passifs |
| `test_remote_view.py`, `test_remote_contract.py` | vue miroir de la télécommande, contrat du lien avec elle |
| `test_dumper_profile.py` | le jeu de channels réel du `dumper_truck` |
| `test_hook.py` | fonctions utilitaires du hook SCons (sans SCons) |
| `test_stress.py` | tests de propriétés sur des `.cb` aléatoires (voir plus bas) |
| `_cpp_compiler.py` | recherche d'un compilateur C++ (aide, pas un test) |

### Tests de stress (`test_stress.py`)

Chaque test tire des déclarations `.cb` aléatoires mais valides, avec un germe
(`seed`) fixe : un échec est reproductible. Invariants vérifiés :

- sortie identique entre deux exécutions et quel que soit l'ordre des fichiers
  et des channels ;
- numérotation des IDs : par type, de 0 à n-1, BASE puis SYSTEM puis BOARD ;
  un channel garde son ID dans toutes les vues ; les vues sont des préfixes ;
- matrice MD5 : changer la valeur par défaut d'un channel BASE change les trois
  empreintes, SYSTEM deux, BOARD une seule ; `infoName`, une valeur par défaut
  écrite explicitement et un `requires` satisfait ne changent rien ;
  `uplink` / `downlink` restent distincts sur la vue BASE ;
- tout doublon d'`id` est refusé ;
- valeurs absurdes dans chaque champ : seule l'exception de validation est
  permise ;
- fichiers YAML aléatoires : seule `ParseError` est permise ;
- diffusion SYSTEM (2 à 5 cartes, env racine présent ou non) comparée à un oracle écrit
  indépendamment à partir des règles de `diffusion.py` : mêmes directions,
  mêmes erreurs, mêmes avertissements.

Réglages : `COMBUS_STRESS_SEEDS` (nombre de germes par test, 25 par défaut) et
`COMBUS_STRESS_SEED0` (premier germe, pour rejouer un échec).

```text
set "COMBUS_STRESS_SEEDS=1500"
python -m pytest scripts/combus_builder/tests/test_stress.py
```

Un passage à 1500 germes représente 15 000 cas, environ 2 minutes.

Les tests de compilation utilisent `g++`, `clang++` ou, à défaut, la chaîne
PlatformIO `xtensa-esp32-elf-g++` (`-fsyntax-only`) ; sans compilateur ils sont
ignorés (`skip`) au lieu d'échouer.

Outils de cohérence avec PlatformIO :

- `python -m scripts.combus_builder.coherence <env>...` compare le
  `BuildContext` acquis par le builder aux définitions réelles renvoyées par
  `pio run -t idedata` (`--no-strict` pour seulement rapporter ; sans argument,
  tous les envs de `platformio.ini`).
- `scripts/combus_builder_dump_ctx.py` est un hook de fin de build qui écrit
  `combus_ctx.json` dans le dossier de build, pour comparer le contexte du vrai
  build à `idedata` par deux invocations `pio` séparées. Il n'est référencé ni
  par `platformio.ini` ni par le hook principal ; il sert aux tests
  d'intégration.
- Les tests d'intégration lourds de `test_flags.py` ne s'exécutent qu'avec
  `COMBUS_INTEGRATION=1`.

Un test doit échouer par `assert` ou par exception. Un test qui `return False`
passe à vide sous pytest. Pour vérifier qu'un test « mord », faire échouer le
code volontairement (en mémoire ou par modification temporaire) et constater
l'échec. `scripts` n'a pas de `__init__.py` (paquet d'espace de noms) : en cas
de doute sur l'origine d'un import, afficher `module.__file__`.
