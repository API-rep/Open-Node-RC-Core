# ComBus Builder — documentation développeur

Document pour qui modifie le builder. Pour écrire un fichier `.cb`, voir
`doc/configuration/cb_syntax.md`. Pour la trame sur le fil, voir
`src/core/system/combus/protocol/frame/combus_frame_protocol.md`.

Les références `fichier:ligne` portent sur la branche `main` au moment de la
rédaction. Les points non prouvés par le code sont dans des blocs
`À VÉRIFIER`.

## 1. Objet

Le builder lit des fichiers YAML `.cb` et génère les en-têtes et sources C++
du ComBus : énumérations d'identifiants, tableaux de canaux, instance du bus,
empreintes MD5. Il remplace la déclaration manuelle des canaux.

Deux points d'entrée, hors du paquet :

| Entrée | Rôle |
|---|---|
| `scripts/combus_scons_hook.py` | Hook PlatformIO, déclaré par `extra_scripts = pre:scripts/combus_scons_hook.py` dans `[env]` (`platformio.ini:53`). Chemin normal. |
| `scripts/combus_builder.py` | CLI autonome. Mono-carte uniquement : pas de détection de nœud, pas de diffusion, pas de filtre par type de machine (`combus_builder.py:100-116`). Options : `--project-root`, `--out-dir`, `--define` (répétable, forme `"-D FLAG"` ou `"-D CLE=VAL"` ; un jeton sans `-D` n'est pas lu) (`combus_builder.py:45-64`, `flags.py:166-190`). |

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
                  generator.py  (requires, vues, IDs, C++)
                       │
                    md5.py      (empreintes par vue)
                       │
              dossier combus_generated/
```

Ordre exact dans le hook : découverte et parsing une seule fois pour tout le
nœud, diffusion, puis `canonize()` et `generate()` carte par carte
(`combus_scons_hook.py:747-885`).

### Principes de conception qui restent valables

Repris de l'ancien document de conception, vérifiés dans le code :

- **Découverte et validation sont séparées.** Le parser (`parser.py`) trouve
  les fichiers et vérifie seulement que le YAML est un mapping non vide
  (`parse_yaml_file`, `parser.py:207-247`). La validation sémantique des
  channels est faite plus loin, dans `canon/channels.py`. L'extension ne décide
  pas du contenu : `canon` lit la clé `channels:` quelle que soit l'extension
  (`channels.py:673-714`).
- **Ce que décrit un `.cb`.** Un `.cb` dit ce qui existe et comment c'est
  exposé (identifiant, type, portée, direction, activation). La logique
  métier (seuils, calculs, processors) reste en C++.
- **Les flags viennent du build réel.** Le builder ne réimplémente pas
  PlatformIO : il lit `env["CPPDEFINES"]`, avec `build_flags` en repli et un
  paramètre `override_cppdefines` pour les tests (`flags.py:328-362`). La
  source utilisée est tracée dans `BuildContext.cppdefines_source`. Pour les
  cartes d'un nœud qu'on ne compile pas, `node_context.py` lit la configuration
  par l'API de PlatformIO (`ProjectConfig`).
- **Pas de résolution silencieuse.** Doublon d'`id`, clé inconnue,
  `channels: null`, conflit de direction : erreur explicite, jamais « le
  dernier gagne ».
- **Un `id` est unique dans tout le projet**, tous types, scopes et thèmes
  confondus (`ChannelConflictError`, `channels.py:741-780`). Deux définitions du
  même `id` avec des `value` différentes lèvent `ChannelValueConflictError`.
- **Le tri est global**, après lecture de tous les fichiers : l'ordre des
  fichiers n'a aucune valeur.

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
- Historique : de A6.1 au 2026-10-21, `resolve_requires` levait
  `RequiresResolutionError` en mode mono-carte, ce qui obligeait tout env à
  définir tous les flags de tous les `.cb`. L'exception existe toujours dans
  `generator.py` mais n'est plus levée.
- Conséquence : les envs `remotes` et `machines` passent maintenant la
  génération et échouent plus loin (aucun `INPUT_xxx` défini).

## 3. Carte des modules

| Module | Rôle |
|---|---|
| `parser.py` | Cherche récursivement `.cb` et `.cbch` sous `PROJECT_SRC_DIR` (défaut `src/`), exclut `.pio`, `.git`, `__pycache__`, `node_modules` (`parser.py:72-91`, `135-187`). Charge le YAML (`safe_load`), exige un mapping en racine. |
| `machine_type.py` | Détecte le type de machine via un unique flag `MACHINE_TYPE_*` parmi `DUMPER_TRUCK`, `EXCAVATOR`, `LOADER` (`machine_type.py:78-82`). Deux flags : `AmbiguousMachineTypeError`. Filtre les `.cb` situés sous `core/config/machines/<autre_type>/`. |
| `flags.py` | `BuildContext` : flags `-D` vus par le compilateur (`defines`, `defines_with_value`). Sert à résoudre `requires`. |
| `node_context.py` | Détection du nœud par noms d'envs `<racine>_<board_id>` (`node_context.py:106-195`). Résolution statique du contexte de chaque carte, sans compiler. |
| `diffusion.py` | Diffusion des canaux SYSTEM entre les cartes d'un nœud (§5). |
| `canon/channels.py` | Vocabulaire et validation des canaux, détection des doublons d'`id`, tri canonique. |
| `canon/__init__.py` | `canonize()` : extraction de `channels:`, fusion, tri. |
| `generator.py` | Résolution de `requires`, sélection des trois vues, attribution des IDs, rendu C++. |
| `md5.py` | Empreinte MD5 de chaque vue et en-têtes associés. |
| `coherence.py` | Vérification que le `BuildContext` correspond à `pio run -t idedata` (test d'intégration). |

Hors paquet : `scripts/combus_scons_hook.py`, `scripts/combus_builder.py`,
`scripts/combus_builder_dump_ctx.py`, `scripts/combus_test_add_generated_source.py`.

> À VÉRIFIER : la docstring de `scripts/combus_builder/__init__.py:1-15` ne
> décrit que l'étape A3 et cite des modules « futurs » (`canon.py`). Elle est
> périmée ; le rôle réel de chaque module est celui du tableau ci-dessus.

## 4. Modèle nœud, carte, env racine (« mère »)

- Un **nœud** est une machine ou une télécommande. Il porte une ou plusieurs
  **cartes**.
- Détection : un env `E` est l'env racine du nœud si au moins un env `E_<id>`
  existe ; un env `E_<id>` est une carte de `E`. Sans sous-env, le build est en mode
  mono-carte (`node_context.py:106-195`).
- L'**env racine** (appelé « mère » dans le code, `board_id = None`) porte
  seulement la configuration commune du nœud (les `build_flags` partagés). Ce
  n'est **pas** une carte : aucun circuit ne lui correspond. La carte
  principale de la machine (par exemple `<racine>_mainboard`) est, elle, un
  sous-env, donc une vraie carte. L'env racine ne participe pas à la diffusion
  et ne produit pas de fichiers par carte en mode nœud :
  - `diffusion_boards` exclut `None` quand `node.is_node and None in node.boards`
    (`diffusion.py:589-597`) ;
  - le hook saute l'entrée `board_id is None` (`combus_scons_hook.py:834-836`).
- En mono-carte, `board_id` vaut `None` et le comportement est celui d'avant
  l'introduction des nœuds : mêmes fichiers, mêmes noms (`combus_scons_hook.py:721-745`).
- Le flag `HAS_RUNLEVEL_MANAGER` désigne la carte propriétaire du canal
  `RUNLEVEL` (« mainboard par convention », `runlevel.cb:20-23`), pas l'env racine.

Le `platformio.ini` actuel ne déclare aucun sous-env (envs : `machines`,
`volvo_A60H_bruder`, `remotes`, `test_combus_loopback`). Le mode nœud n'y est
donc pas exercé (voir §10).

## 5. Diffusion SYSTEM entre cartes

Entrée : toutes les déclarations SYSTEM actives par carte (une déclaration est
active si son `requires` est satisfait pour cette carte). Règles, par `id`
(`diffusion.py:16-60`), avec `n_up`, `n_down`, `n_both`, `n_none` le nombre de
cartes actives dans chaque sens :

| Cas | Résultat |
|---|---|
| rien d'actif | canal ignoré |
| `both` avec `uplink` ou `downlink` | `DiffusionConflictError` (fatal) |
| `both` seul | les cartes absentes reçoivent `both`, ou `both_or` si une carte l'a déclaré (`diffusion.py:464-466`) ; les cartes `none` restent `none` |
| `n_up >= 2` | `DiffusionConflictError` (fatal) |
| `n_up == 1` | les cartes absentes ou `downlink` deviennent/restent `downlink` |
| `n_up == 0`, `n_down >= 1` | avertissement « canal mort », absentes en `none` |
| seulement `none` | aucune diffusion, ni avertissement ni erreur |

Un même `id` déclaré deux fois sur la même carte lève
`DiffusionSystemConflictError` avant toute autre règle. Les variantes `*_or`
suivent la règle de leur famille (`uplink_or` compte comme `uplink`, etc.) ;
`both` et `both_or` sur le même `id` : accepté avec un avertissement
« divergence » (`diffusion.py:420-443`), `both_or` mélangé avec
`uplink`/`downlink` : erreur fatale (`diffusion.py:445-448`).

La diffusion BASE (inversion côté télécommande, « ND5 ») n'existe pas.

Pourquoi diffuser : le ComBus SYSTEM doit être identique sur toutes les cartes
d'un même nœud. Un module consommateur sur la carte B doit pouvoir lire un
canal produit seulement par un module actif sur la carte A, sans activer ce
module sur chaque carte. Un channel qui doit rester privé à une carte se
déclare en `BOARD` : la diffusion ne traite que le scope SYSTEM, et les
déclarations BOARD ne sont reprises que sur les cartes où leur `requires` est
satisfait (`diffusion.py:826-868`).

Note sur les fichiers : chaque carte reçoit ses propres fichiers préfixés (§8),
y compris les fichiers d'identifiants. Le code ne génère donc pas « un seul
jeu d'IDs par nœud », contrairement à ce que supposait l'ancienne roadmap de
diffusion (qui prévoyait un seul `combus_ids` par nœud).

Pourquoi l'empreinte SYSTEM est canonisée (« ND7 ») : la diffusion inverse
`uplink` et `downlink` entre cartes par construction. Sans canonisation, deux
cartes correctement configurées auraient des MD5 SYSTEM différents dès qu'un
channel asymétrique existe (faux désaccord garanti).

## 6. Portées, attribution des IDs et vues

Trois portées (champ `scope` d'un channel), de la plus large à la plus étroite :

| Scope | Rôle | Remplace (avant le 2026-10-18) |
|---|---|---|
| `BASE` | Profil standard du type de machine : channels que tout nœud compatible partage, même sans liaison réseau. C'est le contrat que le handshake vérifie par MD5. | `REMOTE` |
| `SYSTEM` | Channels propres à ce nœud (une machine ou une télécommande), partagés entre ses cartes. | `LOCAL` |
| `BOARD` | Channels internes à une carte, jamais partagés. | `SYSTEM` |

Attention à l'historique : avant le 2026-10-18, `SYSTEM` désignait ce qui est
aujourd'hui `BOARD`. Les anciens messages de commit et les stashes utilisent
les anciens noms.

Trois vues, avec le même espace d'IDs (`generator.py:401-419`) :

| Vue | Contenu | Fichiers émis |
|---|---|---|
| `combus` | BASE + SYSTEM + BOARD | `combus.h`, `combus.cpp`, `combus_ids.h` |
| `combus_system` | BASE + SYSTEM | `combus_system_ids.h` seulement (`generator.py:812-819`) |
| `combus_base` | BASE | `combus_base_ids.h` seulement |

Deux ordres distincts, à ne pas confondre :

- `_VIEW_SCOPE_ORDER` (`generator.py:296`) : BASE 0, SYSTEM 1, BOARD 2. Sert
  à **numéroter les IDs**, dans `_select_view` uniquement (`generator.py:338`, `346`).
- `SCOPE_ORDER` (`canon/channels.py:97`) : SYSTEM 0, BASE 1, BOARD 2. Sert au
  **tri canonique** `(scope, type, theme, id)` (`canon/channels.py:208-211`, `787-789`).

Règle d'attribution (`_select_view`) : les canaux `analog` et `digital`
sont triés séparément, par `(scope, candidate, theme, id)` avec l'ordre
`_VIEW_SCOPE_ORDER`. Les candidats (`candidate: true`, `BASE` seulement, link
rework 2.2a) viennent donc après les `BASE` stables de leur type : en ajouter
un ne déplace aucun ID `BASE` stable. Le `.cb` donne le nom définitif, et
`validate_channel` ajoute `_NEXT` (suffixe réservé, refusé s'il est écrit à la
main) ; `merge_channels` refuse un candidat dont le nom définitif est déjà pris.
Chaque type a son compteur, qui part de 0. Les BASE ont donc les plus petits
IDs de leur type, puis SYSTEM, puis BOARD. Un canal garde le même ID dans toutes
les vues où il apparaît. Les vues `combus_system` et `combus_base` sont ainsi
des préfixes de `combus`.

À quoi sert chaque vue (`md5.py:3-9`, `combus_protocol.cpp:55-88`) :

- `combus_base` : le profil de base du type de machine (ce qui est validé entre nœuds par le handshake) ;
- `combus_system` : base d'alignement entre cartes d'un même nœud. Deux cartes
  peuvent porter des channels `BOARD` différents sans que leurs contrats
  SYSTEM divergent, car cette vue n'inclut pas BOARD ;
- `combus` : la vue complète de la carte (référence locale).

L'ordre des IDs est un contrat : changer l'ordre change le protocole sans
changer les noms. Le MD5 porte sur ce qui fait le contrat de chaque channel
(pas seulement l'ID), donc un changement de `direction` ou de valeur par
défaut à ID constant est détecté.

`CH_COUNT` termine chaque énumération. `View.wire_end` (nombre de canaux
BASE) existe côté Python seulement ; les constantes C++ `ComBusWireEnd*`
ne sont plus émises (`generator.py:588-595`).

Le tri des IDs trie par type d'abord, puis `(scope, theme, id)`
(`generator.py:332-350`), et non `(scope, type, theme, id)` comme le tri
canonique.

### Sens dans le nœud (link rework 2.4)

Chaque channel généré porte deux sens, regroupés dans `.flow` (`ChanFlow`) :
`.betweenNodes` (sur un lien inter-nœud : celui du `.cb`, ou celui de la
diffusion pour un `SYSTEM`) et `.betweenBoards` (sur un lien intra-nœud),
calculé par `node_direction()` (`generator.py`). Pour un `BASE`, seul
`downlink_pt` circule dans le nœud : `uplink` sur la carte connectée à un autre
nœud (`is_connected_board()`, flags `INTER_NODE_FLAGS`), `downlink` ailleurs ;
`both_or` garde son sens. `check_single_connected_board()` refuse, en mode
nœud, plusieurs cartes connectées (appelé par le hook). Le drapeau
`passthrough` du canon n'entre pas dans le MD5.

### Liaisons entre nœuds : les coupleurs (link rework 3.1a, 3.4a)

Le contrat d'une liaison est un **coupleur**, la norme du lien, comme une prise
normalisée : `src/core/config/couplers/<nom>/*.cb`, des channels `BASE`
ordinaires, au **sens vu de l'attelage** (`uplink` : vers le véhicule,
`downlink` : depuis le véhicule, `both_or` : faute partagée), sans `_pt` (le
relais est l'affaire d'un nœud), `value` = la valeur sûre. Un nœud déclare ses
coupleurs : `-D COMBUS_COUPLER_<NOM>` (`links.coupler_names()`), de 0 à n sur
un véhicule, exactement 1 sur un attelage ; `IS_MACHINE` ou `IS_REMOTE` avec un
coupleur est **refusé explicitement** (`node_nature()`), comme l'ancien
`-D COMBUS_PEER_<TYPE>` (supprimé en 3.4a).

- **Lecture à part** (`load_coupler_channels()`) : la découverte des `.cb`
  n'entre jamais dans `core/config/couplers/` (`parser._is_coupler_path()`) ;
  seuls les coupleurs déclarés sont lus.
- **Injection** (`inject_coupler_channels()`, appelée par `generate()` avant
  les vues) : le ComBus du coupleur **se fond** dans celui du nœud ; un channel
  du coupleur inconnu du type de machine est ajouté à sa valeur sûre (une
  fonction absente reste sûre), au sens du coupleur sur l'attelage, au sens
  miroir sur le véhicule. Ajouter un channel à un coupleur change donc le
  ComBus de chaque nœud qui le porte : changement **cassant**.
- **Ensemble de la liaison** (`compute_coupler_link()`) : **tout** le coupleur
  (contrat complet). Un channel de même nom dans le type de machine doit avoir
  le même type, la même valeur et le même `zeroAt`, et **circuler** : deux
  émetteurs, ou personne qui émet, est une erreur (plus de « channel mort »).
  Sur le véhicule, `downlink_pt` relaie (il émet vers l'attelage, ou reçoit
  là où l'attelage émet) ; une source par channel sur le nœud
  (`check_single_source()`, `both_or` excepté).
- **Trame commune** : analogiques puis numériques, stables avant candidats,
  puis le nom (jamais le thème).
- **MD5 du coupleur et du rôle** (`link_md5()`) : il ne porte que le coupleur
  (le même pour toutes les machines qui le portent) et un rôle ; chaque côté
  annonce MD5(coupleur + son rôle) et attend MD5(coupleur + l'autre rôle)
  (`peer=True`), si bien que deux véhicules, ou deux attelages, reliés par
  erreur se refusent.
- `combus_peer_links.h` (toujours écrit, vide sans coupleur) : par coupleur,
  les MD5 annoncés (`kMd5`, `kMd5Stable`) et attendus (`kMd5Peer`,
  `kMd5PeerStable`), les nombres de channels, la **table** position sur le fil
  → ID local (`kAnalog[]`, `kDigital[]`) et ce que ce nœud reçoit
  (`kAnalogRx[]`, `kDigitalRx[]`).

Le journal du build affiche une ligne par liaison :
`[combus_builder] set LINK <coupleur> (<rôle>)  md5=<32 hex>  A3+D2`. Le
coupleur d'essai `test_hitch` et les types d'essai `test_tractor` et
`test_trailer` (channels préfixés `TEST_`) servent aux tests
(`test_couplers.py`).

**Filtre par type de machine** : avec un type, on charge le cœur et le profil
de ce type ; **sans type, aucun profil de type** (depuis 3.1, 2026-10-25 : des
types voisins partagent volontairement des noms, comme `BRAKE_BUS` chez un
tracteur et sa remorque, et entreraient en conflit).

## 7. MD5

- Une empreinte par vue : MD5 d'un JSON canonique (clés triées, séparateurs
  compacts) (`md5.py:175-193`, `213-221`).
- Entrées par canal, dans l'ordre du fil (l'ordre des IDs) : `id`, `type`,
  `scope`, `direction`, `direction_or`, et `value` sous la forme du littéral C++
  émis (une valeur absente et sa valeur par défaut explicite donnent le même
  hash) (`_view_channel_canonical`, `md5.py`). Depuis le chantier 2.1 du link
  rework, `theme` et `infoName` n'y sont plus : un libellé de debug, ou un
  thème qui ne déplace pas le channel sur le fil, ne casse pas le contrat.
  `requires` n'y est pas (son effet est la présence du channel dans la vue).
- Le journal du build affiche une ligne par ensemble :
  `[combus_builder] set BASE    md5=<32 hex>  A9+D9`, puis `set NEXT` (avec le
  nombre de candidats) et `set SYSTEM`, suivies de `[<carte>]` en mode nœud.
- Le code du builder (`scripts/combus_builder/**/*.py` hors tests) et le hook
  font partie des sources de la génération : les modifier régénère les
  fichiers, comme un `.cb`.
- Vue `combus_system` seulement : canonisation `ONE_WAY`. `uplink`, `downlink`,
  `uplink_or` et `downlink_or` donnent le même état ; `both`, `both_or` et
  `none` restent distincts (`md5.py:62-109`, `137-157`, `183`). C'est ce qui
  permet à deux cartes d'un nœud d'avoir des rôles opposés sans désaccord.
- Vues `combus_base` et `combus` : directions brutes, `uplink` et `uplink_or`
  donnent des empreintes différentes.
- `_canonical_direction_for_hash` est réservée au hash, jamais à la génération
  (`md5.py:88-90`). `direction_or` est optionnel (rétro-compatibilité).
- Ensemble `BASE` : deux empreintes (link rework 2.2a). **Stable** : la vue sans
  ses candidats (`stable_view`), émise en `kCombusBaseComBusMd5`. **Next** : la
  vue entière (stables puis candidats), émise en `kCombusBaseComBusMd5Next`.
  Sans candidat, elles sont égales (le drapeau `candidate` n'est pas haché).
  Le même en-tête émet le nombre de channels de l'ensemble stable par type
  (`kCombusBaseStableAnalogCount`, `kCombusBaseStableDigitalCount`) : taille des
  trames d'un lien `BASE` hors du mode next (link rework 2.2b).
  La vue `combus_system` contient les candidats : deux cartes d'un nœud aux
  candidats différents n'ont pas le même MD5 `SYSTEM`.
- La version du projet n'entre pas dans le hash : `MAJOR`, `MINOR` et `DEV`
  (`project_version.h`, lus par `read_project_version`) sont des constantes de
  `combus_wire_common.h`, avec `kCombusHandshakeWirePayloadLen = 19` (16 octets
  de MD5 et les trois versions).
- Le hash change donc quand un de ces champs change, quand un canal entre ou
  sort d'une vue (via `requires`), ou quand l'ordre d'attribution des IDs change.
- Propagation selon le scope du channel modifié (conséquence de la composition
  des vues, §6) : un channel BASE change les trois empreintes ; un channel
  SYSTEM change `combus_system` et `combus`, pas `combus_base` ; un channel
  BOARD change seulement `combus`.
- En-têtes émis : `combus_wire_common.h` (constantes communes
  `kProjectVersionMajor`, `kProjectVersionMinor`,
  `kCombusHandshakeWirePayloadLen`) puis un en-tête par vue
  (`k<Vue>ComBusMd5[16]` et sa forme hexadécimale), qui inclut le premier. Les
  constantes sont `static constexpr` : la chaîne d'outils `xtensa-esp32-elf-g++`
  rejette `inline constexpr` à la portée d'espace de noms dans certaines
  configurations (commentaire `md5.py:38-39`).
- Version du projet : le builder lit `project_version.h` (racine du dépôt) et
  écrit les deux nombres dans `combus_wire_common.h`
  (`md5.read_project_version()`). Le fichier doit définir
  `PROJECT_VERSION_MAJOR` et `PROJECT_VERSION_MINOR` sous la forme
  `= <n>u;`, chacun entre 0 et 255 ; sinon le build s'arrête (`FATAL`, pas de
  valeur par défaut silencieuse). Le hook déclare le fichier comme source :
  le modifier régénère les fichiers. Sans version fournie (appel direct de
  `generate()` dans les tests), `md5.py` garde 0 et 1.

## 8. Sorties générées

Mono-carte : les 9 fichiers de `EXPECTED_ARTIFACTS` (`combus_scons_hook.py:107-122`) :
`combus.cpp`, `combus.h`, `combus_ids.h`, `combus_system_ids.h`,
`combus_system_md5.h`, `combus_md5.h`, `combus_base_ids.h`,
`combus_base_md5.h`, `combus_wire_common.h`.

Mode nœud : en plus, huit fichiers par carte, préfixés par le `board_id`
(`combus_<id>.h`, `combus_<id>.cpp`, `combus_<id>_ids.h`,
`combus_system_<id>_ids.h`, `combus_base_<id>_ids.h`, `combus_<id>_md5.h`,
`combus_system_<id>_md5.h`, `combus_base_<id>_md5.h`) ; `combus_wire_common.h`
n'est pas préfixé (`combus_scons_hook.py:198-229`). Les identifiants C++ ne sont
pas préfixés (`generator.py:76-78`).

Dossier de sortie (`combus_scons_hook.py:266-284`) :

- `Path(PROJECT_BUILD_DIR)/<env>/"combus_generated"` si `PROJECT_BUILD_DIR` est défini ;
- sinon `.pio/build/<env>/combus_generated`.

**Un dossier par env** (depuis le 2026-10-22). Avant, le dossier était partagé
par tous les envs. SCons décide de régénérer env par env, donc un env pouvait
compiler avec les fichiers écrits par un autre. C'était sans effet tant que tous
les envs généraient le même contenu ; depuis que `requires` écarte les channels
dont un flag manque, deux envs peuvent avoir des jeux de channels différents
(constaté : la Volvo compilée après l'env de test ne trouvait plus
`FAILSAFE_SOFT`). Le dossier n'est pas nettoyé par `-t clean` : pour forcer la
régénération d'un env, supprimer `<build_dir>/<env>/combus_generated`.
`scripts/combus_test_add_generated_source.py` suit la même règle. L'ancien dossier
partagé `<build_dir>/combus_generated` n'est plus utilisé et peut être supprimé.

Le CLI écrit par défaut dans `<racine>/out/combus_generated`
(`combus_builder.py:56`, `69`). Les dossiers `out\` et `out_a12_rev4_skip\` à la
racine sont des restes de ces sorties.

## 9. Hook SCons

- Une seule `env.Command` différée : la génération a lieu au build, quand
  `CPPDEFINES` est complet. `varlist=["CPPDEFINES", "CPPPATH", "LIBDEPS"]`
  (`combus_scons_hook.py:1062-1068`). Sources de la commande : les `.cb` et
  `.cbch` trouvés par `rglob` sous `PROJECT_DIR` (`combus_scons_hook.py:321-332`,
  `1036`).
- Pourquoi `pre:` et une commande différée (constats consignés dans l'ancienne
  roadmap, essais A15/A16, non rejoués ici) : en `pre:`, `CPPDEFINES` n'est pas
  encore peuplé ; en `post:`, la liste des sources est déjà figée. En
  enregistrant seulement une commande SCons au chargement du hook, la
  génération s'exécute plus tard avec l'environnement complet, avant la
  compilation des fichiers qui incluent les en-têtes générés. Les variables du
  `varlist` entrent dans la signature SCons : un changement de flags relance la
  génération, un build identique ne la relance pas (essais décrits dans
  l'ancienne roadmap, section « A16 » ; non rejoués ici). Une action
  `AddPostAction` serait trop tardive pour générer des en-têtes
  (`scripts/combus_builder_dump_ctx.py` l'utilise légitimement, voir §11).
- La résolution du dossier source du parser suit cet ordre
  (`parser.py:98-128`) : `env["PROJECT_SRC_DIR"]`, puis `env["PROJECT_DIR"]/src`,
  puis `project_root/src` (mode CLI), puis `cwd/src`. Le hook passe `env`, le
  CLI passe `project_root`.
- Anti-périmé : écriture dans `<dossier>.new`, contrôle de complétude
  (`_check_staging_complete`, `:302`), puis publication (`_publish`, `:291`).
  Toute erreur renvoie un code non nul à SCons.
- `COMBUS_BUILDER_SKIP` (`1`, `true`, `yes`, `on`) : l'action devient un no-op
  qui vérifie seulement la présence des 9 `EXPECTED_ARTIFACTS`, pas des fichiers
  par carte (`combus_scons_hook.py:252-259`, `911-927`). C'est un contournement
  explicite : il ne vérifie pas que les fichiers correspondent aux flags, aux
  `.cb` ni à la version du builder.
- En cas d'échec de génération, le build est interrompu avant toute
  compilation (code de retour non nul de l'action) ; les anciens artefacts
  restent sur disque mais ne sont pas utilisés par ce build. La publication
  `rmtree` puis `os.replace` (`_publish`, `:291-299`) laisse une courte fenêtre
  sans dossier : si le processus est tué à cet instant, le build suivant échoue
  sur un en-tête introuvable plutôt que d'utiliser un jeu incohérent.
- Un type de machine est déduit une fois pour tout le nœud ; si une carte en
  résout un autre, le build s'arrête (`combus_scons_hook.py:671-714`).

## 10. Limites et non implémenté

- **Dossier généré partagé** en mode nœud : compiler deux cartes de suite
  réécrit les fichiers non préfixés (dette acceptée, par sécurité).
- **Inversion BASE** (« ND5 ») : non traitée.
- **Fusion OR côté C++** : faite pour `both_or` (link rework 1.1b,
  `src/core/system/combus/protocol/both_or.md`), état dans
  `DigitalComBus.bothOr` (`BothOrMerge`) ; `uplink_or` et `downlink_or` restent
  déclaratifs.
- **Build réel à deux cartes** (« T4 ») : jamais testé.
- **Script YAML vers `platformio.ini`** : prévu, pas écrit. Il doit produire un
  env racine (configuration commune) et un sous-env par carte, mono-carte compris, avec
  `HAS_RUNLEVEL_MANAGER` sur une seule carte par machine.
- **Ancienne composition par `.inc`** : les deux `.inc` résiduels ont été supprimés le 2026-10-15.
  Le code C++ inclut les fichiers générés directement : `#include <combus.h>`,
  `#include <combus_ids.h>`, `#include <combus_base_ids.h>`, ou
  `src/core/config/machines/combus_types.h` qui fait `#include <combus_ids.h>`
  (le hook ajoute le dossier généré au chemin d'inclusion). Depuis le
  2026-10-18 il n'existe plus d'en-tête de redirection par machine. Attention :
  `combus.h` est unique dans le projet : l'ancien `src/core/system/combus/combus.h`
  (API `combus_init()`) s'appelle `combus_init.h` depuis le 2026-10-18.

**Mode nœud : noms de fichiers (corrigé et vérifié le 2026-10-15).** La carte
dont l'env est compilé reçoit les noms **non préfixés** (`combus.h`,
`combus_ids.h`, `combus_md5.h`, ... : ceux qu'inclut le code C++), les **autres
cartes du nœud** reçoivent les noms préfixés par leur `board_id`. Le dossier
généré est partagé par tous les envs : compiler une autre carte le réécrit, et
le build de l'env racine est refusé avec un message d'erreur explicite (la
racine n'est pas une carte). Vérifié par un vrai `pio run -e test_node_mainboard`
(env racine `test_node`, deux cartes) : le hook publie 17 fichiers (9 non
préfixés + 8 pour l'autre carte), le firmware se compile et se lie
(`SUCCESS`). Compiler l'env racine `test_node` donne bien l'erreur explicite
(« is the root env of a node ... Build one of its boards instead »). L'env
`test_node_extboard` n'est pas fait pour être compilé (seulement résolu
statiquement). Attention : ne pas surcharger `PLATFORMIO_BUILD_DIR` avec un
chemin du projet, le « è » de « Modèlisme » fait échouer l'édition de liens
(`platformio.ini:28` place déjà le build hors projet pour cette raison).
Le contrôle de complétude est couvert par `test_hook.py`.

> Le filtre par type de machine est **actif** depuis le 2026-10-20 : le flag
> `MACHINE_TYPE_DUMPER_TRUCK` est passé en `-D` dans `platformio.ini`
> (`[volvo_A60H_id]` et `test_combus_loopback`), et non plus `#define` dans
> l'en-tête de la machine. Le hook journalise `ND8 machine-type filter active`.
> Constaté sur la Volvo et `test_node_mainboard` : fichiers générés identiques
> à l'état précédent (seul le commentaire `defines=27` devient `28`). Les types
> `excavator` et `loader` sont déclarés dans `MACHINE_TYPE_FLAGS` mais n'ont
> aucun `.cb`. Tout nouvel env d'une machine doit porter son `-D MACHINE_TYPE_*`.

> `scripts/combus_md5.py` n'existe pas : il n'est cité que dans des docs.

## 11. Lancer les tests

Console Windows cp1252 : toujours `PYTHONIOENCODING=utf-8`. Le dossier entier
se lance d'un coup, ou un fichier à la fois :

```text
set "PYTHONIOENCODING=utf-8"
python -m pytest scripts/combus_builder/tests
python -m pytest scripts/combus_builder/tests/test_nd2_diffusion.py
```

Le lancement global plantait avec `INTERNALERROR: I/O operation on closed file`
parce que deux scripts remplaçaient `sys.stdout` à l'import. Ces scripts ont été
convertis ou supprimés (2026-10-15).

Fichiers de `scripts/combus_builder/tests/` :

| Fichier | Teste |
|---|---|
| `test_canon.py` | validation des channels, fusion, tri canonique |
| `test_value.py` | champ `value` (validation, rendu C++) |
| `test_flags.py`, `test_coherence.py` | `BuildContext`, cohérence avec `idedata` |
| `test_machine_type_filter.py` | filtre par type de machine |
| `test_signature_consistency.py` | signatures `env=` / `project_root=` de `discover_and_parse` |
| `test_nd1_integration.py`, `test_nd1_multiboard.py` | nœud, cartes, génération par carte |
| `test_nd2_diffusion.py`, `test_nd6_both_or.py` | diffusion SYSTEM, `both_or` |
| `test_mother_not_a_board.py` | l'env racine (« mère ») n'est pas une carte |
| `test_generator.py` | vues, IDs, rendu C++, compilation |
| `test_md5.py` | empreintes, canonisation `ONE_WAY` |
| `test_candidates.py` | channels candidats, MD5 stable et next, champ `DEV` (link rework 2.2a) |
| `test_peer_links.py` | liaisons entre nœuds : ensemble, ordre, tables, MD5 symétrique (link rework 3.1a) |
| `test_node_direction.py` | sens dans le nœud, `downlink_pt` (link rework 2.4) |
| `test_hook.py` | fonctions utilitaires du hook SCons (sans SCons) |
| `test_stress.py` | tests de propriétés sur des `.cb` aléatoires (voir plus bas) |
| `_cpp_compiler.py` | recherche d'un compilateur C++ (aide, pas un test) |

État au 2026-10-15 : 724 tests réussis, aucun `xfail`, aucun ignoré.

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

Un passage à 1500 germes (15 000 cas, environ 2 minutes) a réussi le
2026-10-15. Il a révélé deux défauts du code de production, corrigés le même
jour (le test correspondant reste en place) :

- `canon/channels.py` : un `type`, `scope` ou `theme` qui est une liste ou un
  mapping levait `TypeError: unhashable type` au lieu de
  `ChannelValidationError` ; le validateur vérifie maintenant `isinstance(str)`
  d'abord ;
- `parser.py` : un fichier qui n'est pas en UTF-8 levait `UnicodeDecodeError` ;
  c'est maintenant un `ParseError` (`cannot read file`).

Les tests de compilation utilisent `g++`, `clang++` ou, à défaut, la chaîne
PlatformIO `xtensa-esp32-elf-g++` (`-fsyntax-only`) ; sans compilateur ils sont
ignorés (`skip`) au lieu d'échouer.

Outils de cohérence avec PlatformIO :

- `python -m scripts.combus_builder.coherence <env>...` compare le
  `BuildContext` acquis par le builder aux définitions réelles renvoyées par
  `pio run -t idedata` (`coherence.py:408-457` ; `--no-strict` pour seulement
  rapporter ; sans argument, tous les envs de `platformio.ini`).
- `scripts/combus_builder_dump_ctx.py` est un hook de fin de build qui écrit
  `combus_ctx.json` dans le dossier de build, pour comparer le contexte du vrai
  build à `idedata` par deux invocations `pio` séparées
  (`flags.py:382-405`). Il n'est référencé ni par `platformio.ini` ni par le
  hook principal ; il sert aux tests d'intégration.
- Les tests d'intégration lourds de `test_flags.py` ne s'exécutent qu'avec
  `COMBUS_A4_INTEGRATION=1` (`test_flags.py:610-613`).

Un test doit échouer par `assert` ou par exception. Un test qui `return False`
passe à vide sous pytest (c'était le cas de 11 tests de diffusion et de nœud,
corrigés le 2026-10-15). Pour vérifier qu'un test « mord », faire échouer le
code volontairement (en mémoire ou par modification temporaire) et constater
l'échec. `scripts` n'a pas de
`__init__.py` (paquet d'espace de noms) : en cas de doute sur l'origine d'un
import, afficher `module.__file__`.
