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
        │                       [mode nœud] diffusion.py (LOCAL entre cartes)
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

### `requires` : actif ou erreur selon le mode

- **Mode nœud** (diffusion) : une déclaration dont le `requires` n'est pas
  satisfait pour une carte est **écartée** pour cette carte
  (`_is_active_for_board`, `diffusion.py:267-298`).
- **Mode mono-carte** : `generate()` appelle `resolve_requires`
  (`generator.py:263-286`), qui **lève `RequiresResolutionError`** dès qu'un
  channel exige un flag absent. Rien ne filtre en amont dans le hook
  (`combus_scons_hook.py:721-745`). Tests : `test_generator.py:310-332`, `:993`.
- Le docstring de `resolve_requires` (« Filter out channels… ») dit « filtrer » ;
  le code lève une erreur.
- Conséquence connue : l'env `remotes` échoue au build avec
  `channel 'FAILSAFE' requires ['HAS_FAILSAFE']` (rapporté par l'ancienne
  roadmap ; cohérent avec le code ci-dessus).

## 3. Carte des modules

| Module | Rôle |
|---|---|
| `parser.py` | Cherche récursivement `.cb` et `.cbch` sous `PROJECT_SRC_DIR` (défaut `src/`), exclut `.pio`, `.git`, `__pycache__`, `node_modules` (`parser.py:72-91`, `135-187`). Charge le YAML (`safe_load`), exige un mapping en racine. |
| `machine_type.py` | Détecte le type de machine via un unique flag `MACHINE_TYPE_*` parmi `DUMPER_TRUCK`, `EXCAVATOR`, `LOADER` (`machine_type.py:78-82`). Deux flags : `AmbiguousMachineTypeError`. Filtre les `.cb` situés sous `core/config/machines/<autre_type>/`. |
| `flags.py` | `BuildContext` : flags `-D` vus par le compilateur (`defines`, `defines_with_value`). Sert à résoudre `requires`. |
| `node_context.py` | Détection du nœud par noms d'envs `<racine>_<board_id>` (`node_context.py:106-195`). Résolution statique du contexte de chaque carte, sans compiler. |
| `diffusion.py` | Diffusion des canaux LOCAL entre les cartes d'un nœud (§5). |
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

## 5. Diffusion LOCAL entre cartes

