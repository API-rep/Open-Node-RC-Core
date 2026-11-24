# WIP - Local remote

Statut : **en conception** (document `WIP`), créé le 2026-11-11 à la clôture du ComBus link rework (idée retenue le 2026-10-25), intercalé le 2026-11-21 avant l'audit final du link rework. **Recadré le 2026-11-24 (modèle B)** : toutes les sources de commandes, la télécommande du projet comprise, deviennent des modules d'entrée de la carte d'acquisition. Le concept du 2026-11-21 (un `ComPort` virtuel, en lien de confiance) est abandonné ; L1 est fait, L2 fait puis à retirer (R0).
Public : le propriétaire et les sessions de travail sur les sources de commandes.
Plan : §1 constat, §2 le modèle retenu, §3 pourquoi et ce qui a été écarté, §4 les couches d'un module, §5 mappeurs, §6 recadreur, §7 décisions de détail, §8 ce qui change dans le builder et le cœur, §9 étapes et preuves, §10 points à vérifier.

Documents liés : [WIP - Remote firmware](<WIP - Remote firmware.md>) (la télécommande, un nœud à part entière) ; [WIP - Simulation chain](<WIP - Simulation chain.md>) (les chaînes de la machine) ; [WIP - ComBus processors](<WIP - ComBus processors.md>) §2.9 (étages et processors sans état) ; [glossaire](glossary.md).

---

## 1. Constat

Une machine reçoit ses commandes par deux chemins différents :

- d'une **télécommande** (un nœud) : un lien ComBus, avec son contrat (MD5), sa table (`kRemoteLink`), son filtrage par sens et sa perte de lien (`NODE_LINK_FAIL`) ; le lien écrit directement les commandes `BASE` du type (`THROTTLE_BUS`, `KEY_BTN`...) ;
- d'une **source locale** branchée sur la machine (aujourd'hui la manette PS4 en Bluetooth, demain un récepteur RC PPM / SBUS) : un backend d'entrée (`input_refresh()`), mappé vers les `*_STICK` et les `*_BUS` par `inputs_map` (`input_update()`), avec un contributeur de santé à part (`LOCAL_REMOTE_FAIL`).

Vu de la machine, une manette PS4 **est** une télécommande : deux chemins pour la même chose. Et les deux chemins n'écrivent pas les mêmes channels : la télécommande les `*_BUS`, la PS4 les `*_STICK` puis les recopies INPUT les `*_BUS` (avec une vraie télécommande, ces recopies écraseraient à chaque tour `STEERING_BUS` et `DUMP_BUS` reçus du lien).

## 2. Le modèle retenu (décidé le 2026-11-24)

> Accepter de voir la télécommande du projet passer au même statut que les autres sources (le propriétaire).

- **Les entrées du type** (`*_STICK`, `*_BTN`) sont des channels **`BOARD`**, déclarés par le type de machine sous `requires: [HAS_REMOTE]`. Ils n'existent que sur la carte d'acquisition : hors du MD5 `BASE`, hors des trames entre cartes, hors de la RAM des autres cartes. Les trois boutons passés `BASE downlink_pt` le 2026-10-25 (`KEY_BTN`, `DIRECT_DRIVE_BTN`, `CRUISE_TOGGLE_BTN`) les rejoignent.
- **Une source = un module** de la carte d'acquisition : `HAS_REMOTE_PS4`, `HAS_REMOTE_NODE`, plus tard SBUS, PPM. Chaque module dit s'il est vivant, donne ses valeurs, et son **mappeur** écrit les entrées. C'est du code de la carte : il écrit avec l'identité `BOARD`, sans exception dans la règle des couches.
- **La télécommande du projet** (`HAS_REMOTE_NODE`) est un module comme les autres. Son lien garde tout ce qu'il a (handshake, table, MD5, âge, perte), mais **n'écrit plus le bus** (`target = nullptr`) : la trame reste dans son snapshot (`combus_rx_snapshot()`), que le mappeur du module lit. Ce mappeur est la table que le builder génère déjà pour `kRemoteLink`, tirée des entrées `HAS_REMOTE` au lieu des commandes `BASE downlink`.
- **La PS4** (`HAS_REMOTE_PS4`) : ses channels de source (`PS4_*`, `BOARD`, `ps4.cb`), son recadreur (étage `source`, sans état, §6), son mappeur de channel à channel vers les entrées.
- **Le traitement des entrées** (chaînes INPUT ; par défaut, la recopie `*_STICK` → `*_BUS`) tourne **sur la carte d'acquisition**. L'idée d'origine (acquisition sur une carte, traitement sur une autre) est abandonnée, sans regret (le propriétaire, 2026-11-24).
- **Les sorties** (`*_BUS`, états) restent ce qu'elles sont : calculées par les chaînes de la carte d'acquisition, qui est la carte reliée ; en `BASE downlink_pt`, elle les redistribue aux autres cartes, dans le bon sens. Leur éventuel reclassement relève du [WIP Simulation chain](<WIP - Simulation chain.md>).
- **La santé** : `LOCAL_REMOTE_FAIL` retiré ; la perte de la source active lève `NODE_LINK_FAIL`, les entrées à leurs valeurs sûres (stick au neutre, bouton relâché : aucune action, aucun état ne change).
- **Deux sources** : plus d'une source vivante (télécommande et manette, ou deux manettes) → la machine bloque, lève `NODE_LINK_FAIL` et logue la cause. Règle générique, tenue par le socle des modules.
- **Les capteurs ordinaires** ne sont pas concernés : channels `SYSTEM` ou `BOARD` locaux, sans contrat ni santé de source.

