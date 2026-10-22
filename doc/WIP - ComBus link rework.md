# ComBus link rework

Statut : **en cours de rédaction** (document `WIP`), étude du 2026-10-18. Chantier 0 : 0.1 à 0.3 faits, 0.4 en conception (§2.1) ; le reste n'est pas implémenté.
Public : le propriétaire et les futures sessions de travail sur le rework de la communication.
Plan : §1 objectif, §2 **intentions** (le modèle retenu), §3 **roadmap** (chantiers et étapes, chacun avec le constat du code qui le motive), §4 questions ouvertes.

## 1. Objectif

Un seul mécanisme de communication pour tous les cas : plusieurs liens en parallèle (TX et RX sur plusieurs ports), plusieurs protocoles (UART aujourd'hui ; ESP-NOW, I2C, WiFi ensuite), qu'il s'agisse d'une télécommande vers une machine, d'une carte vers une carte, ou d'une machine vers une remorque. Une mise à jour d'un véhicule ne doit pas rendre le reste du parc inutilisable. Objectif de fin d'hiver 2027 du roadmap.

## 2. Intentions (le modèle retenu)

### 2.1 Décrire les liens : `NodeCom`, `ComPort`, `ComDev`, `ComDevMap`

Base conservée : le protocole est déjà multi-liens et indépendant du transport (pool d'états par lien en TX et en RX, contexte de handshake par lien, échanges uniquement par `NodeCom`). Le codec n'est pas refondu ; le rework porte sur la configuration, les contrats et le branchement.

| | Rôle | Où |
|---|---|---|
| **NodeCom** | envoyer et lire des octets (existe) | `core/system/hw/node_com.h` |
| **ComPort** | le matériel, port physique sur PCB ou logique interne : une union balisée par `ComPortType` (UART, I2C, CAN ; ESP-NOW pour un port logique interne sans broches ; chaque type a ses broches (si port physique) et son plafond : débit, fréquence d'horloge). La liste d'une carte peut être exhaustive ; seuls les ports utilisés sont initialisés (et leurs broches réservées) | structure dans `core/system/hw/node_com.h` ; tableau dans la config de la carte |
| **ComDev** | une liaison logique de ComPort : nom, **portée** (`INTER_NODE` ↔ `ChanLayer::BASE`, `INTRA_NODE` ↔ `SYSTEM`), `LinkType` (unidirectionnel ou bidirectionnel, **sans sens à ce niveau**), protocole (pointeur vers une config : débit, fréquence d'émission, délai de perte du lien), activation (`ALWAYS` ou `ON_COUPLING`) et couplage. Pas de liste de channels : l'ensemble suit la portée | structure dans `core/system/combus/protocol/combus_protocol.h` ; instances : voir « Où vivent les `ComDev` » |
| **ComDevMap** | le branchement d'un `ComPort` carte sur un `ComDev`, et le rôle du bout (`LinkEnd` : `TX`, `RX`, `TX_RX`). Le sens d'un lien unidirectionnel est donc propre à chaque carte ; un bidirectionnel impose `TX_RX` | config de la carte |

- Les structures vivent en tête des **fichiers maîtres de leur thème** (`include/struct/` est déprécié). `ChanLayer` reste le nom partout ailleurs (générateur, `.cb`, docs) ; la conversion portée vers `ChanLayer` est unique, dans le `ComDev`.
- Le cœur lit ces tables, ouvre les adaptateurs et construit les liens. Les limites matérielles de l'ESP32 (nombre d'UART, taille des pools, pairs ESP-NOW) restent des constantes de la carte, vérifiées au démarrage.
- L'ensemble réduit (§2.2) et le mode des liens à diffusion seule (§2.4) s'ajouteront au `ComDev` ou à son protocole quand leurs chantiers arriveront.
- Le rôle machine / télécommande vient de `IS_MACHINE` / `IS_REMOTE`.
- **Où se range le code de communication** (depuis le 2026-10-21) : `core/system/com/` (`com_update()` pour l'émission dans la boucle, appelée par `output_update()` qui reste l'umbrella des sorties ; `link_health/` pour la santé de lien et la chaîne `REMOTE_LINK_LOST`), `core/config/com/` (config des transports), `machines/init/com/` (init). Le protocole reste dans `core/system/combus/` et les adaptateurs de transport dans `core/system/hw/transport/`. `input` et `output` sont réservés aux dispositifs locaux, plus proches du matériel.
- **Thème `com` des channels de santé de lien** (2026-10-21) : `REMOTE_LINK_LOST`, `UART_LINK_LOST` et `PS4_DS4_BT_LINK_LOST` ont le thème `com` (ajouté à `VALID_THEMES`), pour les retrouver et les trier ensemble (tableau de bord). `PS4_DS4_BT_LINK_LOST` est déclaré à côté du backend PS4 (`core/config/inputs/`) avec `requires: INPUT_PS4_DS4_BT`. Le thème fait partie du tri canonique : le MD5 de la vue `BASE` ne change pas, celui de la vue `SYSTEM` change une fois (`f01298d5` → `3be924b0`) et `REMOTE_LINK_LOST` passe de l'indice numérique 18 à 9. Un env sans PS4 (machine autonome) n'a simplement pas ce channel.
- **`requires` du builder** (2026-10-22) : en mode mono-carte, `resolve_requires` levait `RequiresResolutionError` (A6.1), ce qui obligeait tout env à définir les flags de tous les `.cb`, alors que le mode nœud écartait le channel et que l'intention d'origine est de ne créer le channel que si le flag est activé. Il écarte maintenant le channel et le signale dans le journal (`[combus_builder] requires: channel 'X' not generated (flag not defined: ...)`). Fichiers générés identiques pour la Volvo (9) et le nœud d'essai (17) ; `remotes` et `machines` passent la génération et échouent plus loin (aucun `INPUT_xxx`). Validé : pytest 737 sur 737 (les 4 tests réécrits échouent sur l'ancien code), test Unity 38 sur 38 sur la carte.
- **Audit des `requires`** (2026-10-22) : 4 channels en ont (aucun en `BASE`), et leurs 4 flags sont tous des `-D` (aucun channel ne disparaît toujours) ; toutes les utilisations passent par le nom de l'enum (aucune par chaîne ni par indice brut), donc un channel non généré fait échouer la compilation, jamais l'exécution. `PS4_DS4_BT_LINK_LOST` est réellement optionnel (3 utilisations sur 3 sous `INPUT_PS4_DS4_BT`). `RUNLEVEL` et `FAILSAFE` sont utilisés sans garde (`main.cpp`, `init.cpp`, `failsafe_chain.cpp`, et `combus_tx.cpp:257` dans un log de debug du cœur) : décision du propriétaire, `RUNLEVEL` est obligatoire et réservé à la carte principale (les autres cartes le reçoivent par diffusion), `FAILSAFE` probablement aussi.
- **`REMOTE_LINK_LOST`** a `requires: HAS_REMOTE_LINK_LOST_FALLBACK` (2026-10-22) : son écrivain (la chaîne) et ses 4 lecteurs sont sous ce flag ; une machine sans la chaîne n'a plus ce channel. Fichiers générés de la Volvo et du nœud d'essai inchangés. `UART_LINK_LOST` reste sans `requires` : il sera renommé ou scindé avec la politique de perte de lien (un `requires` ne sait pas exprimer « UART qui reçoit ou ESP-NOW »).
- Plusieurs `NodeCom` peuvent partager un port : l'écriture est séquentielle. En ESP-NOW, un tampon de réception par pair (l'émetteur est connu) ; en UART, un port = un pair (la trame n'a pas d'adresse).

**Où vivent les `ComDev`**

- **Liens intra-nœud** : définis par le nœud, à la racine des cartes (`machines/config/machines/<machine>/board/com_dev.h`). `board/` contient `mainboard/` et `extboard/` ; chaque carte déclare ses `ComPort` et ses `ComDevMap` vers ces `ComDev`. Le `ComDev` est le « bus » commun entre cartes : deux variantes de carte mère peuvent pointer vers le même, et le choix de l'une ou de l'autre à la compilation est transparent pour le reste. Exemple en place : le lien d'extension de la Volvo (`kComDevExtUni` et `kComDevExtBi` dans `volvo_A60H_bruder/board/com_dev.h`, avec son `ComProtocolCfg`).
- **Liens inter-nœuds** : des `ComDev` **globaux, fournis par le cœur** (modèles ESP-NOW, WiFi, radio, UART…), paramétrés par des flags `-D`. Les deux nœuds partagent ainsi la même définition, et des véhicules compilés avec les mêmes flags forment un parc homogène. Les `ComPort` correspondants sont soit avec broches (module radio), soit sans (contrôleur ESP-NOW interne) ; ils sont rattachés à ces `ComDev` par des `ComDevMap`, et s'initialisent comme en intra-nœud, vers l'extérieur.

**Couplage**

Pour un lien qui ne doit exister que lorsque deux éléments sont réunis (camion et remorque de même type, accessoire), un `ComDev` s'active **au couplage** (`ON_COUPLING`). Le déclencheur est un **channel ComBus** défini par un `.cb` conditionnel du dispositif déclencheur (un lecteur RFID, par exemple), et sa valeur identifie le pair attendu. Le module de communication réagit à ce channel ; `ComCoupling` décrit le déclencheur, la valeur attendue et l'identifiant du pair. Un lien inactif est un **état d'exécution** : il est déclaré et ses ressources sont réservées au démarrage, seuls l'émission, la réception et le handshake sont suspendus. Étape 0.4 : seul `ALWAYS` est accepté (contrôle au démarrage) ; la logique de couplage est au chantier 3.

**Démarrage**

```
com_init()
  └─ combus_links_init(comDevMaps, count)
       1. contrôles : port non nul, portée valide, débit <= plafond du port,
          type de lien et rôle du bout cohérents, activation == ALWAYS (0.4)
       2. par branchement : ouvrir le port (NodeCom), puis construire le ComBusLink
          (portée -> layer, tailles de trame de la vue, TX/RX selon le rôle)
       3. pools TX et RX, puis combus_protocol_init_all()
```

La construction du `ComBusLink` est séparée de l'ouverture du port : c'est une fonction pure, testable sans carte. Tant que les flags `COMBUS_UART_TX` et `COMBUS_UART` existent, ils choisissent quels branchements de la carte sont actifs (une dizaine de lignes dans `com_init.cpp`) ; ils disparaissent au chantier 1.

**Où vit la configuration**

| Niveau | Contenu | Où |
|---|---|---|
| Machine (figé) | ports et branchements de la carte ; liens du nœud (`ComDev`) | `envCfg.cpp` de la carte ; `board/com_dev.h` |
| Modules optionnels | son, lumière : leurs ports, devices, `.cb`, mappages de commandes | un fichier par module, inclus par un flag `HAS_*` |
| Amovibles | un emplacement par remorque ou accessoire pris en charge | liste de profils compatibles (`-D`) |

La table de correspondance entrée vers channel (`inputs_map`) devient elle aussi modulaire (fragments inclus par flag).

**Types de dispositifs**

- **Remorque** : un nœud à part entière (sa carte, son ComBus) ; lien bidirectionnel d'égal à égal, avec handshake et un ensemble réduit.
- **Accessoire** (godet, outil) : simple, non décisionnel ; **diffusion seule**, pas de handshake, retour éventuel limité à un identifiant. C'est un module à activer sur la machine. Sans communication : un `ComDev` de protocole `NONE`, sans `ComDevMap` vers un port, sert seulement à activer un `.cb` et à lier une commande à un channel.
- **Modules son et lumière** : des modules logiciels activés par `-D`, sur toute carte désignée (une carte son et lumière peut devenir la carte mère d'une petite machine). C'était l'idée des thèmes des `.cb` ; le thème `sound` est à ajouter.
- Chaque type de machine déclare, par flags `-D`, la **liste des profils compatibles** (une pelleteuse ne charge pas une remorque de semi-remorque).

### 2.2 Un lien = un ensemble de channels = une table de correspondance

- Chaque connexion a un **ensemble de channels** (« combus set ») et une **table de correspondance** entre l'ordre de la trame et les channels locaux, **générée par le builder**.
- Par défaut (télécommande vers machine, ou carte vers carte) : l'ensemble complet de la couche `BASE`. Camion vers remorque : un ensemble **réduit** (« lite ») avec seulement ce qui sert à l'attelage. Les modes spéciaux (priorité réseau, etc.) peuvent aussi être des ensembles.
- **Chaque nœud garde son ComBus fixe**, sans fusion ; le récepteur applique la trame à son ComBus par la table. Les `HAS_*` sont des décisions de configuration prises à la compilation.
- **Règles de correspondance** : deux channels mis en correspondance ont **le même nom** ; entre nœuds, ils sont **définis `BASE`** (à l'intérieur d'un nœud, un lien de carte à carte transporte aussi les channels `SYSTEM`).
- **Entre nœuds seulement**, le builder fait une **seconde lecture** des profils compatibles, uniquement pour calculer les tables, sans jamais les fusionner dans le ComBus local. À l'intérieur d'un nœud, rien ne change (un seul jeu de channels, noms uniques, diffusion ND2).
- **Pas de `requires` sur `BASE`** (règle ND4 bis conservée) : l'ensemble `BASE` d'un type est identique pour toutes ses machines, donc un seul MD5 par type. Les channels `BASE` liés à une remorque existent dans tout le type (inertes sans remorque).
- **Les tables sont générales dès le départ** : un candidat (§2.5) se place parmi les channels `BASE` dans la numérotation locale, donc l'ordre sur le fil (stables, puis candidats) n'est pas l'ordre local. La table n'est l'identité que sans candidat et avec le même ensemble des deux côtés.
- Deux cartes d'un même nœud peuvent ne pas avoir les mêmes channels (un `requires` non satisfait) : la table ignore ce qu'une carte n'a pas, sans décaler les autres.

### 2.3 Sens et relais

- Chaque côté déclare le sens **de son point de vue** ; le builder vérifie que les sens sont complémentaires. `both` est interdit ; **`both_or` est autorisé** (par exemple le failsafe partagé : un tracteur et sa remorque ne forment qu'un niveau de sécurité), avec fusion OR au niveau du lien (câblée le 2026-10-22, étape 1.1b : voir `src/core/system/combus/protocol/both_or.md`). Le profil d'un type (télécommande vers machine) est écrit du point de vue de la machine et inversé côté télécommande selon le rôle.
- À la réception, seuls les channels dont le sens le permet sont appliqués.
- **`passthrough`** (paramètre du `.cb`) : un nœud relais consomme une valeur reçue et la relaie aux autres nœuds connectés qui la reçoivent. Exemple du frein : télécommande `uplink`, machine `downlink` et `passthrough`, remorque `downlink`. Règles :
  - le builder calcule le rôle de la machine sur chaque lien (pair émetteur : elle reçoit ; pair récepteur : elle émet) ;
  - un seul channel local, utilisé aussi par la machine ; une seule source par channel relayé ;
  - perte de la source : retour à la **valeur par défaut sûre** du `.cb`, relayée telle quelle ;
  - jamais de retour vers le lien d'origine ; pour un `both_or` relayé, la valeur envoyée sur un lien est le OR du local et des **autres** liens (sinon un `true` ne retombe jamais) ;
  - pas d'anneau de relais ; une période d'émission de latence par relais (20 ms à 50 Hz) ;
  - contrôles au build : deux récepteurs sans `passthrough` : avertissement ; deux émetteurs : erreur sauf `*_or` ; `passthrough` sans récepteur en aval : avertissement.

### 2.4 Handshake et état du lien

- **MD5 par ensemble** de channels (donc par connexion), et plus pour tout le profil : c'est la vérification du lien et la signature du dispositif. À la mise en place, le MD5 change une fois. Le builder affiche le MD5 de chaque ensemble dans son journal de compilation.
- **Demande et ACK** : la demande est la salve de handshake ; la réponse est une trame de handshake marquée **ACK** (un bit de l'octet `flags`, aujourd'hui à zéro dans le handshake), qui contient le MD5 du répondeur. Format de trame inchangé ; on ne répond jamais à un ACK. Les deux côtés connaissent ainsi le MD5 de l'autre et choisissent le même mode.
- Le récepteur compare le MD5 reçu au sien : identique, table symétrique ; différent, il cherche dans sa liste la table prévue pour ce MD5.
- **État de lien** (inconnu, stable, next) : les trames reçues ne sont pas appliquées tant que le mode n'est pas choisi ; sur un lien bidirectionnel, l'émission attend aussi.
- **Le mode est fixé une fois** au handshake et vaut jusqu'à la perte de lien suivie d'une reconnexion. La perte de lien (délai, failsafe) remet l'état à « inconnu ».
- **ACK périodique** (à cadence lente, par exemple 1 Hz) : il prouve que **le pair me reçoit** (les trames de données ne prouvent que l'inverse) et détecte un pair reflashé. « Hors contrôle » seulement après plusieurs ACK manqués. Côté machine, la sécurité reste fondée sur sa propre réception des commandes (`REMOTE_LINK_LOST`) ; l'ACK périodique sert surtout à la télécommande.
- **Perte de lien** (étape 1.1) : un lien qui reçoit est perdu tant qu'il n'a rien reçu, puis après `linkTimeoutMs` de silence (paramètre du `ComProtocolCfg`). `UART_LINK_LOST` publie l'OU des liens perdus ; un lien qui n'émet que ne contribue pas. Tant que le filtrage par `direction` n'existe pas (2.4), un lien qui reçoit écrase les channels que son pair lui envoie.
- **Lien à diffusion seule** (accessoire) : pas d'ACK ; salve périodique et espacée ; le mode est déclaré dans le `ComDev`.

### 2.5 Versions et channels candidats

- **Seules les majeures cassent** : la version majeure de `project_version.h` est la seule comparée. Passer de V1 à V2 impose de reflasher tous les véhicules ; une version mineure reste rétrocompatible.
- **Un channel `BASE` n'est ajouté qu'à un changement de version majeure** : c'est une discipline, sans fichier dédié ; un écart se voit au handshake (MD5 différent).
- **Channel candidat** : un `.cb` peut marquer un channel `BASE` comme `candidate` (« à tester »). Il est dans le ComBus du nœud mais hors de l'ensemble stable et du MD5 stable : l'ajouter ne casse aucun pair. Son nom porte un **suffixe** (par exemple `_NEXT`), vérifié par le builder ; retiré à la promotion. Le statut facilite la migration par script.
- **Essai next** : chaque ensemble a un **MD5 stable** (sans candidats) et un **MD5 next** (stable suivi des candidats). Le nœud émet une salve avec le MD5 next puis une avec le MD5 stable ; le récepteur compare chacun aux siens : next identique, **mode next** ; sinon stable identique, **mode stable**. En mode next, les candidats sont transmis à la suite des stables ; en mode stable, ils gardent leur valeur par défaut sûre. C'est l'émetteur qui décide de diffuser un candidat. Les deux nœuds doivent avoir la même release (bêta ou nightly pour les essais).
- **Promotion** : à la majeure suivante, les candidats retenus entrent dans l'ensemble stable (à la suite).
- Proposition à valider : un channel **critique** (`FAILSAFE`, `RUNLEVEL`…) ne peut pas être candidat, pour qu'un retour en mode stable ne coupe jamais la sécurité ; un même nom de channel n'a qu'un seul type dans le dépôt.

### 2.6 Deux portées : `BASE` puis `SYSTEM`

| | `BASE` | `SYSTEM` |
|---|---|---|
| Lien | entre nœuds (télécommande, remorque) | entre cartes d'un même nœud |
| Un ensemble stable par | type de machine | nœud (machine précise) |
| Contenu haché | nom, type, sens, dans l'ordre | nom, type, dans l'ordre (le sens varie selon la carte et est déjà canonisé pour le MD5) |

Le même mécanisme vaut pour les deux : en intra-nœud, une carte mise à jour seule (une carte son) peut exiger une fonction à partir d'une release donnée, qui reste dormante en mode stable. Le lien entre deux cartes porte la partie `BASE` suivie de la partie `SYSTEM`. Mise en œuvre en deux temps : `BASE` d'abord, `SYSTEM` ensuite.

### 2.7 Télécommande : plusieurs machines pilotées

- **Un espace de noms par type de machine** accepté, généré pour l'env télécommande (les mêmes noms, comme `STEERING_BUS`, existent dans plusieurs types). Les fichiers qui mappent les entrées de la télécommande et les sorties utilisent cet espace de noms (par exemple `DumperTruck::AnalogComBusID::STEERING_BUS`, ou un alias `namespace M = DumperTruck;`).
- Un espace de noms se choisit **à la compilation** ; le choix **à l'exécution** passe par un **tableau de descripteurs**, un par machine pilotée (nom, type, ComBus de cette machine, table des entrées de son type). La variable d'exécution est l'indice de la machine active.
- **Une instance de ComBus par machine pilotée**, rattachée à son lien, en réception comme en émission (le TX prend le ComBus du lien).
- La portée globale reste le **ComBus propre du nœud** (pour la télécommande : batterie, failsafe, entrées) ; le code du cœur continue de travailler dessus.
- Pas de montage d'un ComBus par pointeur.

### 2.8 Robustesse

- **Tampons de réception générés par le builder**, par ensemble de lien et dimensionnés par ses constantes : la taille ne s'écrit plus à la main ; un tampon trop petit ne compile pas. Dimensionnés pour l'ensemble next.
- Les numériques en trop font refuser la trame (comme les analogiques), au lieu d'être tronqués.
- Une trame fait 255 octets au plus (longueur sur un octet), environ 35 aujourd'hui ; un ensemble reste aussi sous la charge utile ESP-NOW (de l'ordre de 250 octets, à vérifier).

## 3. Roadmap

Chaque étape se vérifie avec le test de boucle (loopback) puis un build complet. Un chantier ne démarre que quand le précédent est validé. Les chantiers 0 et 1 ne changent pas le comportement visible de la machine.

### Chantier 0 : préalables

**Constats** :
- 0.1 : le RX appliquait les trames avec une identité d'écriture constante (`BASE`), ce qui refusait les channels `SYSTEM` d'un lien carte à carte ; le cœur accédait au `comBus` global à deux endroits (`combus_rx.cpp`, invalidation du handshake sur failsafe ; `vbat.cpp`). Fait (2026-10-19) : `combus_rx_init` prend le `layer` du lien (un lien BOARD reçoit l'identité SYSTEM ; `UNDEFINED` avec une cible = FATAL), `combus_rx_update(const ComBus*)` et `vbat_update(ComBus&)` reçoivent le ComBus du nœud en paramètre.
- 0.2 : `test_combus_loopback` ne compilait plus (14 erreurs « too many arguments » sur `combus_frame_encode` et `combus_tx_update`). En le reprenant, d'autres défauts sont apparus : bus de test trop petit pour le profil réel (l'écriture de `RUNLEVEL` débordait du tableau), `seq = 0` utilisé alors que l'encodeur le refuse, tailles de trame codées en dur et périmées, pool UART non enregistré avant `uart_com_init`, test d'un bit failsafe d'en-tête retiré (FS2). Au premier passage sur la carte, les trois `test_md5_resolution_*` échouaient : le lien de test avait un transport nul, que `combus_protocol_init` refuse avant de résoudre le MD5, puis le test comparait des adresses alors que les tableaux MD5 générés sont `static constexpr` (une copie par fichier compilé). Corrigé : transport factice non nul et comparaison du contenu.
- 0.3 : le filtre par type était inactif : `MACHINE_TYPE_*` était un `#define` dans l'en-tête de la machine, pas un `-D` visible du builder. Fait (2026-10-20) : le flag est passé en `-D` dans `platformio.ini` (`[volvo_A60H_id]`, `test_combus_loopback`) et le `#define` est retiré de l'en-tête.
- 0.4 : l'initialisation est statique : `combus_uart_init.cpp` construit un seul lien UART à partir des flags `COMBUS_UART*` ; `uartPins[]` (dans l'`envCfg.cpp` de la carte) n'a pas d'autre utilisateur. Préalable fait (2026-10-20) : `mainboard/` est rangé sous `volvo_A60H_bruder/board/` (déplacement seul, builds inchangés). Au démarrage de la Volvo, `sys_init()` réservait le port de debug avant que `hw_init_com()` n'enregistre le pool de ports : `FATAL: no pool registered ('debug')` (ancien défaut d'ordre, sans lien avec le rework). Corrigé (2026-10-21) : `hw_init_com()` est appelée au début de `sys_init()` et enregistre le pool sans condition de flag ; vérifié sur la carte.

| Étape | Livrable | Preuve |
|---|---|---|
| 0.1 | RX : identité d'écriture = `layer` du lien ; suppression des deux accès du cœur au `comBus` global | **validé sur la carte (2026-10-20)** : groupe H du test Unity (lien `SYSTEM` applique les channels `SYSTEM`, lien `BASE` les refuse, lien `BOARD` = identité `SYSTEM`) ; builds Volvo et `test_node_mainboard` OK |
| 0.2 | Test Unity `test_combus_loopback` aux signatures actuelles, bus de test = ComBus généré, tailles calculées depuis les `CH_COUNT` | **validé sur la carte (2026-10-20)** : 28 tests sur 28 (pont TX↔RX sur Serial2 pour les groupes B et H) |
| 0.3 | Filtre par type de machine réparé (préalable aux profils compatibles) | **fait (2026-10-20)** : le hook journalise `machine_type='dumper_truck'` sur la Volvo et `test_node_mainboard` ; fichiers générés identiques à l'état précédent ; `test_combus_loopback` compile |
| 0.4 | `ComPort` (UART seul) et `ComDevMap`, `ComDev` du lien ComBus UART actuel (d'abord déclaré inter-nœud dans `combus_uart.h`, car l'ancien code le mettait en `BASE` : le fil reste identique, voir 0.4b) ; `combus_links.cpp` ouvre les ports et construit les liens ; `combus_uart_init.cpp` ne garde que le choix du lien d'après les flags ; `uartPins[]` retiré ; champs d'activation et de couplage réservés (`ALWAYS` seul accepté) ; **temporaire** : les tampons RX de `ComLinkRuntime` sont dimensionnés pour la vue complète, à remplacer par les tampons générés (2.3). `board/com_dev.h` (liens intra-nœud) est créé avec le premier lien intra-nœud | **validé sur la carte (2026-10-21)** : groupe I du test Unity (construction du lien sans câblage), 33 tests sur 33 ; builds Volvo et `test_node_mainboard` OK. Le lien est déclaré `INTER_NODE` pour garder le fil identique. Suite : 0.4b |
| 0.4b | Le lien d'extension (`COM/GPIO`) passe en `INTRA_NODE` (vue `SYSTEM`) : ses `ComDev` (`kComDevExtUni`, `kComDevExtBi`) déménagent dans `board/com_dev.h` avec leur `ComProtocolCfg` (115200 bauds, 50 Hz), qui quitte `combus_uart.h` | **fait (2026-10-21)** : builds Volvo et `test_node_mainboard` OK ; la construction d'un lien `INTRA_NODE` est couverte par le groupe I (33 tests sur 33 sur la carte). La trame passe de la vue `BASE` (9 analogiques + 9 numériques) à la vue `SYSTEM` (13 + 20, d'après les fichiers générés de `test_node_mainboard`), avec le MD5 `SYSTEM`. **Vérifié sur la carte (2026-10-21)**, Volvo flashée : `[UART_COM] 'combus' — baud=115200  tx=18  rx=-1` (TX seul, broche RX non réservée) puis `[COMBUS_TX] init — link=0  transport='combus'  rate=50Hz  A13+D20  wireEnd=A13+D20  layer=2` |

### Chantier 1 : lien vivant

**Constats** :
- `combus_rx_update(const ComBus*)` n'avait aucun appelant dans `src/` (seul le test Unity l'appelait). Fait (2026-10-21) : `com_rx_update()` l'appelle en tête de `sys_manager_update`.
- `UART_LINK_LOST` (défaut « perdu ») n'était écrit nulle part, malgré les commentaires qui parlent d'un code qui interroge `combus_rx_is_alive()` (jamais écrit, et jamais déclarée dans `combus_rx.h`). Avec `COMBUS_UART_TX` et `HAS_REMOTE_LINK_LOST_FALLBACK`, la Volvo avait donc le contributeur UART de la chaîne bloqué à « perdu » : d'après le code, `REMOTE_LINK_LOST` restait à 1 et la réaction (sur front) ne pouvait plus se déclencher après le démarrage. Fait (2026-10-21) : `com_rx_update()` publie `UART_LINK_LOST` d'après les liens qui reçoivent, et le contributeur n'existe plus que pour `COMBUS_UART` (un lien TX seul ne sait rien de son pair).
- Le handshake compare MD5 et version (majeure et mineure) en égalité stricte, mais un désaccord est seulement journalisé : aucune trame n'est bloquée.
- La salve n'est émise qu'à l'initialisation du TX (3 trames en 300 ms) et s'arrête dès que le nœud a validé le MD5 de son pair ; rien ne répond à un handshake reçu. Un nœud qui démarre après son pair ne reçoit jamais le MD5 de ce pair.
- Constaté sur la Volvo (2026-10-21), manette PS4 connectée : `REMOTE_LINK_LOST` restait à 1. `input_update.cpp` écrivait `PS4_DS4_BT_LINK_LOST` avec l'identité `ChanLayer::SYSTEM`, alors que ce channel est de portée `BOARD` : `_layer_ok` refuse l'écriture, sans message hors `DEBUG_COMBUS` (défaut apparu avec le renommage des portées du 2026-10-18). Corrigé : identité `BOARD`. Les refus d'écriture sont désormais comptés (`combus_denied_writes()`, affiché sur la ligne « combus state » du tableau de bord) et signalés une fois par channel par `sys_log_warn` (builds `DEBUG_SYSTEM`) ; ils ne dépendent plus de `DEBUG_COMBUS`.
- `kRemoteLinkRunlevelCfg` avait `.low = RunLevel::STARTING`, ce qui contredisait la doc (« front descendant = reprise manuelle à la clé ») : avec le défaut ci-dessus corrigé, la machine aurait démarré seule à la connexion de la manette. Corrigé : `.low = std::nullopt`, la machine reste en `IDLE` jusqu'à l'appui sur la clé, après une perte de lien comme au premier branchement.
- L'ancien RX du projet son (dépôt externe `API-rep/sound_module_rc` @ `c28a471`, ancienne API) n'apporte que l'idée d'une détection de perte de lien par lien.

| Étape | Livrable | Preuve |
|---|---|---|
| 1.1 | RX branché (`com_rx_update()` en tête de boucle, `com_update()` reste l'émission en fin de boucle), perte de lien par lien (`linkTimeoutMs` dans le `ComProtocolCfg`, 250 ms pour le lien d'extension), `UART_LINK_LOST` publié, cible = ComBus du nœud pour les liens qui reçoivent | **validé sur la carte (2026-10-21)** : groupes J et K du test Unity (5 tests : perdu avant la première trame, vivant après une trame, perdu après le silence, lien TX seul jamais perdu, écriture refusée comptée), 38 tests sur 38. Sur la Volvo flashée : manette connectée, `REMOTE_LINK_LOST` passe à off et le runlevel reste `IDLE` ; sans manette il est à on ; `denied: 0` au tableau de bord. Builds Volvo et `test_node_mainboard` OK. Aucun env n'a de lien qui reçoit : pas d'effet sur la Volvo, sauf la chaîne `REMOTE_LINK_LOST` (voir constat) |
| 1.1b | Fusion `both_or` effective, anti ping-pong : valeur de la carte gardée à part (`localValue`), cache des valeurs reçues par lien, `valeur = carte OU liens vivants` réévaluée à chaque cycle, émission sans écho, une carte qui n'a jamais écrit le channel ne contribue pas ; doc `both_or.md`. Préalable : un dossier `combus_generated` par env (le dossier partagé faisait compiler un env avec les fichiers d'un autre depuis le nouveau `requires`) | **validé sur la carte (2026-10-22)** : groupe L du test Unity (5 tests : fusion, retour à `false`, pair silencieux, pas d'écho, carte qui n'écrit jamais), 43 tests sur 43 ; builds Volvo et `test_node_mainboard` OK |
| 1.2 | État de lien et blocage ; ACK de handshake ; ACK périodique | pair allumé plus tard : le lien s'établit ; perte puis reconnexion : renégociation |

