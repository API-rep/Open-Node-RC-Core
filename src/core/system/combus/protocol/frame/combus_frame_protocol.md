# ComBus — trame binaire et ordre des canaux

Complète [`../~README.md`](../~README.md) (couche transport, `NodeCom`). Ce document décrit ce que
le code de `protocol/` et `protocol/frame/` fait aujourd'hui. Rien n'est
documenté ici qui ne soit lu dans le code ; le reste est dans des blocs
`À VÉRIFIER`.

Générateur des identifiants : `scripts/combus_builder/README.md`.
Workflow UART (chaîne d'initialisation) : [`../uart_workflow.md`](../uart_workflow.md).

## 1. Format de la trame

Sources : `frame/combus_frame.h:1-31`, `frame/combus_frame.cpp:111-195`,
`frame/combus_frame_defs.h:46-83`.

| Offset | Taille | Champ |
|---|---|---|
| 0 | 1 | `SOF` = `0xAA` (`CombusFrameSof`) |
| 1 | 1 | `nAnalog` : nombre de canaux analogiques |
| 2 | 1 | `nDigital` : nombre de canaux numériques |
| 3 | 1 | `seq` : compteur de trame |
| 4 | 1 | `flags` : bits `COMBUS_FLAG_*` |
| 5 | `ceil(nDigital/8)` | canaux numériques, 1 bit par canal, LSB d'abord |
| 5 + d | `2 × nAnalog` | canaux analogiques, `uint16` petit-boutiste |
| fin | 1 | CRC-8/MAXIM sur tous les octets précédents |

- L'en-tête après `SOF` fait 4 octets ; avec `SOF` : 5 (`static_assert`,
  `combus_frame.h:52`).
- **Les canaux numériques sont avant les analogiques** (`combus_frame.cpp:168-188`).
- Longueur maximale 255 octets ; l'encodage renvoie 0 au-delà (`combus_frame.cpp:143`).
- `seq` : 1 à 255 pour une trame de contrôle. `seq == 0` est réservé au
  handshake ; `combus_frame_encode` le refuse (`combus_frame.cpp:137`).
- `flags` : un seul bit défini, `COMBUS_FLAG_FAILSAFE` (bit 0,
  `combus_frame.h:69`). L'encodeur écrit `flags = 0` aujourd'hui
  (`combus_frame.cpp:148-154`) : l'état failsafe voyage par un canal.

## 2. Le runLevel n'est pas dans l'en-tête

Le run level ne fait plus partie de l'en-tête (changement « RL2 »,
`frame/combus_frame_defs.h:71-75`). Il voyage comme un canal ordinaire :
`RUNLEVEL`, analogique, scope SYSTEM, `direction: uplink`, activé par
`HAS_RUNLEVEL_MANAGER` (`src/core/system/combus/runlevel.cb`).

> À VÉRIFIER : plusieurs commentaires du code parlent encore de `runLevel`
> dans l'en-tête ou dans `combus_frame_apply` :
> `frame/combus_frame.h:150`, `frame/combus_frame.cpp:316` et `:164-165`
> (« header is now 5 bytes (was 6) » : le décompte inclut `SOF`).
> Le code (§1) fait foi.

## 3. Quels canaux sont transmis

Chaque lien a un `ChanLayer` (BASE, SYSTEM ou BOARD). À l'initialisation du
TX, le nombre de canaux à émettre est la valeur `CH_COUNT` de la vue
correspondante (`combus_tx.cpp:122-133`) :

| Layer du lien | Analogiques | Numériques |
|---|---|---|
| BASE | `AnalogComBusBaseID::CH_COUNT` | `DigitalComBusBaseID::CH_COUNT` |
| SYSTEM | `AnalogComBusSystemID::CH_COUNT` | `DigitalComBusSystemID::CH_COUNT` |
| BOARD | `AnalogComBusID::CH_COUNT` (vue complète, repli) | `DigitalComBusID::CH_COUNT` |
| UNDEFINED | arrêt fatal | arrêt fatal |

L'encodeur envoie les canaux d'indice `0 .. n-1`, avec
`n = min(wireEnd, cfg.nAnalog / cfg.nDigital)` (`combus_frame.cpp:124-125`).
Cela suppose que les vues BASE et SYSTEM soient des **préfixes** de la vue
complète, avec les mêmes indices : c'est une propriété de l'attribution des IDs
par le builder (`scripts/combus_builder/README.md`, §6).