## 3. Pourquoi, et ce qui a été écarté

Le modèle B règle les trois défauts relevés le 2026-11-23 : l'aller-retour bus → trame → bus de la PS4, le thème chargé de sens (alors qu'il déplace les IDs), le contrat du type devenu flou. `BASE` redevient le profil du type, ses sorties ; les entrées sont locales à la carte d'acquisition ; le lien télécommande a son propre contrat (`kRemoteLink` a déjà son MD5). Et les flags tombent juste : `requires` crée les entrées, ce que le `BASE` interdit.

| Piste | Date | Écartée parce que |
|---|---|---|
| Un `ComPort` virtuel sur `kRemoteLink`, en lien de confiance (la PS4 produit de vraies trames) | 2026-11-21 | une fois les channels de la source sur le bus, la trame n'est plus qu'un détour encodé puis décodé sur la même carte, pour hériter de la supervision du lien |
| Les entrées en `BASE downlink_pt` (A) | 2026-11-23 | pas de `requires` possible sur un `BASE` : les entrées sur tous les nœuds du type, dans le MD5 `BASE`, dans les trames entre cartes et dans la RAM de chaque carte |
| Des commandes `BASE` (`THROTTLE`...) et des sorties `*_OUT` | 2026-11-23 | la même structure que `*_STICK` / `*_BUS`, renommée ; et un thème `remote` chargé de dire « commande », alors que le thème entre dans la numérotation des IDs |
| Les entrées en `SYSTEM` | 2026-11-23 | un lien entre nœuds écrit avec l'identité `BASE` (`combus_access.cpp:47-48`) ; et un `SYSTEM` est diffusé à toutes les cartes |
| Les entrées en `BOARD`, écrites par un lien dont la table donne le droit d'écrire | 2026-11-23 | une exception dans la règle des couches ; le modèle B obtient la même chose par le mappeur du module |
| Passer par un coupleur | 2026-11-23 | ses channels de lien sont `BASE` ; l'appairage et le déclencheur ne servent à rien pour une manette |

## 4. Les couches d'un module

| Couche | Rôle | Propre à |
|---|---|---|
| **Backend** | lire le matériel (ou le snapshot du lien, pour `HAS_REMOTE_NODE`), dire si la source est vivante, piloter ses actionneurs | chaque modèle (PS4, SBUS, PPM, télécommande du projet) |
| **Recadreur** | cadrer les channels de la source aux entrées (§6) ; inutile pour la télécommande du projet, qui envoie déjà cadré | chaque type de machine × modèle, puis l'instance |
| **Mappeur** | channels de la source → entrées ; plus tard, sorties de la machine → actionneurs du modèle (le retour) | chaque type de machine × modèle, puis l'instance et l'utilisateur ; généré pour `HAS_REMOTE_NODE` |
| **Socle commun** | appeler les modules, une seule source vivante, valeurs sûres et `NODE_LINK_FAIL` à la perte | rien : un seul code |

