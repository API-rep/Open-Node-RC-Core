# Note de passation ComBus Builder

Document de travail : matière pour rédiger la doc, à supprimer une fois la doc écrite.

Références `fichier:ligne` vérifiées dans le dépôt (branche main). Les autres points sont marqués « rapporté, non revérifié » ou placés en section D.

## A. Architecture (faits)

### Scopes et directions
- Scopes : REMOTE, LOCAL, SYSTEM.
- Directions : uplink, downlink, both, none, uplink_or, downlink_or, both_or.
- Ordre des scopes dans la vue : `_VIEW_SCOPE_ORDER` (scripts/combus_builder/generator.py:296) = REMOTE 0, LOCAL 1, SYSTEM 2. REMOTE d'abord (ordre « wire-end »). Usages : voir « Résolus par l'audit du 2026-10-14 » ci-dessous.

### Diffusion LOCAL entre N>=2 cartes d'un même nœud
- Maximum 1 uplink. 2 uplinks = erreur fatale `DiffusionConflictError` (scripts/combus_builder/diffusion.py:104).
- 1 uplink : les cartes sans déclaration sont en downlink.
- 0 uplink + downlink : avertissement, cartes absentes en none.
- both mélangé avec up/down : erreur fatale.

### L'env racine (« mère » dans le code) n'est pas une carte
- L'env racine (board_id=None, appelé « mère » dans le code) porte seulement la configuration commune du nœud : ce n'est PAS une carte et il ne correspond à aucun circuit. La carte principale de la machine est un sous-env (`<racine>_mainboard`), donc une vraie carte.
- Elle est exclue de la diffusion uniquement en mode node, via le point unique `diffusion_boards` (scripts/combus_builder/diffusion.py:589-597, condition `node.is_node and None in node.boards`).
- Le hook saute les cartes board_id None en mode node : `if node.is_node and board_id is None: continue` (scripts/combus_scons_hook.py:835).

### Mono-carte
- Sortie identique avant/après le correctif « mère != carte » : MD5 sur 11 fichiers via le vrai hook, env volvo_A60H_bruder. Rapporté, non revérifié.
- Les 11 fichiers = les 9 EXPECTED_ARTIFACTS + combus.cpp.d + combus.cpp.o. Rapporté, non revérifié.

### Hook SCons (scripts/combus_scons_hook.py)
- Enregistré par `extra_scripts = pre:scripts/combus_scons_hook.py` ([env], platformio.ini:53).
- Une seule `env.Command` différée, `varlist=["CPPDEFINES", "CPPPATH", "LIBDEPS"]` (hook:1067).
- Staging `<out_dir>.new` puis publication (hook:717, `_publish` hook:291, `_check_staging_complete` hook:302).
- Variable COMBUS_BUILDER_SKIP : action no-op, les artefacts doivent déjà exister (hook:252, 911).
- Mono-carte : liste `EXPECTED_ARTIFACTS` (hook:107). En mode node, des cibles par carte s'ajoutent (`_expected_artifacts_for_node`, hook:198).

### Dossier de sortie
- `Path(build_dir)/"combus_generated"` si build_dir est défini (hook:274), sinon `.pio/build/<env>/combus_generated` (hook:279).
- Avec le build_dir du propriétaire (…\Temp\PlatformIO\build\Open_Node_RC_Core\combus_generated), le dossier est PARTAGÉ par tous les envs.
- Il n'est pas nettoyé par `-t clean` : le supprimer pour forcer la régénération.
- Fragilité pour le multi-carte (voir C).

### Fichiers générés (mono-carte)
combus.cpp, combus.h, combus_ids.h, combus_local_ids.h, combus_local_md5.h, combus_md5.h, combus_remote_ids.h, combus_remote_md5.h, combus_wire_common.h.

### Objectif suivant
- Script YAML -> platformio.ini.
- Env racine = configuration commune. Chaque carte = sous-env, y compris en mono-carte.
- HAS_RUNLEVEL_MANAGER unique par machine, normalement sur la carte principale.