### Deux ordres de scopes à ne pas confondre

- Attribution des IDs (`generator.py:296`, `_VIEW_SCOPE_ORDER`) : BASE, puis
  SYSTEM, puis BOARD. C'est elle qui rend les vues préfixes l'une de l'autre,
  donc qui fixe l'ordre des canaux sur le fil.
- Tri canonique (`canon/channels.py:97`, `SCOPE_ORDER`) : SYSTEM, BASE, BOARD.
  Il n'intervient pas dans les IDs.

Les IDs sont comptés séparément pour les analogiques et les numériques ; chaque
compteur part de 0 (`generator.py:362-386`).

## 4. `WIRE_END` et `CH_COUNT`

- `CH_COUNT` : dernier élément de chaque énumération d'IDs générée
  (`generator.py:575`, `584`). Il dimensionne les tableaux de canaux
  (`generator.py:692-708`) et sert de `wireEnd` au TX (§3).
- `WIRE_END` : notion du générateur Python (`View.wire_end`, nombre de canaux
  BASE de la vue, `generator.py:186-199`). Les constantes C++ correspondantes
  ne sont plus émises (`generator.py:588-595`). Aucun code de `src/` ne les lit,
  hors un commentaire (`src/machines/system/debug/dashboard_machine.cpp:137`).
- Dans la vue BASE, `WIRE_END == CH_COUNT` (`generator.py:46`). Dans la vue
  complète, `CH_COUNT` est supérieur ou égal à `WIRE_END`. Ils diffèrent dès
  qu'il existe des canaux SYSTEM ou BOARD.

## 5. Décodage et application

- `combus_frame_decode` (`combus_frame.cpp:236-302`) : vérifie `SOF`, la
  longueur, le CRC. Refuse une trame dont `nAnalog` ou `nDigital` dépasse la
  capacité (`cfg`, chantier 2.3) ; ne déballe que les `nDigital` bits annoncés.
  Le décodage ne connaît pas les layers.
- `combus_frame_apply` (`combus_frame.cpp:338-395`) : écrit les canaux par
  indice via `combus_set_analog` / `combus_set_digital`, et met à jour
  `combus->lastFrameMs`. Le contrôle de layer est fait par ces fonctions
  (`_layer_ok`, `combus_access.cpp:28-54`). **Filtre par sens (chantier 2.4)** :
  un channel n'est écrit que si son sens pour le lien comprend `Downlink` :
  `flow.betweenNodes` sur un lien inter-nœud (identité `BASE`),
  `flow.betweenBoards` sur un lien intra-nœud (`ChanFlow`, `combus_defs.h`). Un channel que la carte produit (`uplink`) n'est donc jamais
  écrasé par son pair ; les `both_or` restent traités par la fusion de 1.1b.
- En production, `combus_frame_apply` n'a qu'un appelant : `tryDecode` dans
  `combus_rx.cpp`, appelé automatiquement quand le lien a une cible
  (`st.target != nullptr`), avec l'identité d'écriture du lien (`st.writeLayer`,
  voir ci-dessous). `combus_rx_update` est appelé à chaque tour par
  `com_rx_update` (voir §10) ; la `target` est le ComBus du nœud, renseignée par
  `combus_links_init` pour les liens qui reçoivent seulement. Le lien d'extension
  de la Volvo est en TX seul : aucune trame n'y est appliquée aujourd'hui.
- Le même indice sert d'ID des deux côtés : émetteur et récepteur doivent donc
  avoir généré les mêmes IDs. Le handshake compare l'empreinte MD5 de la vue du
  lien (`combus_protocol.cpp:55-88`, `kCombusBaseComBusMd5`,
  `kCombusSystemComBusMd5` ou `kCombusComBusMd5` selon le layer).
