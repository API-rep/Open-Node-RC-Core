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

- **Filtre par type de machine : inactif aujourd'hui (constaté le 2026-10-18).** Le builder lit un flag `MACHINE_TYPE_*` dans les defines de l'env, or `MACHINE_TYPE_DUMPER_TRUCK` est `#define` dans l'en-tête `volvo_A60H_bruder.h`, pas passé en `-D`. `detect_machine_type` renvoie donc `None` pour `volvo_A60H_bruder`, `test_node_mainboard` et `machines`, et le hook ne filtre rien : tous les `.cb` de `src/` sont chargés dans tous les envs (aucun mécanisme n'a encore besoin du filtre : `excavator` et `loader` n'ont aucun `.cb`). À décider : passer `-D MACHINE_TYPE_*` dans `platformio.ini` (par exemple dans `[volvo_A60H_id]`, en retirant le `#define` de l'en-tête pour éviter une redéfinition) ou déduire le type du flag `MACHINE_*`. À faire avant d'ajouter un `.cb` propre à un autre type.
- **Layer de `combus_frame_apply`.** Le layer `ChanLayer::BASE` passé à `combus_rx.cpp:223` : `_layer_ok` (`combus_access.cpp:42-45`) semble refuser les canaux SYSTEM, alors que le commentaire de `combus_rx.cpp:209-221` dit l'inverse. Non exécuté. De plus `combus_rx_update()` n'a aucun appelant dans `src/`.
- **Direction.** Aucun filtrage TX/RX aujourd'hui : décision de conception à prendre avec le rework de la communication (`doc/roadmap.md`).
- **Fusion OR côté C++ (ND6, partie runtime) et inversion BASE (ND5).** Non faites : `bothOr` est émis mais `combus_frame_apply` ne le lit pas.
- **Références à `sound_module` dans `src/`** (code mort, jamais compilé aujourd'hui). `SOUND_NODE` a été retiré des gardes de configuration le 2026-10-18 (`SOUND_ENABLED` reste, aucun env ne le définit). Restent : `light_core.cpp:24` (derrière `LIGHT_ENABLE`, inclut `sound_module/...`), `light_servo_beacon.cpp:11-13,21,64` (garde `SOUND_NODE`, inclut `sound_module/...`), `neopixel.cpp:17` et `neopixel_modes.cpp:21` (exclus du build), `esc_dev.cpp:18` (commentaire), commentaires de `include/struct/sound_struct.h` et `uart_struct.h`. Les corriger demande de décider du sort du code lumière / servo-gyrophare : à traiter avec la convergence Machine / Sound (`doc/roadmap.md`).
- **Env `remotes`.** Échec de build préexistant : le hook s'arrête sur `channel 'RUNLEVEL' requires ['HAS_RUNLEVEL_MANAGER']` (mono-carte, `RequiresResolutionError`). L'env n'a les flags d'aucune carte. Même échec pour `machines` (env de base, pas fait pour être compilé seul).
- **Build à deux cartes réel (T4).** Le mode nœud compile (`pio run -e test_node_mainboard`, envs `test_node*` conservés dans `platformio.ini` pour le débogage). Le refus de l'env racine n'a été vérifié que par lecture du code, et `test_node_extboard` n'est pas fait pour être compilé. Le test complet attend le script YAML vers `platformio.ini`.
- **Dossier `combus_generated` partagé** entre les envs : dette acceptée par le propriétaire (compiler une carte réécrit les fichiers non préfixés). Ne pas surcharger `PLATFORMIO_BUILD_DIR` avec un chemin du projet (le « è » casse l'édition de liens).
- **Test Unity `test_combus_loopback`.** Il ne compile plus (constaté le 2026-10-18, antérieur au renommage des scopes) : 14 erreurs « too many arguments » sur `combus_frame_encode` et `combus_tx_update`, dont les signatures ont changé. À remettre à jour avec le rework de la communication.