### Chantier 2 : contrats `BASE`

**Constats** :
- `direction` et `bothOr` sont générés mais ignorés à l'émission et à la réception.
- Les tampons de réception sont passés par pointeur seul, à côté d'un nombre de channels déclaré à part : rien ne vérifie leur taille. Le décodeur refuse les analogiques en trop mais tronque les numériques en silence. Le tampon circulaire, lui, ne déborde pas (plein, il écrase l'octet le plus ancien).
- Les champs (`ALLOWED_FIELDS`) et les thèmes (`VALID_THEMES`) du builder sont des listes fermées.

| Étape | Livrable | Preuve |
|---|---|---|
| 2.1 | MD5 par ensemble, affiché au build ; contrôles du builder (§2.5) | tests unitaires ; jeu de channels de la Volvo équivalent à celui d'avant |
| 2.2 | Statut `candidate`, suffixe, ensemble et MD5 next ; essai next en deux salves | même release : next ; releases différentes : stable |
| 2.3 | Tables de correspondance et tampons générés, utilisés par TX et RX (remplacent les tampons temporaires de `ComLinkRuntime`, 0.4) | test de boucle identique ; un tampon trop petit ne compile pas |
| 2.4 | Filtrage par `direction` à la réception | un channel émis localement n'est pas écrasé par le pair |