- **Lien entre nœuds avec un type pair (link rework 3.1b)** : quand le `ComDev`
  porte un `peer` (`ComPeerLink`, `combus_frame_defs.h`, généré dans
  `combus_peer_links.h` par `-D COMBUS_PEER_<TYPE>`), la trame est la trame
  commune du lien, et non le préfixe de la vue `BASE`. Ses comptes viennent du
  lien. À l'émission, chaque position sur le fil lit le channel local que donne
  la table (`analogMap` / `digitalMap` de `combus_frame_encode`). À la réception,
  seules les positions que ce nœud reçoit sont écrites (`analogRx` / `digitalRx`),
  dans le channel local de la table : la table remplace alors le filtre par sens.
  Les `both_or` restent à la fusion, que `combus_rx_peer_or` fait par position
  sur le fil. Le contrat est le MD5 du lien (`md5Stable`, `md5` en mode next),
  et le mode stable s'arrête aux comptes stables du lien. Sans `peer`, rien ne
  change. Preuve : `test/test_peer_link` (validé sur la carte le 2026-10-25).

Identité d'écriture du RX : `combus_rx_init` reçoit le `layer` du lien et en
dérive `st.writeLayer`. `_layer_ok` n'autorise l'écriture d'un canal SYSTEM que
pour un appelant SYSTEM ou BOARD (`combus_access.cpp:42-45`) : un lien BASE
écrit donc les canaux BASE seulement, un lien SYSTEM les canaux BASE et SYSTEM.
Un lien BOARD reçoit l'identité SYSTEM (une trame n'écrit jamais un canal
BOARD). Avec une cible et un layer `UNDEFINED`, l'init s'arrête en FATAL. Avant
le chantier 0.1, l'identité était la constante `ChanLayer::BASE`, qui refusait
les canaux SYSTEM d'un lien carte à carte. Preuve : groupe H de
`test/test_combus_loopback` (validé sur la carte le 2026-10-20).

## 6. Direction : pas de filtrage au transport

Le champ `direction` (`None`, `Uplink`, `Downlink`, `Both`) est présent dans
chaque canal généré (`combus_defs.h:100-129`). Aucune lecture de ce champ n'a
été trouvée dans `src/` : ni `combus_tx`, ni `combus_rx`, ni
`combus_frame_*` ne filtrent les canaux selon la direction. L'encodeur émet tous
les canaux du préfixe de la vue (§3).

> À VÉRIFIER : la direction est donc aujourd'hui une donnée du contrat (hash
> MD5, génération), pas un filtre d'émission ou de réception. La doc de
> roadmap (`doc/~combus_v2.md`, « Métadonnées transport ») la décrit comme
> « diffusion ou non des valeurs » : c'est un objectif, pas ce que fait le code.

## 7. Fusion OR

`DigitalComBus.bothOr.enabled` est généré à `true` pour `both_or` (`BothOrMerge`,
`combus_defs.h`). `combus_frame_apply` n'écrit jamais un `both_or` : la valeur
reçue reste dans l'instantané du lien, et `com_rx_update()` fait la fusion OR
à chaque cycle (link rework 1.1b, voir `../both_or.md`).

## 8. Handshake (`seq == 0`)

Le handshake détecte deux participants dont les contrats ComBus ou les versions
de projet diffèrent (firmwares flashés séparément). C'est un contrôle de
cohérence, pas une mesure de sécurité (MD5).

### 8.1 Format

Source : `frame/combus_handshake_tx.cpp:32-115` (émission),
`frame/combus_handshake_rx.cpp:129-225` (réception),
`frame/combus_handshake.h:118-124`.

| Offset | Taille | Champ |
|---|---|---|
| 0 | 1 | `SOF` = `0xAA` |
| 1 | 1 | `nAnalog` = 0 |
| 2 | 1 | `nDigital` = 0 |
| 3 | 1 | `seq` = 0 (réservé au handshake) |
| 4 | 1 | `flags` : bit 0 = ACK (`kCombusHandshakeFlagAck`, sinon demande) ; bit 1 = NEXT (`kCombusHandshakeFlagNext`, sinon legacy) |
| 5 à 20 | 16 | MD5 de l'ensemble du lien (BASE, SYSTEM ou complet) ; MD5 next pour un handshake NEXT |
| 21 | 1 | version de projet, majeur (`kProjectVersionMajor`) |
| 22 | 1 | version de projet, mineur (`kProjectVersionMinor`) |
| 23 | 1 | version de projet, dev (`kProjectVersionDev`, chantier 2.2b) |
| 24 | 1 | CRC-8/MAXIM sur les octets 0 à 23 |

