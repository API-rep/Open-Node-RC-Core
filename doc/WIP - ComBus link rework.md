# Étude préliminaire : liens ComBus multi-instances et multi-protocoles

Statut : **en cours de rédaction** (document `WIP`), étude du 2026-10-18. Rien n'est implémenté ici.
Public : le propriétaire et les futures sessions de travail sur le rework de la communication.

## 1. Objectif

Un seul mécanisme de communication, pour tous les cas : plusieurs liens en parallèle (TX et RX sur plusieurs ports), plusieurs protocoles (UART aujourd'hui ; ESP-NOW, I2C, WiFi ensuite), qu'il s'agisse d'une télécommande vers une machine, d'une carte vers une carte, ou d'une machine vers une remorque. Objectif de fin d'hiver 2027 du roadmap.

## 2. Où on en est (vérifié dans le code)

- Le protocole ComBus est **déjà multi-liens et indépendant du transport** : TX et RX tiennent un pool d'états par lien, bouclent sur tous les liens, et ne parlent qu'à `NodeCom`. Seule l'**initialisation** est statique : `combus_uart_init.cpp` construit un seul lien UART à partir des flags `COMBUS_UART*`.
- Seul l'**UART** a un adaptateur. ESP-NOW est cité comme futur dans `protocol/~README.md` et dans la doc de `ComBusLink`.
- Le **RX** n'est pas branché : `combus_rx_update()` n'a aucun appelant dans `src/` (seul le test Unity l'appelle). Il applique les trames avec une identité d'écriture constante (`BASE`), ce qui refuserait les channels `SYSTEM` d'un lien carte à carte.
- `direction` et `bothOr` sont générés mais **ignorés** à l'émission et à la réception.
- Le handshake compare le MD5 et la version (majeure **et** mineure) en **égalité stricte**, mais un désaccord est seulement **journalisé** (`MISMATCH`) : aucune trame de données n'est bloquée. La salve de handshake n'est émise qu'à l'initialisation du TX (3 trames en 300 ms) et s'arrête dès que le nœud a validé le MD5 de son pair.
- Le **filtre par type de machine est inactif** aujourd'hui (détail et options dans `doc/combus_builder_handoff.md`).
- L'ancien RX du projet son vivait dans un dépôt externe (`API-rep/sound_module_rc` @ `c28a471`, sous-module retiré en `da982a8`) avec une ancienne API ; peu de code à récupérer. Reste l'idée d'une détection de perte de lien par lien (`is_alive`), à recréer.

## 3. Le modèle proposé

### 3.1 Trois briques de configuration

| | Rôle | Où |
|---|---|---|
| **NodeCom** | envoyer et lire des octets (existe) | `core/system/hw/node_com.h` |
| **ComPort** | le matériel : UART et broches, canal ESP-NOW, adresse I2C. La liste d'une carte peut être exhaustive ; seuls les ports utilisés sont initialisés (et leurs broches réservées) | structure dans `include/struct/`, tableau dans la config de la carte |
| **ComDev** | un lien logique : ID, nom, port, portée (`BASE` ou `SYSTEM`), ensemble de channels (par défaut ou réduit), tx / rx, pair ; pour un lien à diffusion seule, le mode (stable ou next) | idem |

Le cœur lit ces tables, ouvre les adaptateurs et construit les liens. Les limites matérielles de l'ESP32 (nombre d'UART, taille des pools, pairs ESP-NOW) restent des constantes de la carte, vérifiées au démarrage. Le rôle machine / télécommande vient de `IS_MACHINE` / `IS_REMOTE`.

### 3.2 Un lien = un ensemble de channels = une table de correspondance

- Chaque connexion a un **ensemble de channels** (« combus set ») et une **table de correspondance** entre l'ordre de la trame et les channels locaux, **générée par le builder**.
- Par défaut (télécommande vers machine, ou carte vers carte) : l'ensemble complet de la couche `BASE`, table symétrique (l'identité tant qu'il n'y a pas de candidat, voir §8 point 6).
- Camion vers remorque : un ensemble **réduit** (« lite ») avec seulement ce qui sert à cet attelage. Les modes spéciaux (priorité réseau, etc.) peuvent aussi être des ensembles.
- Le récepteur applique la trame à son propre ComBus par la table. **Chaque nœud garde son ComBus fixe**, sans fusion ; les `HAS_*` sont des décisions de configuration prises à la compilation.
- À la réception, seuls les channels dont le sens le permet sont appliqués. Les sens se déclarent chacun de son point de vue et doivent être complémentaires ; `both` est interdit, **`both_or` est autorisé** (par exemple le failsafe partagé entre un tracteur et sa remorque, qui ne forment qu'un niveau de sécurité), ce qui demande la fusion OR au niveau du lien. Un nœud relais déclare le channel `passthrough` (§8 point 1).

### 3.3 Un MD5 par ensemble

- Le MD5 est calculé **par ensemble de channels**, donc par connexion, et plus pour tout le profil. C'est la vérification du lien et la signature du dispositif.
- Au handshake, l'émetteur envoie le MD5 de l'ensemble de sa connexion. Le récepteur le compare au sien : identique, table symétrique ; différent, il cherche dans sa liste la table prévue pour ce MD5.

### 3.4 Versions : seules les majeures cassent

Objectif : qu'une mise à jour d'un véhicule ne rende pas inutilisable le reste du parc. Seul le passage d'une version majeure à la suivante (V1 vers V2) impose de reflasher tous les véhicules.

**Trois règles du propriétaire**

1. Deux channels mis en correspondance ont **le même nom**.
2. Deux channels mis en correspondance sont **définis `BASE`**. (Entre nœuds. À l'intérieur d'un nœud, un lien de carte à carte transporte aussi les channels `SYSTEM`.)
3. Un channel `BASE` n'est **ajouté qu'à un changement de version majeure**.

**Mécanisme proposé pour les respecter**

- **La règle 3 est une discipline**, sans fichier ni mécanisme dédié. Un changement accidentel de l'ensemble stable dans une même version majeure se voit au handshake (MD5 stable différent : lien refusé une fois le blocage mis en place, voir §8 point 5). Le builder affiche le MD5 stable de chaque ensemble dans son journal de compilation, pour le repérer plus tôt. L'ordre sur le fil reste celui du tri actuel.
- **Channel candidat.** Un `.cb` peut marquer un channel `BASE` comme `candidate` (« à tester, prochaine fonctionnalité »). Il est généré dans le ComBus du nœud mais **n'est ni dans l'ensemble stable ni dans le MD5 stable** : l'ajouter ne casse aucun pair. Ce statut facilitera la migration par script. Son nom porte un **suffixe de candidat** (à choisir, par exemple `_NEXT`), vérifié par le builder : cela évite les collisions de noms et les faux positifs de MD5. À la promotion, le suffixe est retiré. Aucune règle d'ordre supplémentaire : le tri actuel suffit, car un MD5 différent veut simplement dire « pas de mode next ».
- **Essai « next » à la connexion : deux salves de handshake.** Chaque ensemble a deux MD5 : le **MD5 stable** (l'ensemble sans candidats) et le **MD5 next** (le stable suivi des candidats). Le nœud émet d'abord une salve de handshake avec le MD5 next, puis une salve avec le MD5 stable. Le récepteur compare chaque MD5 reçu aux deux siens : correspondance avec son MD5 next, **mode next** ; sinon correspondance avec son MD5 stable, **mode stable** (legacy). Le format du handshake ne change pas. Sur un lien bidirectionnel, le pair qui reçoit ces salves répond par ses propres salves marquées **ACK** (§8 point 4), pour que chacun connaisse les MD5 de l'autre. Sur un lien à diffusion seule, pas de réponse : le mode est déclaré dans le `ComDev`.
  - En mode next, les candidats sont appliqués à la réception, placés à la suite des channels stables. En mode stable, ils restent à leur valeur par défaut sûre (celle du `.cb`).
  - C'est l'émission qui décide de diffuser un candidat. L'autre nœud le consomme s'il prend en charge la fonctionnalité : aucun souci de compatibilité du code.
  - Contrainte : les deux nœuds doivent avoir le même ensemble de candidats, donc la même release (bêta ou nightly pour les essais). Règle de bonne pratique : une version mineure reste rétrocompatible avec la version actuelle.
- **Trames de données : même fonctionnement en mode next et en mode stable.** Le choix next / stable se fait **une seule fois au handshake** et vaut jusqu'à la prochaine perte de lien suivie d'une reconnexion. Chaque côté évalue les MD5 annoncés par le pair avec la même règle, donc les deux côtés aboutissent au **même mode**. La longueur des trames émises suit le mode : stable, le préfixe stable ; next, avec les candidats. Aucun changement du décodeur et aucune trame en double.
- **Lien à diffusion seule** (accessoire) : l'émetteur ne reçoit rien, donc il ne négocie pas. L'ensemble utilisé (stable ou next) est déclaré dans le `ComDev` du lien.
- **Reconnexion** : la perte de lien (aucune trame pendant le délai prévu) remet le mode à zéro ; la prochaine demande reçue relance l'évaluation, et le nœud répond par un ACK pour que le pair redémarré apprenne le mode. Un ACK périodique (§8 point 4) détecte aussi un pair reflashé en cours de session. À concevoir avec la détection de perte de lien par lien (étape 5 du plan).
- **Promotion.** À la prochaine version majeure, les candidats retenus entrent dans l'ensemble stable (à la suite) ; le MD5 stable change, c'est le changement de majeure.
- **Point d'attention** : le code qui utilise un candidat doit tolérer son absence en mode stable (valeur par défaut, pas de plantage).


### 3.5 Portée du mécanisme : inter-nœud et intra-nœud

La mécanique (ensemble de channels, table, MD5 stable et next, négociation au handshake) est la même pour un lien **entre nœuds** (portée `BASE`) et pour un lien **entre cartes d'un même nœud** (portée `SYSTEM`). Le protocole ne connaît pas la différence ; seule change l'origine de l'ensemble. Décision de principe : **l'appliquer aux deux**.

Intérêt en intra-nœud : deux cartes d'un même nœud n'ont pas toujours la même version (une carte son mise à jour seule). Une fonction ne peut alors être exigée qu'à partir d'une release donnée : en mode stable, la fonction reste dormante, sans casser le reste.

Conditions pour l'intra-nœud (développées en §3.6 et §3.7) : les channels de sécurité restent dans la partie stable ; l'ensemble stable `SYSTEM` est défini par nœud ; mise en œuvre en deux temps (`BASE` puis `SYSTEM`).

### 3.6 Les deux portées et les contrôles du builder

| | `BASE` | `SYSTEM` |
|---|---|---|
| Un ensemble stable par | type de machine | nœud (machine précise) |
| Contenu haché | nom, type, sens, dans l'ordre | nom, type, dans l'ordre (le sens varie selon la carte et est déjà canonisé pour le MD5) |

Le lien entre deux cartes porte la partie `BASE` suivie de la partie `SYSTEM`.

**Pourquoi des tables même entre cartes d'un même nœud.** Deux cartes peuvent ne pas avoir les mêmes channels (un `requires` non satisfait sur l'une). Les identifiants locaux du générateur restent denses ; la table relie l'ordre de la trame aux identifiants locaux, et ignore ceux qu'une carte n'a pas, sans décaler les autres. Quand les deux côtés ont le même ensemble et qu'il n'y a pas de candidat, la table est l'identité (voir §8 point 6).

**Contrôles du builder**

- Un channel `candidate` porte le suffixe de candidat (décision du propriétaire).
- Proposition : un channel **critique** (`FAILSAFE`, `RUNLEVEL` et leurs semblables) ne peut pas être candidat, pour qu'un retour en mode stable ne désactive jamais la sécurité. Marquage par un champ `critical` dans le `.cb` (à ajouter aux champs autorisés), ou liste fixée dans le builder.
- Proposition : un même nom de channel n'a pas deux types différents dans le dépôt.

**Conséquence** : à la mise en place, le MD5 stable change une fois, parce qu'il ne porte plus sur la même chose (ensemble stable par connexion, et non vue entière).

### 3.7 Mise en œuvre en deux temps

**Préalable** : étapes 1 à 4 du plan (§6) : identité d'écriture du RX et suppression des accès au ComBus global, test Unity, filtre par type de machine, `ComPort` / `ComDev` pour l'UART.

**Temps 1 : portée `BASE`** (télécommande vers machine, remorque vers machine)

| Étape | Livrable | Preuve |
|---|---|---|
| 1.1 | MD5 stable calculé sur l'ensemble non candidat, affiché dans le journal de compilation ; contrôles du §3.6 | tests unitaires ; le jeu de channels de la Volvo reste équivalent à celui d'avant |
| 1.2 | Statut `candidate`, suffixe, ensemble next et MD5 next ; candidats purement locaux | channel candidat présent dans le ComBus, absent du MD5 stable |
| 1.3 | Tables de correspondance et tampons de réception générés par le builder, utilisés par le TX et le RX même quand les tables sont l'identité | le test de boucle donne les mêmes résultats qu'avant ; un tampon trop petit ne compile pas |
| 1.4 | État de lien (inconnu, stable, next) et blocage des trames ; handshake en deux salves avec ACK ; ACK périodique ; renégociation à la reconnexion | deux nœuds de même release : next ; releases différentes : stable ; pair allumé plus tard : le lien s'établit ; perte de lien puis reconnexion : renégociation |
| 1.5 | Étiquette d'égal à égal, ensembles réduits, filtrage par `direction`, `passthrough`, fusion `both_or` sans retour vers l'origine | lien d'essai de type télécommande / tracteur / remorque en boucle ; frein relayé ; failsafe partagé qui retombe |
| 1.6 | Télécommande : espace de noms par type accepté, instance de ComBus par machine pilotée, TX par lien, descripteurs de machines pilotées | une télécommande d'essai pilote deux machines |

**Temps 2 : portée `SYSTEM`** (carte vers carte)

| Étape | Livrable | Preuve |
|---|---|---|
| 2.1 | Ensemble stable `SYSTEM` par nœud et son MD5, règle des channels critiques | mêmes contrôles qu'en 1.1, sur un nœud à deux cartes (`test_node_*`) |
| 2.2 | Tables `SYSTEM` pour des cartes aux channels différents ; MD5 stable et next de la vue `SYSTEM` | deux cartes du nœud d'essai avec des `requires` différents restent liées |
| 2.3 | Négociation du mode sur les liens de carte à carte (même code qu'en 1.4) | une carte reflashée seule retombe en stable, la fonction candidate reste dormante, la sécurité continue |

Le temps 2 ne démarre que quand le temps 1 est validé en réel (télécommande vers machine).

## 4. Les types de dispositifs

- **Remorque** : un nœud à part entière (sa carte, son ComBus). Lien bidirectionnel d'égal à égal, avec handshake et un ensemble réduit.
- **Accessoire** (godet, outil) : simple, non décisionnel ; **diffusion seule**, pas de handshake, retour éventuel limité à un identifiant. C'est un module à activer sur la machine. Certains n'ont aucune communication : un `ComDev` sans port (protocole `NONE`) sert alors seulement à activer un `.cb` et à lier une commande à un channel.
- **Modules son et lumière** : des modules logiciels activés par `-D`, qui tournent sur toute carte désignée (une carte son et lumière peut devenir la carte mère d'une petite machine). C'était l'idée des thèmes des `.cb`. Le builder devra accepter le thème `sound`.
- Chaque type de machine déclare, par flags `-D`, la **liste des profils compatibles** (une pelleteuse n'a pas à charger une remorque de semi-remorque). Le builder en tire les tables.

## 5. Où vit la configuration

| Niveau | Contenu | Où |
|---|---|---|
| Machine (figé) | ports, devices, liens essentiels | `envCfg.cpp` de la carte |
| Modules optionnels | son, lumière : leurs ports, devices, `.cb`, mappages de commandes | un fichier par module, inclus par un flag `HAS_*` |
| Amovibles | un emplacement par remorque ou accessoire pris en charge | liste de profils compatibles (`-D`) |

La table de correspondance entrée vers channel (`inputs_map`) devra aussi devenir modulaire (fragments inclus par flag).

## 6. Plan progressif

1. RX : utiliser le `layer` du lien comme identité d'écriture, avec un test lien `SYSTEM` ; supprimer les deux accès du cœur au ComBus global (`combus_rx.cpp`, `vbat.cpp`).
2. Test Unity `test_combus_loopback` : adapter aux signatures actuelles.
3. Réparer le filtre par type de machine (préalable aux profils compatibles et à la seconde lecture des profils).
4. Config : `ComPort` / `ComDev` pour l'UART seul, en remplacement de `combus_uart_init.cpp` (comportement identique).
5. Brancher le RX : un appelant de `combus_rx_update()`, perte de lien par lien, état de lien et blocage, ACK de handshake.
6. Temps 1 du §3.7 : MD5 par ensemble, candidats, tables et tampons générés, filtrage par `direction`, `passthrough`, fusion `both_or`.
7. Télécommande (étape 1.6), puis l'adaptateur ESP-NOW (télécommande vers machine), la remorque et les accessoires.
8. Temps 2 du §3.7 : portée `SYSTEM`.

Chaque étape se vérifie avec le test de boucle (loopback) puis un build complet. Le détail des temps 1 et 2 (MD5 stable, candidats, tables, handshake) est en §3.7.

## 7. Questions ouvertes

- Format de la déclaration d'un ensemble « lite » et de l'étiquette d'égal à égal (champ des `.cb`, fichier dédié).
- Renégociation : déclenchée par la perte de lien, ou par un ACK périodique qui annonce un MD5 différent ; à préciser avec la cadence de l'ACK.
- Suffixe de candidat : quel nom, et comment le builder le contrôle.
- Points A du §8 encore ouverts : format exact du paramètre `passthrough` ; génération des espaces de noms et des instances multiples pour la télécommande ; cadence et seuil de l'ACK périodique.
- ESP-NOW : un `NodeCom` par pair avec tampon de réception par pair ; nombre maximal de pairs de l'ESP32 à vérifier dans la documentation ESP-IDF.
- Deux nœuds de même type en égal à égal : hors périmètre pour l'instant.
- Un module qui fait de sa carte la carte mère : quelles ressources minimales vérifier au démarrage.

## 8. Vérification de compatibilité avec le code (2026-10-18)

Passe de vérification du modèle contre le code actuel, pour détecter ce qui pourrait remettre en cause le principe. Classement : **A** = à trancher avant de coder (sinon le principe ne tient pas), **B** = à prévoir dans la mise en œuvre, **C** = sans conséquence sur le principe.

### A. À trancher avant de coder

1. **Le sens est attaché au channel, alors qu'un relais a besoin d'un sens par lien.** Exemple : le frein (`BRAKE`) part de la télécommande, passe par la machine et va jusqu'à la remorque. Un `.cb` ne porte qu'un `direction` par channel.
   **Réponse retenue (propriétaire) : un paramètre `passthrough` dans le `.cb`.** Chaque nœud déclare son sens de son point de vue (télécommande `uplink`, machine `downlink`, remorque `downlink`) ; la machine est déclarée `passthrough` pour ce channel : elle consomme la valeur reçue et la relaie aux autres nœuds connectés qui la reçoivent. Analyse :
   - **Rôle par lien calculé par le builder** : pour chaque lien de la machine, il compare le sens local et le sens du pair (connus par les tables). Pair émetteur : la machine reçoit sur ce lien. Pair récepteur et machine `passthrough` : la machine émet sur ce lien.
   - **Un seul channel local** : la machine utilise elle-même la valeur relayée ; pas de second channel ni de processeur de recopie.
   - **Une seule source** : un channel relayé a une seule origine. Un écrivain local en plus de la source est une erreur de configuration.
   - **Perte de la source** : si le lien d'origine est perdu, la valeur locale et relayée revient à sa **valeur par défaut sûre** (celle du `.cb`). Indispensable pour un frein.
   - **Jamais de retour vers l'origine** : une valeur n'est jamais renvoyée sur le lien d'où elle vient.
   - **`both_or` relayé** (failsafe partagé) : la valeur envoyée sur un lien est le OR du local et des **autres** liens, sans ce qui vient de ce lien. Sinon un `true` tourne en boucle et ne retombe jamais (verrouillage).
   - **Boucles** : avec la règle précédente, une chaîne ne boucle pas ; un anneau de plusieurs relais n'est pas pris en charge.
   - **Latence** : une période d'émission de plus par relais (20 ms à 50 Hz).
   - **Contrôles du builder** (suggestion du propriétaire) : deux récepteurs sur un lien sans `passthrough` : avertissement (« pas de source sur ce lien ») ; deux émetteurs : erreur, sauf variante `*_or` ; `passthrough` sans aucun récepteur en aval : avertissement.
   - **Portée** : channels `BASE` entre nœuds. À l'intérieur d'un nœud, la diffusion entre cartes (ND2) fait déjà ce travail.
2. **Un nom de channel est unique dans tout ce que le builder charge** (`ChannelConflictError`, `canon/channels.py:741-780`). **Réponse retenue : la règle « même nom des deux côtés » n'évolue que pour les nœuds, pas pour les cartes.** À l'intérieur d'un nœud, rien ne change : un seul jeu de channels, des noms uniques, la diffusion ND2. Entre nœuds, le builder fait une **seconde lecture** des profils compatibles, seulement pour calculer les tables, sans jamais les fusionner dans le ComBus local. Cela suppose un filtre par type qui fonctionne (chaque env ne charge que ses propres `.cb`).
3. **Une télécommande pilote plusieurs machines, donc plusieurs ComBus dans un même firmware.** Constats : l'env `remotes` est vide (`src/remotes/main.cpp`, 9 lignes), donc libre ; l'API du cœur prend déjà le ComBus en paramètre (`combus_set_analog(ComBus&, …)`) ; le protocole vise déjà un ComBus cible par lien (`ComBusLink.target`) ; mais le générateur n'émet qu'un ComBus par env, et l'émission prend un seul ComBus pour tous les liens (`combus_tx_update(const ComBus* bus)`).
   **Décisions du propriétaire** :
   - Les **deux accès du cœur au `comBus` global** sont à corriger (`combus_rx.cpp`, invalidation du handshake sur failsafe ; `vbat.cpp`) : le ComBus doit être passé en paramètre ou rattaché au lien.
   - **Pas de montage par pointeur.** Un **espace de noms par type de machine**, utilisé dans les fichiers qui mappent les entrées de la télécommande et les sorties.
   Analyse :
   - **Possible**, avec une limite : en C++, un espace de noms est choisi **à la compilation**. Une variable ne peut pas désigner un espace de noms à l'exécution. Chaque fichier de correspondance est écrit pour un type et utilise son espace de noms (par exemple `DumperTruck::AnalogComBusID::STEERING_BUS`, ou un alias `namespace M = DumperTruck;` en tête de fichier).
   - **Le choix « à la volée »** se fait par une donnée : un tableau de descripteurs, un par machine pilotée (nom, type, ComBus de cette machine, table des entrées de son type). La variable d'exécution est l'**indice** de la machine active dans ce tableau. Le code générique de la télécommande lit le descripteur actif ; seul le contenu des tables dépend du type.
   - **Répartition claire** : la portée globale reste le **ComBus propre du nœud** (pour la télécommande : sa batterie, son failsafe, ses entrées), comme aujourd'hui pour la machine ; les espaces de noms ne servent qu'aux **types pilotés**. Le code du cœur continue de travailler sur le ComBus du nœud.
   - Côté générateur : produire, pour l'env télécommande, un espace de noms par type accepté (énumérations et modèle de tableaux) et une instance de ComBus par machine pilotée.
4. **Le handshake ne répond pas** : c'était intentionnel, et sans effet tant que rien n'est bloqué. Le problème est l'**annonce** : un nœud qui démarre après son pair ne reçoit jamais le MD5 de ce pair, qui n'émet qu'à son propre démarrage. Avec le blocage du point 5, ce nœud resterait bloqué.
   **Décision du propriétaire : un accusé de réception (ACK) contenant le MD5, en réponse à une demande de connexion.**
   - Une **demande** est la salve de handshake actuelle ; la **réponse** est une trame de handshake marquée comme telle (un bit de l'octet `flags` de l'en-tête, aujourd'hui à zéro dans le handshake). Format de trame inchangé. On ne répond jamais à une réponse : pas de ping-pong.
   - Un nœud qui reçoit une demande répond par un ACK avec son MD5 ; les deux côtés connaissent alors le MD5 de l'autre et choisissent le même mode.
   - Pour les liens à diffusion seule (accessoires), pas d'ACK possible : salve périodique et espacée.
   **Extension proposée par le propriétaire : le même mécanisme, périodique, pour vérifier que la machine est toujours sous contrôle et connectée.** Analyse :
   - **Intérêt réel** : les trames de données prouvent que je reçois mon pair ; l'ACK périodique prouve que **mon pair me reçoit**. Pour une télécommande, c'est la preuve que la machine reçoit bien ses commandes, même si la machine n'envoie rien d'autre.
   - Il détecte aussi un changement de contrat en cours de session (pair reflashé).
   - **Coût faible** : une trame de 24 octets, à cadence lente (par exemple 1 Hz).
   - **Règle de prudence** : ne déclarer « hors contrôle » qu'après plusieurs ACK manqués (une liaison bruitée en perd). Côté machine, la sécurité reste fondée sur sa propre réception des commandes (`REMOTE_LINK_LOST`) ; l'ACK périodique sert surtout à la télécommande (affichage « connecté / sous contrôle », perte de la machine).
   - Rejoint le volet « timeout » du roadmap (relance pour un lien sans retour, accusé de réception et délai pour un lien avec retour).
5. **Blocage des données** : validé. Un **état de lien** (inconnu, stable, next) bloque l'application des trames reçues tant que le mode n'est pas choisi, et retarde l'émission (longueur de trame) sur les liens bidirectionnels.

### B. À prévoir dans la mise en œuvre

6. **Les candidats décalent les identifiants locaux.** Les identifiants sont triés par type, puis par portée, thème et nom ; un candidat `BASE` se place parmi les channels `BASE` stables, et les candidats `BASE` se placent entre les channels `BASE` et `SYSTEM`. Dès qu'un candidat existe, l'ordre sur le fil (stables puis candidats) n'est plus l'ordre local : les tables doivent être générales dès l'étape 1.3, même sur le lien par défaut.
7. **Le contexte de handshake ne garde qu'un MD5 attendu** (`CombusHandshakeContext::expectedMd5`). Il en faut deux (stable et next) et, pour la comparaison, seulement la version majeure (aujourd'hui majeure et mineure). La charge du handshake reste de 18 octets.
8. **Robustesse des tampons de réception.** Constats (`combus_rx.cpp`, `combus_frame.cpp`) :
   - le tampon circulaire (255 octets) ne déborde pas en mémoire : plein, il écrase l'octet le plus ancien ; on perd une trame, le décodeur se resynchronise sur le SOF et le CRC ;
   - le vrai point faible : les tampons de valeurs d'un lien (`analogBuf`, `digitalBuf`) sont passés **par pointeur seul**, à côté d'un nombre de channels déclaré à part (`rxCfg`). Rien ne vérifie que le tampon a bien la taille annoncée : un nombre déclaré plus grand que le tableau ferait écrire hors du tableau au décodage. Aujourd'hui le seul appelant utilise les mêmes constantes pour les deux, donc c'est juste par construction, mais rien ne l'impose ;
   - le décodeur refuse une trame dont le nombre d'analogiques dépasse le nombre déclaré, mais tronque en silence les numériques en trop.
   Propositions : (a) **lier le tampon et sa taille** dans un même type (par exemple une structure de tampons dont la taille est une constante du type), d'où le nombre déclaré est déduit, avec un `static_assert` à la compilation ; mieux, (b) **faire générer ces tampons par le builder**, par ensemble de lien, dimensionnés par ses constantes (`CH_COUNT`) : la taille ne s'écrit plus à la main ; (c) traiter les numériques en trop comme les analogiques (trame refusée et journalisée) au lieu de les tronquer. Dimensionnement : pour l'ensemble next (le plus grand).
9. **Champs et thèmes fermés dans le builder** : `ALLOWED_FIELDS` (`canon/channels.py:51-60`) doit accepter le statut candidat, l'étiquette d'égal à égal, `passthrough` et, si retenu, `critical` ; `VALID_THEMES` doit accepter `sound`.
10. **Garder la règle ND4 bis (pas de `requires` sur `BASE`).** C'est elle qui garde l'ensemble `BASE` d'un type identique pour toutes les machines de ce type, donc un seul MD5 par type. Conséquence : les `HAS_*` ne font jamais varier l'ensemble `BASE` ; les channels `BASE` liés à une remorque sont présents dans tout le type (inertes sans remorque), et les ajouter demande une version majeure ou le statut candidat. Cela répond à la question ouverte sur les variantes de profil.
11. **Le cœur lit le ComBus global à deux endroits** : `combus_rx_update()` consulte `comBus` et `DigitalComBusID::FAILSAFE` pour invalider le handshake (`combus_rx.cpp`, `#if defined(HAS_FAILSAFE)`), et `vbat.cpp` écrit `FAILSAFE_VBAT` dans `comBus`. Décision : à corriger (voir point 3).

### C. Sans conséquence sur le principe

12. **Taille des trames** : 255 octets au plus (longueur sur un octet), environ 35 aujourd'hui. La charge ESP-NOW est de l'ordre de 250 octets (à vérifier) : un ensemble devra rester sous cette limite.
13. **Le protocole est déjà multi-liens et indépendant du transport** : pools par lien, contexte de handshake par lien, `NodeCom`. Aucune refonte du codec n'est nécessaire.
14. **Docs** : `combus_frame_protocol.md` décrit correctement le comportement actuel (§8.4 : un `MISMATCH` ne bloque rien) ; `~combus_v2.md` (§ Métadonnées transport, priorité réseau) et `deferred_features.md` (§ 5, multiplexage de trames) restent compatibles avec le modèle ; le roadmap pointe vers cette étude.

### Conséquence sur le plan

Les points A1 à A5 et B8, B11 sont intégrés au plan (§6) et au détail des temps (§3.7).