Le backend PS4 existe déjà (`PS4_dualshock.cpp`, son descripteur `inputDev` : min, max et type de chaque axe). Il reste fin : le Bluetooth doit disparaître (une radio par config).

## 5. Mappeurs : une configuration personnalisable

Le mappeur est une **configuration**, en trois niveaux. Décidé le 2026-11-21 : **pas de surcouche ni de fusion**. Un niveau qui personnalise réécrit le fichier complet, qui remplace celui du niveau précédent.

### 5.1 Les niveaux

| Niveau | Où | Ce qu'il fait |
|---|---|---|
| **Type de machine** (défaut) | `core/config/machines/<type>/inputs/<modèle>_map.*` (comme aujourd'hui) | la table complète ; écrite avec les entrées du type, elle vaut pour toutes ses instances |
| **Instance de machine** | `machines/config/machines/<instance>/inputs/` | facultatif : la table complète, qui remplace celle du type |
| **Utilisateur** | plus tard : fichier de config (builder YAML, Master §3), puis en direct (§5.3) | même format, la table complète |

Format d'une ligne : `{ channel de la source, entrée, inversion }`, deux IDs générés, vérifiés par le compilateur (`{ AnalogComBusID::PS4_LY, AnalogComBusID::THROTTLE_STICK, false }`). Une seule table est retenue, celle du niveau le plus personnalisé qui en déclare une.

Le mappeur reste **un simple aiguillage** : la mise à l'échelle est faite par le backend, le cadrage (zone morte, centre, expo) par le recadreur (§6), le comportement de la machine (rampes, inertie) par ses chaînes ([Simulation chain](<WIP - Simulation chain.md>)).

### 5.2 Contrôles, au boot

Le module copie la table retenue dans une **table en RAM**. Il refuse la source, avec la cause au log, si :

- une entrée visée n'existe pas sur la carte ;
- deux entrées sortent du même channel de la source, ou une entrée apparaît deux fois.

Contrôle au boot, pas de `static_assert` (même choix que la charge de ligne). Une entrée sans aiguillage garde sa valeur sûre.

### 5.3 Modification à chaud : hors de ce chantier, la partie statique conçue pour

- le module ne lit **que la table en RAM**, jamais les tables en flash directement ;
- une seule porte d'entrée, celle du boot : `local_map_apply(table)` passe les contrôles du §5.2, puis remplace la table d'un coup entre deux lectures ; refusée, l'ancienne reste ;
- plus tard : quand un changement est permis (source non vivante, ou machine à l'arrêt), qui appelle la porte (tableau de bord, télécommande, page web), et la persistance (NVS ; aucun stockage persistant n'existe aujourd'hui dans le firmware).

## 6. Le recadreur de la source (décidé le 2026-11-23)

Une télécommande du projet cadre ses commandes avant de les envoyer. Une télécommande du commerce (PS4, récepteur SBUS ou PPM) a un protocole figé : il faut cadrer chez nous.

- **Une chaîne de processors propre à la source**, à l'**étage `source`** ([ComBus processors](<WIP - ComBus processors.md>) §2.9) : le module l'appelle juste après la lecture du backend, sur les channels de la source, qu'elle écrase à leur place.
- **« Seulement si la valeur a été mise à jour » est garanti par construction** : elle ne tourne que quand le module vient de lire la source, une fois par échantillon ; source non connectée, ni lecture ni chaîne. L'écrasement est sans danger : `refresh()` vient de réécrire son entrée.
- **Avant toutes les chaînes de la machine**, par sa place : elle n'a pas besoin de couche de priorité.
- **Sans état** : seuls les processors dont la sortie ne dépend que de l'entrée et de leur configuration (échelle, zone morte, centre, inversion, expo) ; rampes et inertie appartiennent aux chaînes SIM. Règle et inventaire : ComBus processors §2.9.
- **Le coût suit le rythme du module** : une passe par lecture, `rythme × coût d'une passe` ; une manette qui envoie plus vite n'appelle pas la chaîne plus souvent. Le plafond du rythme (§7) borne ce coût. Par rapport à aujourd'hui (`input_refresh()` et `input_update()` à chaque tour de boucle), c'est un gain.
- **Sa configuration** suit les niveaux du mappeur (§5.1).
- Le dashboard montre la valeur recadrée ; voir aussi la brute demanderait des channels à part (pas d'emblée).

## 7. Décisions de détail (2026-11-22 et 2026-11-23), reprises dans le modèle B

| Question | Décision |
|---|---|
| Les channels de la source | un `.cb` par modèle, à côté de son backend (`ps4.cb` : `PS4_LX`, `PS4_SQUARE`...) : `scope: BOARD`, `direction: none`, `requires: [HAS_REMOTE_PS4]`, un thème à ajouter à `VALID_THEMES` (§10) |
| Le backend | `begin()` et `refresh(ComBus&)` : il écrit ses channels déjà mis à l'échelle (min, max, zone morte de son `InputDev`) et dit si la source est connectée ; non connectée, il remet ses channels à leur valeur sûre |
| **Contrat de `refresh()`** | il **réécrit tous ses channels** depuis l'état du matériel, à chaque appel : le recadreur part toujours de la valeur brute |
| Le rythme et son plafond | une lecture par période du module ; un `static_assert` à sa déclaration (`<= kRemoteSourceMaxHz`, 100 Hz proposé), comme `board_links_init.cpp:47` |
| La mémoire | fournie par l'appelant, rien de statique dans le cœur (la règle du projet) |
| Le retour (vibration, barre lumineuse) | des channels de sortie du modèle dans son `.cb`, avec leur valeur sûre ; un mappeur inverse ; plus tard, avec le retour de santé (Remote firmware §4) |

## 8. Ce qui change dans le builder et le cœur

- **Builder** :
  - les entrées du type (`inputs/input.cb`, plus les trois boutons `BASE` de `combus/`) passent en `BOARD`, `requires: [HAS_REMOTE]` : le MD5 `BASE` change (contrat cassé, attendu) ;
  - le lien télécommande porte ces entrées : `remote_commands()` et `compute_remote_link()` (`links.py`) les prennent au lieu des `BASE downlink` ; la vue de la télécommande (`remote_channels()`) devient leur miroir ;
  - la carte reliée : `INTER_NODE_FLAGS` (`generator.py:711`) passe de `INPUT_PS4_DS4_BT` à `HAS_REMOTE`.
- **Cœur** : le socle des sources (un module par source, une seule vivante, valeurs sûres et `NODE_LINK_FAIL` à la perte) ; le lien télécommande sans bus cible, lu par son module.
- **Machine** : `input_refresh()` et `input_update()` sortent de `sys_manager`, remplacés par l'appel du socle (avant les chaînes d'entrée) ; `LOCAL_REMOTE_FAIL` retiré (`.cb`, chaîne failsafe) ; les `*_STICK` ne sont plus écrits par `input_update()`.

## 9. Étapes et preuves (proposées)

| Étape | Livrable | Preuve |
|---|---|---|
| L1 | ~~Encodeur à positions brutes (`frame/`)~~ fait le 2026-11-21 : `combus_frame_encode_values()`. Inutilisé dans le modèle B, sans danger : il reste | Unity : loopback 93/93, peer_link 82/82 ; deux mutants détectés |
| L2 | ~~Lien de confiance~~ fait le 2026-11-21 (`9ceeb9a`), **à retirer** en R0 : sans port virtuel, il n'a plus de raison d'être | Unity : loopback 95/95 ; 4 mutants détectés |
| R0 | Retrait de L2 (`git revert`), et du correctif « mode next » jamais commité | builds ; loopback 93/93, peer_link 82/82, remote_link 3/3, runlevel 7/7 |
| R1 | ~~Builder~~ fait le 2026-11-24 (non commité) : les 11 entrées de la benne et `TEST_COUPLER_BTN` en `BOARD` sous `HAS_REMOTE` ; lien télécommande tiré des entrées (`is_remote_input()`), généré sur la télécommande et la carte `HAS_REMOTE` seules ; vue de la télécommande : les entrées en `BASE uplink` ; `INTER_NODE_FLAGS` = `HAS_REMOTE` ; `-D HAS_REMOTE` dans volvo, test_node_mainboard, bench_machine, bench_node_mainboard, test_peer_link ; tests de vues de loopback recibles (`RUNLEVEL`, `LIGHTS`, `HAZARDS`, `BRAKING`) | pytest 1013/1013, 5 mutants détectés ; builds 10/10 ; Unity loopback 93/93 (le test du bourrage passé à 1 digital, mutant détecté), peer_link 82/82, remote_link 3/3, runlevel 7/7. MD5 : `BASE` benne `1a766e28…` (A6+D4), lien `remote_dumper_truck` A3+D8 (socket `7f3e4980…`, plug `7a8bdd60…`) |
| R2 | Cœur : le socle des sources, une source factice scriptée | Unity : valeurs → entrées ; perte → valeurs sûres et `NODE_LINK_FAIL` ; deux sources vivantes → blocage ; retour → vivant |
| R3 | Module PS4 : `ps4.cb`, backend, recadreur (étage `source`, vide par défaut), mappeur (type, instance, table en RAM, contrôles, `local_map_apply()`) | Unity avec un backend factice : la table de l'instance remplace celle du type ; refus ; une table refusée laisse l'ancienne ; le recadreur ne tourne qu'à chaque lecture réussie ; mesure de son coût |
| R4 | Module télécommande : le lien sans bus cible, le mappeur sur son snapshot | Unity (remote_link, peer_link) : la trame reçue arrive sur les entrées ; perte du lien → valeurs sûres |
| R5 | Bascule de la Volvo : PS4 par son module, `input_refresh()` / `input_update()` hors de `sys_manager`, `LOCAL_REMOTE_FAIL` retiré, recopies INPUT conservées (chaîne par défaut) | builds, pytest ; sur la carte, avec une vraie manette (le propriétaire) : pilotage, perte et reconnexion |
| — | Puis l'audit final du link rework, sources comprises | |

## 10. Points à vérifier, questions ouvertes

- **`NODE_LINK_FAIL` n'a qu'un écrivain** aujourd'hui : le superviseur des liens, depuis `combus_links_lost(INTER_NODE)` (`combus_link_supervisor.cpp:178`). Le socle des sources doit s'y raccorder sans second écrivain (un contributeur est « consume + guard ») : le superviseur lit l'état du socle, ou le socle devient l'écrivain et le lien télécommande une source parmi d'autres. À trancher en R2.
- **La vue de la télécommande** (`remote_channels()`) : aujourd'hui le miroir du `BASE` du type ; demain le miroir de ses entrées. Ce que la télécommande garde en retour (lecture seule) reste à voir avec Remote firmware.
- **Le bouton d'attelage** : ~~à vérifier avec la chaîne du coupleur~~ fait en R1 : `TEST_COUPLER_BTN` (tracteur de test) est une entrée `BOARD` sous `HAS_REMOTE`, lue par la chaîne du coupleur ; `test_peer_link` reçoit `-D HAS_REMOTE`.
- **Le trou de R1 à R4** (accepté le 2026-11-24) : tant que le lien télécommande écrit le bus, il écrit avec l'identité `BASE`, et une entrée `BOARD` refuse cet écrivain ; la télécommande n'atteint plus les entrées de la machine. Banc deux cartes : B2, B3 et le monkey B (`bench.py`, qui écrivent encore `THROTTLE_BUS` / `KEY_BTN` côté télécommande) sont attendus en échec jusqu'à R4, à réécrire sur les entrées.
- **Les commandes relayées par un coupleur** (`TEST_BRAKE_BUS`, `TEST_LIGHTS` du tracteur de test) : restent sur le lien télécommande, en `BASE`, écrites par le lien. En R4 (lien sans bus cible), le mappeur du module devra aussi les écrire, ou elles devront devenir des entrées : à trancher en R4.
- **Le tracteur de test** : `TEST_THROTTLE_BUS`, `TEST_BEACON`, `TEST_GEAR_BUS` restent `BASE downlink`, sur aucun lien depuis R1 ; gardés pour `test_peer_link` (channels hors du lien, avant ceux du lien : la table n'est pas l'identité ; `zeroAt`).
- **Le dashboard d'une autre carte** ne voit plus les entrées : acceptable a priori.
- **Le thème des channels de la source** : `VALID_THEMES` est une liste fermée (`canon/channels.py:98`) : un thème par modèle (`ps4`...) ou un seul (`source`), à y ajouter. Avis : `source`.
- **L'appel du socle dans la boucle** : sans le crochet du RX, il faut un appel explicite, avant les chaînes d'entrée (à la place de `input_refresh()` / `input_update()` dans `sys_manager`).
- Plus tard, avec la modification à chaud : quand un changement est permis, qui l'appelle, la persistance.