Longueur totale : 25 octets (`CombusFrameHandshakeMinLen` = SOF + en-tête 4 +
charge utile 19 + CRC). La charge utile fixe est `kCombusHandshakePayloadLen
= 19` (`combus_handshake.h`) ; un `static_assert` de `combus_handshake.cpp`
la compare à la constante générée `kCombusHandshakeWirePayloadLen`.

### 8.2 Quel MD5 ?

À l'initialisation du lien, `combus_protocol_init` résout `expectedMd5` selon
`link->layer` (`combus_protocol.cpp:55-88`) :

| Layer du lien | MD5 |
|---|---|
| BASE | `kCombusBaseComBusMd5` |
| SYSTEM | `kCombusSystemComBusMd5` |
| BOARD | `kCombusComBusMd5` (vue complète) |
| UNDEFINED | arrêt fatal |

TX et RX lisent le même pointeur dans le contexte du lien. Si `expectedMd5`
est nul à la réception, `combus_handshake_compare` s'arrête en erreur
fatale (`combus_handshake_rx.cpp`). L'émetteur, lui, replie sur le MD5 de
la vue complète si le pointeur est nul (`combus_handshake_tx.cpp:51-53`,
`:179-181`).

### 8.3 Émission : rafale, demandes, ACK (chantier 1.2)

`combus_tx_update` appelle `combus_handshake_tx_update` **avant** le timer des
trames de contrôle (`combus_tx.cpp`) : les deux peuvent écrire dans le même
appel. Dans l'ordre :

- une demande reçue (`ackPending`, posé par le RX d'un bout bidirectionnel)
  reçoit un ACK qui porte notre MD5. On ne répond jamais à un ACK ;
- la rafale : `combus_tx_init` l'arme (`startBurst`) ; 3 demandes
  (`kCombusHandshakeBurstCount`), une toutes les 100 ms
  (`kCombusHandshakeBurstPeriodMs`), arrêtée dès que le contrat est `STABLE` ;
- ensuite, une demande toutes les 250 ms (`kCombusHandshakePeriodMs`), quel que soit
  l'état du contrat : elle établit le contrat, elle demande l'ACK périodique, et
  sur un bout qui n'émet que, elle permet à un récepteur démarré plus tard de
  s'établir en 250 ms au plus.

**Genre des demandes (chantier 2.2b)** : un bout **avec candidats** (MD5 next
différent du MD5 stable, liens `BASE` seulement) envoie d'abord des demandes
NEXT, la salve puis 3 relances (`kCombusHandshakeNextRetries`), puis des
demandes legacy ; une fois le contrat `STABLE`, les demandes ont le genre du
mode. Un bout **sans candidat** n'envoie que des demandes legacy. Un ACK a le
genre de la demande à laquelle il répond (`ackPending`, `ackPendingNext`).

Un bout bidirectionnel n'émet **aucune trame de données** tant que son contrat est
`INCONNU`. Un bout qui n'émet que les émet toujours. `combus_handshake_sendOnce`
(émission unique, pour le debug et les tests) existe aussi.

### 8.4 Contrat du lien à la réception (chantier 1.2)

- Les octets d'un handshake sont consommés après contrôle du CRC ; un CRC faux
  saute un octet et resynchronise.
- **À chaque handshake** (demande ou ACK), le MD5 et la version **majeure** sont
  comparés (`combus_handshake_compare`) ; la mineure est seulement journalisée.
- Le contrat est `INCONNU` au départ, puis `STABLE` sur un handshake conforme :
  l'**ACK** de sa propre demande pour un bout bidirectionnel (preuve que le pair
  le reçoit), la **demande** du pair pour un bout qui ne fait que recevoir.
- Retour à `INCONNU` : un handshake refusé (MD5 ou majeure différents) ; un
  silence plus long que `linkTimeoutMs` après des données ; sur un bout
  bidirectionnel, aucun ACK pendant 1 s (`kCombusHandshakeAckTimeoutMs`, 4 demandes) :
  le pair ne reçoit plus ; une réinitialisation (`combus_rx_init`). Un `FAILSAFE`
  actif ne touche plus au contrat (l'ancienne réaction P4 est retirée : avec le
  blocage, elle aurait bloqué le `FAILSAFE` des autres cartes).
