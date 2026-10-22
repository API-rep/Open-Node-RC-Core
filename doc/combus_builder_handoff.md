# Note de passation ComBus Builder : points ouverts

Ce fichier ne garde que ce qui reste à faire ou à décider. L'architecture, les règles et l'historique sont dans la documentation (voir « Où lire »). Il peut être supprimé quand la liste est vide.

## Où lire

- Architecture, diffusion, IDs, MD5, hook, tests, limites : `scripts/combus_builder/README.md`.
- Trame, handshake, lien UART : `src/core/system/combus/protocol/frame/combus_frame_protocol.md` et `src/core/system/combus/protocol/uart_workflow.md`.
- Écrire des fichiers `.cb` : `doc/configuration/cb_syntax.md`.
- Vue d'ensemble ComBus v2 : `doc/~combus_v2.md` (section « ComBus Builder »).
- Chantiers futurs : `doc/roadmap.md`. Règles de travail : `CLAUDE.md`.
- Historique : `git log`.

Consigne : l'env racine (« mère » dans le code) n'est jamais une carte.

## À vérifier ou à décider

Les points sur la communication (identité d'écriture de `combus_frame_apply`, RX non branché, direction, fusion OR, inversion pour la télécommande, test Unity `test_combus_loopback`) sont suivis dans l'étude `doc/WIP - ComBus link rework.md` (intentions au §2, roadmap au §3). Ils ne sont plus répétés ici.

- **Filtre par type de machine : actif depuis le 2026-10-20 (étape 0.3).** `-D MACHINE_TYPE_DUMPER_TRUCK` est passé dans `platformio.ini` (`[volvo_A60H_id]`, repris par `volvo_A60H_bruder` et les envs `test_node*`, et ajouté à `test_combus_loopback`), le `#define` est retiré de `volvo_A60H_bruder.h`. Le hook journalise `ND8 machine-type filter active: machine_type='dumper_truck'`. Fichiers générés identiques à l'état précédent (seul le commentaire `defines=27` devient `28`). `machines` et `remotes` n'ont pas de type : ils ne filtrent rien (inchangé). Tout nouvel env d'une machine doit porter son `-D MACHINE_TYPE_*`.
- **Références à `sound_module` dans `src/`** (code mort, jamais compilé aujourd'hui). `SOUND_NODE` a été retiré des gardes de configuration le 2026-10-18 (`SOUND_ENABLED` reste, aucun env ne le définit). Restent : `light_core.cpp:24` (derrière `LIGHT_ENABLE`, inclut `sound_module/...`), `light_servo_beacon.cpp:11-13,21,64` (garde `SOUND_NODE`, inclut `sound_module/...`), `neopixel.cpp:17` et `neopixel_modes.cpp:21` (exclus du build), `esc_dev.cpp:18` (commentaire), commentaires de `include/struct/sound_struct.h` et `uart_struct.h`. Les corriger demande de décider du sort du code lumière / servo-gyrophare : à traiter avec la convergence Machine / Sound (`doc/roadmap.md`).
- **Env `remotes`.** Depuis le 2026-10-22 la génération passe (les channels dont un flag manque sont écartés et listés dans le journal du build) ; le build échoue plus loin sur `inputs.h: no INPUT_xxx flag defined`. L'env n'a les flags d'aucune carte. Même chose pour `machines` (env de base, pas fait pour être compilé seul).
- **Build à deux cartes réel (T4).** Le mode nœud compile (`pio run -e test_node_mainboard`, envs `test_node*` conservés dans `platformio.ini` pour le débogage). Le refus de l'env racine n'a été vérifié que par lecture du code, et `test_node_extboard` n'est pas fait pour être compilé. Le test complet attend le script YAML vers `platformio.ini`.
- **Dossier `combus_generated` partagé** entre les envs : dette acceptée par le propriétaire (compiler une carte réécrit les fichiers non préfixés). Ne pas surcharger `PLATFORMIO_BUILD_DIR` avec un chemin du projet (le « è » casse l'édition de liens).