### Résolus par l'audit du 2026-10-14 (lecture du code, rien exécuté)
- `_VIEW_SCOPE_ORDER` ne sert qu'à numéroter les IDs (`generator.py:338`, `346`), pas à ordonner des champs. Compteurs d'ID séparés par type, tri `(scope, theme, id)` (`generator.py:332-386`). Autre ordre, distinct : `SCOPE_ORDER` (LOCAL 0, REMOTE 1, SYSTEM 2) pour le tri canonique (`canon/channels.py:97`, `208-211`).
- Filtrage TX/RX par direction : INTROUVABLE dans le runtime. Aucun `.direction` lu dans `src/` ; `direction` ne sert qu'au générateur et au MD5. Le TX envoie tous les canaux du préfixe de la vue du lien (`combus_tx.cpp:122-133`, `combus_frame.cpp:124-125`).
- runLevel : pas dans l'en-tête (`combus_frame_defs.h:71-75`). En-tête = `nAnalog, nDigital, seq, flags` après `SOF` ; il voyage comme canal `RUNLEVEL` (`runlevel.cb`).
- `combus_frame_apply` : un seul appelant en production, `combus_rx.cpp:223`, avec `ChanLayer::REMOTE`. Définition `combus_frame.cpp:338`.
- `WIRE_END` : notion Python (`View.wire_end`, `generator.py:186-199`) ; constantes C++ plus émises (`generator.py:588-595`). Le TX utilise `CH_COUNT` de la vue du lien. Égaux sur la vue REMOTE seulement.
- Clés `.cb` : celles de `ALLOWED_FIELDS` (`canon/channels.py:51-60`). `module:` en tête de fichier est ignoré, dans un channel il est rejeté (`tests/test_canon.py:232-253`). `schema_version` : INTROUVABLE. `.cbch` : découvert comme `.cb` (`parser.py:72-83`) ; seule la section `channels:` est lue ; `chains:` non traité.
- `COMBUS_BUILDER_SKIP` : n'exige que les 9 `EXPECTED_ARTIFACTS` (`combus_scons_hook.py:911-927`). En mode node, cibles = 9 + 8 par carte (`:198-229`, `:1004-1023`).
- `.inc` : les deux fichiers de `src/core/config/machines/common/combus/` (vides, DEPRECATED, référencés nulle part) ont été supprimés le 2026-10-15. `scripts/combus_md5.py` n'existe pas ; cité seulement dans des docs.
- Doc UART : chemin mort `src/sound_module/init/com/combus_uart_init.cpp` (signalé dans la doc). Autres chemins existants.
- « 9 cibles / 21 sources » : 9 = `len(EXPECTED_ARTIFACTS)` (`combus_scons_hook.py:107-122`) ; 21 = nombre de `.cb` sous `src/` (comptés par recherche de fichiers). Le hook les compte par `rglob` sur tout `PROJECT_DIR` (`:1036`), donc le chiffre exact affiché peut différer.
- Le document principal ComBus est `doc/~combus_v2.md` (section « ComBus Builder » ajoutée avant « Phases d'implémentation »).
- `requires` non satisfait : en mono-carte, `RequiresResolutionError` (erreur de build) ; en mode nœud, déclaration écartée pour la carte (`generator.py:263-286`, `diffusion.py:267-298`, `tests/test_generator.py:310-332`). Explique l'échec de l'env `remotes` (`channel 'FAILSAFE' requires ['HAS_FAILSAFE']`, rapporté par l'ancienne roadmap).
- Handshake : trame de 24 octets, rafale 3 trames / 100 ms, MD5 de la vue du lien, invalidation du contrat sur `FAILSAFE` (`combus_frame_protocol.md` §8, sources dans le document).
- `combus_rx_update()` n'a aucun appelant dans `src/` ; le lien UART de la machine n'a pas de `target` (`combus_frame_protocol.md` §9).
- Tri des anciens documents `combus_v2 - *` (2026-10-15) : fait ; fichiers périmés supprimés, idées conservées dans `doc/roadmap.md`. Le document de tri a été supprimé.

## B. Travail effectué (d'après git log)

- ND1 : dimension « node » formalisée.
- ND2 : diffusion généralisée à N>=2 cartes. Règle 6 « canal mort » = avertissement.
- ND3 : terminé.
- ND4 : le pass-through SYSTEM est filtré par `requires` par carte.
- ND4 bis : un canal REMOTE ne peut pas déclarer `requires`.
- ND6 : drapeau anti-oscillation both_or (Python uniquement).
- ND7 : canonisation ONE_WAY pour le hash de la vue LOCAL.
- A2.1 : représentation direction_or.
- Filtre par type de machine. both_or dans le MD5.
- `_canonical_direction_for_hash` (scripts/combus_builder/md5.py:62) reste rétro-compatible (direction_or optionnel).
- Correctif « mère != carte » : commit 036043a, merge 19e2b50. Tests : scripts/combus_builder/tests/test_mother_not_a_board.py (3 tests, mordent sur l'ancien code).
- Base de tests remise au vert : 6f38219, merge e604fae.
- Attente périmée corrigée dans test_coherence : 5df3152.
- sound_module et envs sound_node_* retirés : da982a8.
- main poussé sur origin (5df3152).
- État des tests : voir CLAUDE.md et section C.

## C. Limites et non-implémenté

- Mode nœud : la carte compilée reçoit les noms non préfixés, les autres cartes les noms préfixés (corrigé le 2026-10-15, `combus_scons_hook.py` + `generator._emit_md5_files`). Dossier généré partagé entre envs (dette acceptée). Branche combus-board-aliases supprimée.
- Inversion REMOTE (ND5) : non traitée.
- Fusion OR côté C++ dans combus_frame_apply : non faite.
- Build à deux cartes réel (T4) : pas encore testé. Sera couvert par le futur script YAML -> platformio.ini.
- Dossier combus_generated partagé entre sous-envs : fragilité à traiter.
- Tests : remis en état le 2026-10-15. 724 tests réussis (aucun `xfail`), lancement global OK (`python -m pytest scripts/combus_builder/tests`). Anciens rouges corrigés (test_generator 9, test_md5 3, test_value 30 : fixtures et attentes périmées, aucun bug du builder). `test_signature_consistency` converti en pytest ; `test_a12_rev4` et `test_b4_phase_b` supprimés (ancien hook, 13 artefacts) et remplacés par `test_hook.py`. 11 tests de diffusion/nœud qui passaient à vide (`return False`) échouent maintenant réellement.
- Env `remotes` : échec de build préexistant.
- Résidus à la racine : dossiers `out\` et `out_a12_rev4_skip\`. Le CLI scripts/combus_builder.py écrit par défaut dans `<racine>/out/combus_generated`.
- Tests de robustesse : première base écrite le 2026-10-15 (`test_stress.py`, README builder §11). Deux défauts du code relevés puis corrigés le 2026-10-15 (marqueurs `xfail` retirés) : `canon/channels.py` (`type` ou `scope` en liste/mapping : `TypeError` au lieu de `ChannelValidationError`, lignes `ctype not in VALID_TYPES` et `cscope not in VALID_SCOPES`) et `parser.py` (`parse_yaml_file` : fichier non UTF-8 : `UnicodeDecodeError` au lieu de `ParseError`). À étendre plus tard : tests de stress du hook en mode nœud, du CLI, des alias quand ils existeront.

## D. À VÉRIFIER avant d'écrire la doc

Ne PAS formuler comme faits. Les points résolus ont été déplacés en A.

- Mode nœud, build réel du 2026-10-15 : hook OK (17 fichiers publiés) et compilation OK ; édition de liens échouée sur le chemin absolu accentué du `.map` (hors builder). Refus de l'env racine non exercé en réel (SSL git). Une carte sans flag `INPUT_*` (extboard de test) ne compile pas : config, pas builder. Test envs `test_node*` toujours dans `platformio.ini` : à garder ou retirer (décision propriétaire).
- Filtre par type de machine : `platformio.ini:90-95` dit que `MACHINE_TYPE_*` est défini dans un en-tête de config machine ; savoir si le builder voit ce flag n'est pas établi.
- Effet du layer `ChanLayer::REMOTE` passé à `combus_frame_apply` (`combus_rx.cpp:223`) sur les canaux LOCAL : `_layer_ok` (`combus_access.cpp:42-45`) semble les refuser, alors que le commentaire `combus_rx.cpp:209-221` dit l'inverse. Non exécuté.
- Direction : aucun filtrage TX/RX aujourd'hui. Décision de conception à prendre par le propriétaire avant de documenter autre chose que « donnée du contrat ».
- Commentaires périmés (`combus_frame.h`/`.cpp` runLevel et taille d'en-tête, `combus_frame.cpp` « sound node », `combus_handshake.h`, `combus_protocol.h`, `generator.py` tri/dossier/`sound_module`/`combus_struct.h`, `md5.py:12`, `__init__.py`, `diffusion.py` hors périmètre) : corrigés le 2026-10-15, commentaires seulement.
- Références vivantes à `sound_module` / `SOUND_NODE` dans `src/` (par exemple `light_core.cpp:24`, `light_servo_beacon.cpp:11-13`, `neopixel.cpp:17`, `esc_dev.cpp:18`, `dumper_truck_config.h:80`) alors que `src/sound_module/` n'existe plus. Décision du 2026-10-15 : documenté seulement, à traiter avec le chantier « Convergence des environnements Machine / Sound » de `doc/roadmap.md`.
- Chemins morts dans `doc/~combus_v2.md` : `include/struct/combus_struct.h` (l.273 d'origine), `include/core/system/combus/combus_defs.h` (le fichier est `src/core/system/combus/combus_defs.h`). Non corrigés (fichier du propriétaire).
- Nettoyage de code (décision du 2026-10-15 : consigné ici seulement) : deux en-têtes de redirection identiques vers `<combus_remote_ids.h>`, `src/core/config/machines/dumper_truck/combus_ids_remote.h` (inclus par aucun fichier) et `.../dumper_truck/combus/combus_ids_remote.h` (inclus par `dumper_truck_config.h:59`). Leur commentaire affirme que `DigitalComBusRemoteID` n'est pas généré, alors que `combus_tx.cpp:123` et `combus_uart_init.cpp:32` l'utilisent.
- Commentaires qui pointaient vers des docs supprimés (`md5.py:239`, `platformio.ini:9` et `:36`, et les « WIP combus_v2 § » / « WIP - Failsafe module design.md §12.13 » de `protocol/frame/`) : réécrits le 2026-10-15, commentaires seulement.
- Les deux fichiers « hook mechanism » (identiques) ont été supprimés du disque le 2026-10-15 sur décision du propriétaire (récupérables dans git, commit `7d8aed8`).
- `project_version.h` (racine) n'est lu par aucun script ni code ; le builder écrit 0.1 en dur (`md5.py:264-265`). Décision du 2026-10-15 : à relier au builder (tâche dans `doc/roadmap.md`).
- Idées jetées le 2026-10-15 (`schema_version`, mode `--check`, manifest du SKIP) : retirées des docs. Filtrage par direction, timeout / relance / accusé de réception et QoS : déplacés dans `doc/roadmap.md` (2026-10-15) ; cache de handshake multi-entrées abandonné. Les chaînes `.cbch` ont été retirées des docs du builder et déplacées dans `doc/roadmap.md` (décision du 2026-10-15).

## E. Docs à produire

- README développeur : scripts/combus_builder/README.md (écrit le 2026-10-14).
- Doc de protocole : src/core/system/combus/protocol/frame/combus_frame_protocol.md (écrit le 2026-10-14 dans protocol/, déplacé dans frame/ par le propriétaire ; `~README.md` non touché).
- Section ComBus Builder dans doc/~combus_v2.md (insérée avant « Phases d'implémentation »).
- Doc utilisateur .cb : doc/configuration/cb_syntax.md (déplacée et renommée le 2026-10-15 ; non suivie, appartient au propriétaire). À compléter : ligne extension/recherche, section MD5 réduite, « l'env racine n'est pas une carte », alias une fois implémentés.
- Docs existantes à considérer : doc/combus_v2 - YAML implementation*.md, src/core/system/combus/protocol/uart_workflow.md (`doc/ND2_investigation_report.md` a été supprimé par le propriétaire).
- Consigne : ne jamais présenter l'env racine (« mère ») comme une carte. Ne documenter comme acquis que ce qui est dans A et B.

### Titres de doc/~combus_v2.md (niveaux 1 à 3, ligne : titre)
Relevé en lecture seule. Le fichier n'est pas modifié. Le choix de l'emplacement revient au propriétaire.

- 1 : # ComBus v2 — Migration Roadmap
- 3 : ## Objectif
- 18 : # Architecture cible
- 20 : ## Layering
- 49 : ## Groupes fonctionnels
- 68 : ## Métadonnées transport
- 79 : ## ComBus Processors
- 89 : # Phases d'implémentation
- 91 : ## Phase 1 — Layering (ChanLayer)
- 93 : ### Objectif
- 107 : ### Implémentation
- 123 : ### Point d'attention
- 141 : ### Questions ouvertes
- 147 : ### Validation
- 153 : ### Prochaine étape
- 156 : ## Phase 1 — Structure multisource (Layering) - Implémentation détaillée
- 158 : ### Objectif
- 164 : ### Implémentation
- 185 : ### Point d'attention
- 192 : ### Fichiers et sections à modifier
- 205 : ### Validation
- 212 : ### Documentation associée
- 217 : ### Questions résolues
- 224 : ### Prochaine étape
- 227 : ## Direction (uplink / downlink) et propagation
- 229 : ### Contrat de propagation
- 251 : ### Principe (wire)
- 255 : ### Format
- 266 : ### Exemples
- 271 : ### Notes d'implémentation

Points d'insertion possibles : avant « # Phases d'implémentation » (l.89), ou après « ## Direction (uplink / downlink) et propagation » (l.227 et suivantes).