- En `INCONNU`, une trame de données bien reçue est ignorée : elle n'est pas
  appliquée, et la dernière trame gardée pour ce lien est effacée
  (`combus_rx_snapshot()` renvoie `nullptr`) ; le lien n'est pas vivant
  (`combus_rx_is_alive`). Le CRC est vérifié avant : un faux octet `0xAA` ne
  fait sauter qu'un octet, sans avaler la trame suivante. Un pair
  au contrat différent compte donc comme jamais vu, et la politique de perte du
  lien (`onLoss`) s'applique.
- **Mode next / stable / legacy (chantier 2.2b)**, deux pairs par lien :
  - un handshake NEXT ne décide que du mode, jamais du contrat ;
  - ACK NEXT conforme (MD5 next et majeure) : contrat `STABLE` en mode next ;
    ACK legacy conforme : mode stable (ou legacy sans candidat) ;
  - un ACK NEXT non conforme pendant la négociation fait passer aussitôt aux
    demandes legacy ;
  - le mode suit ce que le pair annonce : en mode stable, un handshake NEXT
    conforme fait passer en next ; en mode next, un handshake NEXT non conforme
    ramène au mode stable ; le contrat ne tombe pas ;
  - une perte remet le mode à stable et réarme les demandes NEXT ;
  - sur un lien `BASE` hors du mode next, l'émetteur n'envoie que les channels
    stables (`kCombusBaseStableAnalogCount`, `kCombusBaseStableDigitalCount`)
    et le récepteur n'applique qu'eux : les candidats gardent leur valeur par
    défaut. Les candidats suivent les stables : une trame next lue en mode
    stable reste juste sur sa partie stable.
- Journal : une ligne quand le contrat devient `STABLE` (avec le mode, la
  version et `DEV` du pair), quand il redevient `INCONNU` (avec la raison), à
  chaque changement de mode, et au premier refus d'un handshake.
- Un lien sans contexte de handshake (tests du codec, appel direct de
  `combus_rx_init`) n'a pas de contrat : ses trames sont appliquées.

### 8.5 Interrupteurs de compilation

| Macro | Effet | Source |
|---|---|---|
| `COMBUS_MD5_CHECK_DISABLE` | Court-circuite la comparaison : tout handshake est accepté (bannière au démarrage). Défaut 0. | `combus_handshake.h`, `combus_handshake_rx.cpp` |
| `COMBUS_HANDSHAKE_BURST_DISABLE` | Désactive la rafale ; la demande périodique reste. Défaut 0. | `combus_handshake.h`, `combus_handshake_tx.cpp` |

> À VÉRIFIER : l'en-tête `combus_handshake.h` (lignes 3-24) décrit encore le
> handshake comme « NOT YET implemented on the wire » avec un simple stub, et
> cite un « friend cache » hors périmètre. Le code lu (§8.1 à 8.4) implémente
> l'émission, la rafale et la validation : ce commentaire est périmé.

## 9. Liens, pools et états par lien

- `ComBusLink` (`combus_protocol.h:108-119`) décrit un lien : transport
  (`com`), configuration TX et RX optionnelles (`std::optional`), `layer`,
  contexte de handshake, `target` optionnelle.
- `combus_protocol_init_all(links, count)` initialise chaque lien
  (`combus_protocol.cpp:31-37`).
- Les états TX et RX sont des tableaux fournis par l'appelant et enregistrés
  une seule fois (`combus_tx_register_pool`, `combus_rx_register_pool`) ; un
  second enregistrement est ignoré avec un message d'erreur
  (`combus_rx.cpp:45-57`). Le nombre de liens n'est pas limité par le module.
- Un contexte de handshake appartient à un seul lien ; TX et RX du même lien le
  partagent.
- Une trame reçue n'est appliquée au ComBus que si `link.target` est non nul
  (`combus_rx.cpp:222-225`). `combus_links_init` la renseigne pour les liens qui
  reçoivent (`src/machines/init/com/combus_uart_init.cpp` lui passe le ComBus du
  nœud) ; un lien TX seul n'en a pas.
- `combus_rx_update` est appelé par `com_rx_update` en tête de chaque tour de la
  machine. Depuis le chantier 2.4, un lien qui reçoit n'écrit que les channels
  que la carte reçoit (filtre par sens) : un `RUNLEVEL` produit ici n'est plus
  écrasé par celui que renvoie le pair.

