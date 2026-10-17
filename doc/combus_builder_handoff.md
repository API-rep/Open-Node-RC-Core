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

- **Filtre par type de machine.** `platformio.ini:90-95` dit que `MACHINE_TYPE_*` est défini dans un en-tête de configuration machine. Savoir si le builder voit ce flag n'est pas établi. Les types `excavator` et `loader` sont déclarés mais n'ont aucun `.cb`.
- **Layer de `combus_frame_apply`.** Le layer `ChanLayer::BASE` passé à `combus_rx.cpp:223` : `_layer_ok` (`combus_access.cpp:42-45`) semble refuser les canaux LOCAL, alors que le commentaire de `combus_rx.cpp:209-221` dit l'inverse. Non exécuté. De plus `combus_rx_update()` n'a aucun appelant dans `src/`.
- **Direction.** Aucun filtrage TX/RX aujourd'hui : décision de conception à prendre avec le rework de la communication (`doc/roadmap.md`).
- **Fusion OR côté C++ (ND6, partie runtime) et inversion BASE (ND5).** Non faites : `bothOr` est émis mais `combus_frame_apply` ne le lit pas.
- **Références à `sound_module` / `SOUND_NODE` dans `src/`.** Par exemple `light_core.cpp:24`, `light_servo_beacon.cpp:11-13`, `neopixel.cpp:17`, `esc_dev.cpp:18`, `dumper_truck_config.h:80`, alors que `src/sound_module/` n'existe plus. À traiter avec la convergence Machine / Sound (`doc/roadmap.md`).
- **Chemins morts dans `doc/~combus_v2.md`.** `include/struct/combus_struct.h` et `include/core/system/combus/combus_defs.h` (le fichier est `src/core/system/combus/combus_defs.h`).
- **Env `remotes`.** Échec de build préexistant : `channel 'FAILSAFE' requires ['HAS_FAILSAFE']` (mono-carte, `RequiresResolutionError`).
- **Build à deux cartes réel (T4).** Le mode nœud compile (`pio run -e test_node_mainboard`, envs `test_node*` conservés dans `platformio.ini` pour le débogage). Le refus de l'env racine n'a été vérifié que par lecture du code, et `test_node_extboard` n'est pas fait pour être compilé. Le test complet attend le script YAML vers `platformio.ini`.
- **Dossier `combus_generated` partagé** entre les envs : dette acceptée par le propriétaire (compiler une carte réécrit les fichiers non préfixés). Ne pas surcharger `PLATFORMIO_BUILD_DIR` avec un chemin du projet (le « è » casse l'édition de liens).
- **Liens entre les .md.** `doc/README.md` (section « By Subject ») relie maintenant les README de sujet. Restent non reliés : `.pytest_cache/README.md` (généré, à ignorer). Vérifier à l'occasion qu'un nouveau .md est lié depuis `doc/README.md`.
- **Test Unity `test_combus_loopback`.** Il ne compile plus (constaté le 2026-10-18, antérieur au renommage des scopes) : 14 erreurs « too many arguments » sur `combus_frame_encode` et `combus_tx_update`, dont les signatures ont changé. À remettre à jour avec le rework de la communication.
- **`src/core/config/machines/README.md`** : décrit l'architecture d'avant le builder (dispatchers `combus_ids.h`, namespaces, étapes « add a machine »). Une note d'état en tête signale les parties obsolètes ; une réécriture reste à faire.