### Chantier 3 : égal à égal et relais

**Constat** : un nom de channel est unique dans tout ce que le builder charge (`ChannelConflictError`) ; la seconde lecture des profils compatibles ne doit donc jamais passer par la fusion du ComBus local.

| Étape | Livrable | Preuve |
|---|---|---|
| 3.1 | Seconde lecture des profils compatibles ; ensembles réduits ; étiquette d'égal à égal | tables générées pour un couple tracteur / remorque d'essai |
| 3.2 | `passthrough` et ses contrôles (la fusion `both_or` sans retour vers l'origine est faite, 1.1b ; un relais devra suivre la même règle) | frein relayé ; failsafe partagé qui retombe |
| 3.3 | Couplage : `ComCoupling`, activation `ON_COUPLING` d'après un channel déclencheur (`.cb` conditionnel du dispositif, par exemple un lecteur RFID) | un lien s'active au couplage et se suspend au découplage |

### Chantier 4 : télécommande

**Constats** :
- L'env `remotes` est vide (`src/remotes/main.cpp`, 9 lignes) : la télécommande part de zéro.
- Le générateur n'émet qu'un ComBus par env, en portée globale ; l'émission prend un seul ComBus pour tous les liens (`combus_tx_update(const ComBus* bus)`).
- Déjà en place : l'API du cœur prend le ComBus en paramètre (`combus_set_analog(ComBus&, …)`) et le protocole vise un ComBus cible par lien (`ComBusLink.target`).

| Étape | Livrable | Preuve |
|---|---|---|
| 4.1 | Espaces de noms par type et instances de ComBus par machine (générateur) ; TX par lien | une télécommande d'essai compile avec deux types |
| 4.2 | Descripteurs de machines pilotées ; mapping des entrées par type | une télécommande d'essai pilote deux machines |

### Chantier 5 : transports et dispositifs

**Constat** : seul l'UART a un adaptateur `NodeCom` ; ESP-NOW est cité comme futur dans `protocol/~README.md` et dans la doc de `ComBusLink`.

| Étape | Livrable | Preuve |
|---|---|---|
| 5.1 | Adaptateur ESP-NOW (un tampon par pair) ; `ComDev` globaux du cœur, paramétrés par `-D`, et `ComPort` sans broches (contrôleur interne) ou avec broches (module radio) | télécommande vers machine en ESP-NOW |
| 5.2 | Remorque (nœud), accessoires (diffusion seule, I2C ou `NONE`) | attelage et accessoire d'essai |
| 5.3 | Ports I2C et CAN (autres variantes de `ComPort`) | un port I2C ou CAN s'ouvre depuis la table de la carte |

### Chantier 6 : portée `SYSTEM`

| Étape | Livrable | Preuve |
|---|---|---|
| 6.1 | Ensemble stable `SYSTEM` par nœud et son MD5 | contrôles du chantier 2 sur un nœud à deux cartes (`test_node_*`) |
| 6.2 | Tables pour des cartes aux channels différents ; MD5 stable et next de la vue `SYSTEM` | deux cartes avec des `requires` différents restent liées |
| 6.3 | Négociation du mode entre cartes (même code qu'en 1.2 et 2.2) | une carte reflashée seule retombe en stable, la sécurité continue |

**Validation de fin de rework** (décidé le 2026-10-22) : les tests sont écrits par le même auteur que le code, donc exposés aux erreurs de mode commun (une mauvaise compréhension d'une règle passe dans le code et dans son test). En fin de rework :
- relecture des tests par un **agent tiers** (sous Cline), à partir des règles et des exemples validés par le propriétaire, sans le code ;
- **accès direct aux cartes** pour compiler, téléverser et lancer les tests sans intermédiaire ;
- **monkey test** de longue durée sur les liens.
D'ici là, chaque règle est validée par le propriétaire sur un exemple avant le code, et chaque test doit échouer sur l'ancien code et passer sur la carte.

**Côté builder, en cours de route** : champs à ajouter à `ALLOWED_FIELDS` (statut candidat, étiquette d'égal à égal, `passthrough`, et `critical` si retenu) ; thème `sound` dans `VALID_THEMES`.

## 4. Questions ouvertes

- Format de la déclaration d'un ensemble réduit et de l'étiquette d'égal à égal (champ des `.cb` ou fichier dédié) ; format exact du paramètre `passthrough`.
- Nom du suffixe de candidat.
- **Identité d'écriture locale** (`ChanLayer caller` de `combus_set_analog` / `combus_set_digital`) : pour du code local, c'est toujours « ce firmware » (`BOARD`), et le paramètre est un piège (13 des 15 appels explicites de `src/` passent `SYSTEM`, ce qui est refusé sur un channel `BOARD`, comme ci-dessus). L'identité garde son sens à la frontière d'un lien (`combus_frame_apply`), et devrait y être remplacée par la table des channels et le sens du lien (2.3, 2.4). Fait : le refus est visible (compteur et message unique par channel). Reste : migrer les appels locaux vers la forme sans identité, puis retirer le paramètre de l'API publique quand l'application des trames passe par la table.
- Nom du jeu de `ComDev` fournis par le cœur (« catalogue » est provisoire), et forme de leur paramétrage par `-D` (flags par modèle, ou une table).
- Contenu exact de `ComCoupling` (channel déclencheur, valeur attendue, identifiant du pair) ; comportement d'un lien inactif (état d'exécution, ressources réservées).
- Où définir un `ComDev` inter-nœuds partagé par deux types de nœuds (machine et télécommande) sans le dupliquer.
- Le backend PS4 Bluetooth reste dans `input` : c'est un dispositif d'entrée, dont les boutons logiques sont mappés vers le ComBus par `inputs_map` ; il n'expose pas de flux d'octets comme un `NodeCom`. Seule sa santé de lien est dans `com/link_health/`. À reconsidérer quand la télécommande deviendra un nœud (chantier 4).
- Cadence et seuil de l'ACK périodique ; renégociation quand un ACK périodique annonce un MD5 différent.
- Channels critiques et unicité des types (propositions du §2.5).
- ESP-NOW : nombre maximal de pairs de l'ESP32, charge utile exacte (documentation ESP-IDF).
- Un module qui fait de sa carte la carte mère : ressources minimales à vérifier au démarrage.
- Hors périmètre pour l'instant : deux nœuds de même type en égal à égal.