> À VÉRIFIER : l'en-tête `combus_protocol.h:169-173` annonce que
> `combus_protocol_init` « return » pour un layer `UNDEFINED` ou `BOARD`. Le
> code arrête le système pour `UNDEFINED` (`combus_protocol.cpp:79-87`) et traite
> `BOARD` comme la vue complète (`:74-78`, `combus_tx.cpp:129-134`).

## 10. Ordre d'un cycle de la machine

Source : `src/machines/main.cpp` (appels lignes 51, 57-58, 233) et
`src/machines/system/sys_manager.cpp:19-43`.

1. `sys_manager_update` : réception ComBus (`com_rx_update`, qui publie aussi
   `NODE_LINK_FAIL` et `BOARD_LINK_FAIL`), acquisition des entrées, `vbat_update`, `failsafe_update`,
   puis `remote_link_fallback_update` si `HAS_REMOTE_LINK_LOST_FALLBACK` ;
2. chaînes d'entrée et de simulation (`proc_chain_update`) ;
3. machine à états des runlevels ;
4. `output_update` → `com_update` : émission ComBus (TX).

Le « reset » des canaux agrégés par OU est fait en tête de chaîne par le
processor `cb_reset_fn` (`failsafe_chain.cpp:49`, `failsafe.h:9-10`), pas par
l'accesseur ni par `sys_manager_update`. L'agrégation OU est faite par
`cb_or_fn` dans la chaîne. `combus_set_digital` écrit la valeur reçue sans
fusion (`combus_access.cpp:91-102`).

## 11. Dette connue (relue le 2026-10-14)

Reprise de la revue de code « R1 » de l'ancien document de travail, vérifiée
dans le code actuel.

| Point | État | Source |
|---|---|---|
| Valeur de retour manquante de `compareAndLog` | résolu | `combus_handshake_rx.cpp:55-121` retourne un `bool` sur tous les chemins |
| `periodMs = 1000 / txHz` : TX inerte si `txHz > 1000` | **toujours vrai**, pas de garde ni de journal | `combus_tx.cpp:107`, `:201` (`periodMs == 0` → lien ignoré) |
| `CombusFrameHeader` non `packed` | atténué par un `static_assert` sur la taille | `combus_frame_defs.h:79-83`, `combus_frame.h:52` |
| `COMBUS_MD5_CHECK_DISABLE` défini en deux endroits | résolu (définition unique dans l'ombrelle) | `combus_handshake.h:171-173` |
| Include codé en dur de `dumper_truck/combus_ids_remote_md5.h` | résolu (MD5 par layer, indépendant du type de machine) | `combus_handshake.h:51-53` |
| `isDrived` | supprimé | plus de champ dans `combus_defs.h` |
| Tampon `linear[]` surdimensionné dans le décodeur de handshake | **toujours vrai** : 1 + 4 + 255 + 1 octets de pile pour une trame de 24 | `combus_handshake_rx.cpp:155-158` |
| Offset de `seq` calculé par `offsetof` imbriqués | **toujours vrai** | `combus_rx.cpp:155-156` |
| Tampon TX `static uint8_t frame[255]` | partagé entre liens, utilisé de façon synchrone dans la boucle | `combus_tx.cpp:194` |
| Pas de contrôle de taille des tampons RX fournis | **toujours vrai** (seuls les pointeurs nuls sont refusés) | `combus_rx.cpp:264-265` |
| Écriture directe de `.value` dans `protocol/` | aucune trouvée (recherche `(digital|analog)Bus[..].value =`) | `src/core/system/combus/protocol/` |

## 12. Non implémenté dans le transport

- Filtrage TX/RX selon `direction` (voir §6), avec relance / accusé de
  réception / délai d'attente, et deux niveaux de qualité de service : suivis
  dans `doc/roadmap.md` (« Rework du module de communication ComBus »). Aucune
  trace de ces mécanismes dans le code lu.
- Fusion OU à la réception (voir §7).

Le handshake reste en mode simple : un booléen `contractValidated` par lien
(`STABLE` ou `INCONNU`, `combus_handshake.h`). Le cache à plusieurs entrées avec « slot
unique » du document de conception d'origine est abandonné.