Entrée : toutes les déclarations LOCAL actives par carte (une déclaration est
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
`DiffusionLocalConflictError` avant toute autre règle. Les variantes `*_or`
suivent la règle de leur famille (`uplink_or` compte comme `uplink`, etc.) ;
`both` et `both_or` sur le même `id` : accepté avec un avertissement
« divergence » (`diffusion.py:420-443`), `both_or` mélangé avec
`uplink`/`downlink` : erreur fatale (`diffusion.py:445-448`).

La diffusion REMOTE (inversion côté télécommande, « ND5 ») n'existe pas.

Pourquoi diffuser : le ComBus LOCAL doit être identique sur toutes les cartes
d'un même nœud. Un module consommateur sur la carte B doit pouvoir lire un
canal produit seulement par un module actif sur la carte A, sans activer ce
module sur chaque carte. Un channel qui doit rester privé à une carte se
déclare en `SYSTEM` : la diffusion ne traite que le scope LOCAL, et les
déclarations SYSTEM ne sont reprises que sur les cartes où leur `requires` est
satisfait (`diffusion.py:826-868`).

Note sur les fichiers : chaque carte reçoit ses propres fichiers préfixés (§8),
y compris les fichiers d'identifiants. Le code ne génère donc pas « un seul
jeu d'IDs par nœud », contrairement à ce que supposait l'ancienne roadmap de
diffusion (qui prévoyait un seul `combus_ids` par nœud).

Pourquoi l'empreinte LOCAL est canonisée (« ND7 ») : la diffusion inverse
`uplink` et `downlink` entre cartes par construction. Sans canonisation, deux
cartes correctement configurées auraient des MD5 LOCAL différents dès qu'un
channel asymétrique existe (faux désaccord garanti).

## 6. Attribution des IDs et vues

Trois vues, avec le même espace d'IDs (`generator.py:401-419`) :

| Vue | Contenu | Fichiers émis |
|---|---|---|
| `combus` | REMOTE + LOCAL + SYSTEM | `combus.h`, `combus.cpp`, `combus_ids.h` |
| `combus_local` | REMOTE + LOCAL | `combus_local_ids.h` seulement (`generator.py:812-819`) |
| `combus_remote` | REMOTE | `combus_remote_ids.h` seulement |

Deux ordres distincts, à ne pas confondre :

- `_VIEW_SCOPE_ORDER` (`generator.py:296`) : REMOTE 0, LOCAL 1, SYSTEM 2. Sert
  à **numéroter les IDs**, dans `_select_view` uniquement (`generator.py:338`, `346`).
- `SCOPE_ORDER` (`canon/channels.py:97`) : LOCAL 0, REMOTE 1, SYSTEM 2. Sert au
  **tri canonique** `(scope, type, theme, id)` (`canon/channels.py:208-211`, `787-789`).

Règle d'attribution (`generator.py:332-386`) : les canaux `analog` et `digital`
sont triés séparément, par `(scope, theme, id)` avec l'ordre `_VIEW_SCOPE_ORDER`.
Chaque type a son compteur, qui part de 0. Les REMOTE ont donc les plus petits
IDs de leur type, puis LOCAL, puis SYSTEM. Un canal garde le même ID dans toutes
les vues où il apparaît. Les vues `combus_local` et `combus_remote` sont ainsi
des préfixes de `combus`.

À quoi sert chaque vue (`md5.py:3-9`, `combus_protocol.cpp:55-88`) :

- `combus_remote` : ce qui circule sur le fil entre nœuds ;
- `combus_local` : base d'alignement entre cartes d'un même nœud. Deux cartes
  peuvent porter des channels `SYSTEM` différents sans que leurs contrats
  LOCAL divergent, car cette vue n'inclut pas SYSTEM ;
- `combus` : la vue complète de la carte (référence locale).

L'ordre des IDs est un contrat : changer l'ordre change le protocole sans
changer les noms. Le MD5 porte sur la définition complète de chaque channel
(pas seulement l'ID), donc un changement de `direction` à ID constant est
détecté.

`CH_COUNT` termine chaque énumération. `View.wire_end` (nombre de canaux
REMOTE) existe côté Python seulement ; les constantes C++ `ComBusWireEnd*`
ne sont plus émises (`generator.py:588-595`).

> À VÉRIFIER : les docstrings `generator.py:55` et `md5.py:12` décrivent le tri
> comme `(scope, type, theme, id)`. Le code d'attribution des IDs trie par type
> d'abord, puis `(scope, theme, id)` (`generator.py:332-350`). Le code fait foi.

## 7. MD5

- Une empreinte par vue : MD5 d'un JSON canonique (clés triées, séparateurs
  compacts) (`md5.py:175-193`, `213-221`).
- Entrées par canal : `id`, `type`, `scope`, `theme`, `direction`,
  `direction_or`, `infoName` (`md5.py:164-172`). `value` et `requires` n'y sont
  pas.
- Vue `combus_local` seulement : canonisation `ONE_WAY`. `uplink`, `downlink`,
  `uplink_or` et `downlink_or` donnent le même état ; `both`, `both_or` et
  `none` restent distincts (`md5.py:62-109`, `137-157`, `183`). C'est ce qui
  permet à deux cartes d'un nœud d'avoir des rôles opposés sans désaccord.
- Vues `combus_remote` et `combus` : directions brutes, `uplink` et `uplink_or`
  donnent des empreintes différentes.
- `_canonical_direction_for_hash` est réservée au hash, jamais à la génération
  (`md5.py:88-90`). `direction_or` est optionnel (rétro-compatibilité).
- La version du projet n'entre pas dans le hash : constantes dans
  `combus_wire_common.h` (`md5.py:22-24`).
- Le hash change donc quand un de ces champs change, quand un canal entre ou
  sort d'une vue (via `requires`), ou quand l'ordre d'attribution des IDs change.
- Propagation selon le scope du channel modifié (conséquence de la composition
  des vues, §6) : un channel REMOTE change les trois empreintes ; un channel
  LOCAL change `combus_local` et `combus`, pas `combus_remote` ; un channel
  SYSTEM change seulement `combus`.
- En-têtes émis : `combus_wire_common.h` (constantes communes
  `kProjectVersionMajor`, `kProjectVersionMinor`,
  `kCombusHandshakeWirePayloadLen`) puis un en-tête par vue
  (`k<Vue>ComBusMd5[16]` et sa forme hexadécimale), qui inclut le premier. Les
  constantes sont `static constexpr` : la chaîne d'outils `xtensa-esp32-elf-g++`
  rejette `inline constexpr` à la portée d'espace de noms dans certaines
  configurations (commentaire `md5.py:38-39`).
- Version du projet : le builder écrit des valeurs **codées en dur**, majeur 0
  et mineur 1 (`md5.py:264-265`, commentaire `md5.py:296`). Le fichier
  `project_version.h` (racine du dépôt, mêmes valeurs 0 et 1) n'est lu par
  aucun script ni aucun code C++ (recherche `project_version` dans `scripts/`
  et dans le dépôt) ; son commentaire cite encore `scripts/combus_md5.py`, qui
  n'existe plus. Modifier `project_version.h` ne change donc pas la version
  transmise dans le handshake.

## 8. Sorties générées

Mono-carte : les 9 fichiers de `EXPECTED_ARTIFACTS` (`combus_scons_hook.py:107-122`) :
`combus.cpp`, `combus.h`, `combus_ids.h`, `combus_local_ids.h`,
`combus_local_md5.h`, `combus_md5.h`, `combus_remote_ids.h`,
`combus_remote_md5.h`, `combus_wire_common.h`.

Mode nœud : en plus, huit fichiers par carte, préfixés par le `board_id`
(`combus_<id>.h`, `combus_<id>.cpp`, `combus_<id>_ids.h`,
`combus_local_<id>_ids.h`, `combus_remote_<id>_ids.h`, `combus_<id>_md5.h`,
`combus_local_<id>_md5.h`, `combus_remote_<id>_md5.h`) ; `combus_wire_common.h`
n'est pas préfixé (`combus_scons_hook.py:198-229`). Les identifiants C++ ne sont
pas préfixés (`generator.py:76-78`).

Dossier de sortie (`combus_scons_hook.py:266-284`) :

- `Path(PROJECT_BUILD_DIR)/"combus_generated"` si `PROJECT_BUILD_DIR` est défini ;
- sinon `.pio/build/<env>/combus_generated`.

Avec le `build_dir` du propriétaire (`platformio.ini:28`, sous
`%LOCALAPPDATA%\Temp\PlatformIO\build\Open_Node_RC_Core`), ce dossier est
**partagé par tous les envs** et **n'est pas nettoyé par `-t clean`**. Pour
forcer la régénération, supprimer `combus_generated`. Fragilité à traiter avant
le multi-carte réel.

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
- **Inversion REMOTE** (« ND5 ») : non traitée.
- **Fusion OR côté C++** : non faite. `DigitalComBus.bothOr` est émis mais
  `combus_frame_apply` ne le lit pas (`combus_defs.h:130-137`).
- **Build réel à deux cartes** (« T4 ») : jamais testé.
- **Script YAML vers `platformio.ini`** : prévu, pas écrit. Il doit produire un
  env racine (configuration commune) et un sous-env par carte, mono-carte compris, avec
  `HAS_RUNLEVEL_MANAGER` sur une seule carte par machine.
- **Ancienne composition par `.inc`** : les deux `.inc` résiduels ont été supprimés le 2026-10-15.
  Le code C++ inclut les identifiants générés par
  `src/core/config/machines/combus_types.h`, qui fait `#include <combus_ids.h>`
  (le hook ajoute le dossier généré au chemin d'inclusion).
- **`project_version.h` non relié au builder** (voir §7).

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

> À VÉRIFIER : le filtre par type de machine lit un flag `MACHINE_TYPE_*` dans
> le `BuildContext`. `platformio.ini:90-95` indique que `MACHINE_TYPE_*` est
> défini dans un en-tête de configuration machine ; savoir si ce flag est alors
> visible du builder n'est pas établi. Les types `excavator` et `loader` sont
> déclarés dans `MACHINE_TYPE_FLAGS` mais aucun `.cb` n'existe sous leur dossier.

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
| `test_nd2_diffusion.py`, `test_nd6_both_or.py` | diffusion LOCAL, `both_or` |
| `test_mother_not_a_board.py` | l'env racine (« mère ») n'est pas une carte |
| `test_generator.py` | vues, IDs, rendu C++, compilation |
| `test_md5.py` | empreintes, canonisation `ONE_WAY` |
| `test_hook.py` | fonctions utilitaires du hook SCons (sans SCons) |
| `test_stress.py` | tests de propriétés sur des `.cb` aléatoires (voir plus bas) |
| `_cpp_compiler.py` | recherche d'un compilateur C++ (aide, pas un test) |

État au 2026-10-15 : 724 tests réussis, aucun `xfail`, aucun ignoré.

### Tests de stress (`test_stress.py`)

Chaque test tire des déclarations `.cb` aléatoires mais valides, avec un germe
(`seed`) fixe : un échec est reproductible. Invariants vérifiés :

- sortie identique entre deux exécutions et quel que soit l'ordre des fichiers
  et des channels ;
- numérotation des IDs : par type, de 0 à n-1, REMOTE puis LOCAL puis SYSTEM ;
  un channel garde son ID dans toutes les vues ; les vues sont des préfixes ;
- matrice MD5 : renommer un channel REMOTE change les trois empreintes, LOCAL
  deux, SYSTEM une seule ; `value` et un `requires` satisfait ne changent rien ;
  `uplink` / `downlink` restent distincts sur la vue REMOTE ;
- tout doublon d'`id` est refusé ;
- valeurs absurdes dans chaque champ : seule l'exception de validation est
  permise ;
- fichiers YAML aléatoires : seule `ParseError` est permise ;
- diffusion LOCAL (2 à 5 cartes, env racine présent ou non) comparée à un oracle écrit
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
