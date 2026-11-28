# WIP - Master

Point d'entrée des chantiers en cours et à venir : **le premier fichier à lire en reprenant le travail**, avant le sous-WIP du chantier visé. Créé le 2026-11-11, à la clôture du ComBus link rework.
Public : le propriétaire et les sessions de travail (humaines ou IA).

Pour reprendre une session : lire ce fichier, puis `CLAUDE.md` et [doc/ia-guidelines.md](ia-guidelines.md) (règles de travail), puis le sous-WIP du chantier en cours. Vocabulaire : [glossaire](glossary.md).

Convention : un sous-WIP (`doc/WIP - <sujet>.md`) est en français tant que son chantier est ouvert ; à la clôture, son contenu utile passe dans la doc du projet, en anglais, et le WIP est supprimé (l'historique reste dans git).

---

## 1. Chantier en cours : clôture du ComBus link rework (branche `combus-link-rework`)

Le code est relu (blocs 1 à 11) ; reste, dans l'ordre :

1. **Doc** : fait le 2026-11-11 (doc du rework en anglais ; `WIP - ComBus link rework.md`, `~combus_v2.md` et `combus_builder_handoff.md` répartis puis supprimés). Concept d'entrée : [linking_nodes.md](linking_nodes.md).
2. **Test à deux cartes** (deux cartes reliées en UART). **Session à part** (décidé le 2026-11-11, en parallèle de la relecture de la doc, qui garde sa propre conversation) : une première batterie de tests menée en autonome par l'IA, puis une reprise par un agent tiers sous Cline (étape 3).
   - Montage : deux ESP32 en USB, liaison croisée GPIO 17 ↔ 16 dans les deux sens, masse commune par l'USB du même PC.
   - Ce qui reste à prouver à deux cartes : la mémoire `historique-tests-rework` (colonne « reste à deux cartes »), à reprendre ligne par ligne.
   - État au 2026-11-21 : banc à deux cartes (`test/two_boards/bench.py`) vert sur tous ses scénarios (A, B, C, monkey, corruption, trames forgées, charge, configurations mal assorties). Reste :
     - ~~**levier 0**~~ **mesuré le 2026-11-28** : les pics viennent des logs de `DEBUG_SYSTEM`. Monkey MA, MB, MC rejoués avec les mêmes graines (`1795859226`, `1795859614`, `1795859963`, 300 événements), avec puis sans `DEBUG_SYSTEM` (`--fa=-UDEBUG_SYSTEM --fb=-UDEBUG_SYSTEM`), tous verts :

       | | pic ComBus (a / b) | pic ComBus après un événement | boucle la plus longue (a / b) |
       |---|---|---|---|
       | MA avec / sans debug | 24,0 / 23,3 ms → 4,4 / 2,9 ms | ~11 ms → ≤ 1,0 ms | 32 / 23 ms → 18 / 18 ms |
       | MB avec / sans debug | 23,2 / 23,3 ms → 2,1 / 2,9 ms | ~11 ms → ≤ 1,2 ms | 32 / 23 ms → 20 / 17 ms |
       | MC avec / sans debug | 13,3 / 14,7 ms → 3,4 / 2,5 ms | ~11 ms → ≤ 1,0 ms | 22 / 22 ms → 22 / 16 ms |

       **Cause** : le code du lien logue chaque changement d'état (perte, retour, handshake, refus) par `sys_log_*()`, qui écrit sur l'USB série par `Serial.printf()` ([debug.h:200](../src/core/system/debug/logging/debug.h#L200)). À 115 200 bauds, un caractère prend ~87 µs : une ligne de ~100 caractères, ~9 ms, dès que le tampon d'émission du port est plein ; la boucle attend que la ligne parte. D'où ~11 ms par événement et ~23 ms quand deux ou trois lignes s'enchaînent. Sans les logs, le code du lien prend ≤ 1 ms par événement : il n'est pas en cause. L'attente a lieu **que quelqu'un lise ou non** : le pont USB-série absorbe les octets au rythme de la ligne, même port fermé.
       **Reste hors ComBus** : la boucle entière reste à 16-22 ms sans debug, alors que le ComBus n'y prend que ≤ 4 ms. Probablement la console du banc (`pollConsole()` : une ligne d'état de plusieurs centaines de caractères par `Serial.printf()`, plus `delay(2)`) : un artefact de l'outil, pas du firmware ; déduit, pas mesuré (le banc ne chronomètre pas sa console à part).
       **Pas mesuré** : `DEBUG_DASHBOARD` (le banc ne compile pas le tableau de bord) ; à mesurer sur la Volvo, qui tourne avec `DEBUG_SYSTEM` et le tableau de bord : elle a sans doute les mêmes saccades de ~11 ms à chaque changement d'état d'un lien.
       **Pas de risque de sûreté** : on reste loin des délais de perte des liens et du seuil du banc (100 ms). Correction : un chantier à part, §4 « Logs qui n'attendent jamais ».
     - **la vraie Volvo** (carte mère et extension, moteurs et servos) : seulement compilée ; le banc moteurs du propriétaire viendra plus tard.
   - Les deux bouts d'un lien sont joués par les firmwares du banc (`src/bench/`, envs `bench_*` : tracteur et remorque, machine et télécommande, nœud à deux cartes). La Volvo n'a toujours pas de lien vers la télécommande : elle pilote par la PS4.
3. **Sources de commandes** (manette PS4, télécommande du projet, plus tard SBUS), intercalées le 2026-11-21 pour que l'audit final les couvre : recadré le 2026-11-24 : chaque source (télécommande du projet comprise) est un module de la carte d'acquisition, qui écrit les entrées `BOARD` du type (`*_STICK`, `*_BTN`, sous `HAS_REMOTE`). Concept, décisions et étapes R0 à R5 : [Local remote](<WIP - Local remote.md>).
   - **R0 à R5 faits**, commités et poussés le 2026-11-27, ainsi que le retrait de l'ancien chemin d'entrée (`INPUT_*`, `input_manager`) et la reprise de la doc qui en découle (sens de `BASE`, entrées, sources ; ébauche `src/core/system/input/~README.md`).
   - **Reste** : l'essai de la Volvo avec une vraie manette (le propriétaire) : pilotage, clé, perte de la manette (`IDLE`, reprise par la clé), reconnexion, tableau de bord des entrées (« ps4 »). Démarrage en mode direct (`DIRECT_DRIVE_AT_BOOT`) le temps du rework des chaînes : L2 ne freine pas.
   - **Puis** : clôture du WIP Local remote, son contenu utile passe dans `src/core/system/input/README.md` (en anglais, selon `documentation_guidelines.md` §4) ; le WIP est supprimé.
4. **Revue indépendante** par un agent tiers : relecture et tests fonctionnels, à partir des règles et des résultats attendus, sans les conclusions de l'auteur.
5. **Relecture de la doc** par le propriétaire. Note : le commit `ee4f269` (`docs(builder)`) contient aussi le `refactor(tests)` de `test_combus_loopback.cpp` (identifiants « legacy »).
6. **Merge** dans `main`.

## 2. Chantiers suivants (ordre proposé, à valider)

| Ordre | Chantier | Sous-WIP | Pourquoi à cette place |
|---|---|---|---|
| 1 | Norme des channels communs, bibliothèques de processors et de coupleurs types | [ComBus processors](<WIP - ComBus processors.md>) §2.8 | les chantiers suivants reposent sur ces noms |
| 2 | Norme des processors (flags par thème, paquets, redéfinition) | [ComBus processors](<WIP - ComBus processors.md>) | préalable à la chaîne de simulation |
| 3 | Chaîne de simulation (entrées → simulation → sorties `BASE`) | [Simulation chain](<WIP - Simulation chain.md>) | s'appuie sur la norme des processors |
| 4 | Feux | [Light rework](<WIP - Light rework.md>) | consommateur des sorties de la simulation |
| 5 | Télécommande | [Remote firmware](<WIP - Remote firmware.md>) | grande utilisatrice des channels communs |
| 6 | Transports (ESP-NOW d'abord, I2C, CAN, contrat `SYSTEM`) | [Node transports](<WIP - Node transports.md>) | la télécommande et la remorque sans fil |
| 7 | Builder de configuration à partir de fichiers YAML | — (pas encore de WIP) | voir §3 |

## 3. Futur rework sans WIP : builder de configuration YAML

Générer la configuration (dont `platformio.ini`) à partir de fichiers YAML, comme le ComBus Builder le fait pour les `.cb`. Idées à y reprendre :

- un env racine (configuration commune) et un sous-env par carte, mono-carte compris, avec `HAS_RUNLEVEL_MANAGER` sur une seule carte par machine (README du ComBus Builder, §10) ;
- **configuration modulaire** : un fichier par module optionnel (son, lumière : ses ports, ses devices, ses `.cb`, ses mappages de commandes), inclus par un flag `HAS_*` ; les tables des sources de commandes (les mappeurs, `ps4_map.cpp` aujourd'hui) en fragments inclus par flag ; la liste des profils compatibles par type de machine (une pelleteuse ne charge pas une remorque de semi-remorque) ;
- la table des liens d'une carte (`comLinks[]`, aujourd'hui dans son `envCfg.h`) et le paramétrage des `ComDev` du cœur ;
- **`doc/board_architecture.md` à remanier** avec ce rework (décision du propriétaire, 2026-11-11) : le document n'est pas vraiment défini ; il trouvera sa place dans la section configuration YAML et dans la section flags du script. Laissé en l'état d'ici là (catégorie, arbre de configuration, fonctions d'une carte, `PERIPH_` réservé, relevés sur le code).

## 4. En attente, sans chantier

- **Logs qui n'attendent jamais, affichés seulement pour un lecteur** (idée du propriétaire, 2026-11-28 ; cause mesurée au levier 0, §1 point 2). Aujourd'hui, `sys_log_*()` écrit chaque ligne sur l'USB série et la boucle attend qu'elle parte : ~9 ms par ligne à 115 200 bauds, des saccades de 11 à 23 ms à chaque changement d'état d'un lien, et ce même sans personne au bout du câble.
  - **Contrainte matérielle** : on ne peut pas savoir qu'un terminal est ouvert. La carte (`esp32dev`) passe par un pont USB-série : le PC ne signale pas l'ouverture du port, et ses lignes DTR/RTS servent au reset. Seule une puce à USB natif (ESP32-S2, S3) le saurait. « Quelqu'un lit » doit donc être **dit par le lecteur** (une touche, comme le tableau de bord pour se suspendre et reprendre).
  - **Solution proposée** : séparer l'enregistrement de l'affichage, et ne jamais attendre le port.
    1. **Toujours enregistrer en RAM** : chaque log dans un tampon circulaire de quelques Ko (quelques µs) ; une copie existe déjà pour la « log tail » du tableau de bord ([debug.h:187](../src/core/system/debug/logging/debug.h#L187)), à généraliser.
    2. **N'afficher que pendant une session de lecture** : sans session, rien ne part sur le port ; une touche de la console l'ouvre, une autre la ferme.
    3. **À l'ouverture, vider le tampon** : le log du boot et les derniers événements, y compris ce qui s'est passé avant qu'on regarde ; ne pas afficher ne fait rien perdre.
    4. **Une écriture qui n'attend jamais**, même en session : n'écrire que ce qui tient dans le tampon d'émission du port (`Serial.availableForWrite()`), le reste attend dans le tampon RAM ; s'il déborde, une ligne « N lignes perdues ». C'est ce point qui supprime les saccades.
    5. **Plus tard** : le même tampon lu par d'autres lecteurs (la télécommande par ESP-NOW, une page web) ; le port série n'est plus qu'un lecteur parmi d'autres.
  - **Ce qui ne change pas** : `DEBUG_SYSTEM` reste le flag de compilation qui fait exister les logs dans le firmware ; la session ne décide que de leur affichage, à l'exécution.
  - **Quand** : après le merge du link rework, ou avec le chantier des transports (ESP-NOW change le chemin chaud des liens). Le module touché : `src/core/system/debug/logging/` (et le tableau de bord, qui partage le port série). Ensuite, mesurer `DEBUG_DASHBOARD` sur la Volvo et rejouer le levier 0.

- **Un même type plusieurs fois** dans un ensemble de nœuds (deux remorques, double traction) et deux nœuds de même type en égal à égal : il faudra raisonner sur les instances, plus sur les types ; piste d'un mode « twin » (deux composants qui ne forment qu'une entité). À reprendre avec l'identité des pairs (Node transports §6).
- **Un module qui fait de sa carte la carte mère** (une carte son et lumière pour une petite machine) : ressources minimales à vérifier au démarrage.
- **Renommage de `FAILSAFE_HARD` / `FAILSAFE_SOFT`** (piste `FAILSAFE_HALT` / `FAILSAFE_IDLE`), mis de côté pendant le rework.
- **`include/struct/`** : dossier déprécié, à vider vers les fichiers maîtres des thèmes (avec la norme des processors).
- **Relecture de l'application** (`src/machines/main.cpp`, `envCfg`, processors simulés, `volvo_A60H_bruder.cpp`, `core/system/sound/`, profils de son des types de machine). Y traiter les références restantes à l'ancien `sound_module` : le module lumière (`light_core.cpp`, `light_servo_beacon.cpp` sous `SOUND_NODE`, `neopixel/`, voir [Light rework](<WIP - Light rework.md>)), et des commentaires (`esc_dev.cpp`, `combus_types.h`, `include/struct/uart_struct.h`, `sound_struct.h`).

Le reste du backlog : [deferred_features.md](deferred_features.md) et [roadmap.md](roadmap.md).
