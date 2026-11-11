# WIP - Node transports

Statut : **à ouvrir** (document `WIP`), créé le 2026-11-11 à la clôture du ComBus link rework, dont il reprend les chantiers 5 et 6 et l'analyse « plusieurs pairs par lien ». Rien n'est implémenté de ce document.
Public : le propriétaire et les futures sessions de travail sur les transports entre nœuds et entre cartes.
Plan : §1 objectif, §2 ESP-NOW (premier transport traité), §3 plusieurs pairs sur un lien, §4 autres transports, §5 contrat `SYSTEM` entre cartes, §6 questions ouvertes.

Vocabulaire : voir le glossaire de [doc/README.md](README.md#glossary). Fonctionnement actuel des liens : [protocol/link](../src/core/system/combus/protocol/link/README.md), [handshake](../src/core/system/combus/protocol/handshake/README.md), [coupling](../src/core/system/combus/protocol/coupling/README.md).

---

## 1. Objectif

Le protocole ComBus est indépendant du transport : un lien n'échange des octets que par un `NodeCom`, et seul l'UART a un adaptateur aujourd'hui. Ce chantier ajoute les autres transports, en commençant par ESP-NOW (télécommande et remorque sans fil), sans toucher au codec ni au contrat des liens.

## 2. ESP-NOW (premier transport)

| Étape | Livrable | Preuve |
|---|---|---|
| 2.1 | Adaptateur `NodeCom` ESP-NOW, un tampon de réception par pair (l'adresse MAC de l'émetteur est connue à la réception) | une trame ComBus fait l'aller-retour entre deux ESP32 |
| 2.2 | `NodeCom::setPeer` : ne délivrer que les trames du pair appairé. Le cœur l'appelle déjà à l'appairage ; reste à le prouver à deux nœuds (`TODO(chantier 5, ESP-NOW)` de `test_peer_link`) | les trames d'un troisième nœud sont ignorées |
| 2.3 | `ComDev` globaux fournis par le cœur (modèles ESP-NOW, radio, UART…), paramétrés par `-D` : deux nœuds compilés avec les mêmes flags partagent la même définition. `ComPort` sans broches (contrôleur interne) ou avec broches (module radio) | télécommande vers machine en ESP-NOW |
| 2.4 | Remorque et accessoires sur ESP-NOW (coupleurs actifs et passifs) ; filtrage des trames par identité | attelage et accessoire d'essai |

**Lien à diffusion seule** (accessoire qui ne fait qu'émettre) : pas d'ACK, salve de handshake périodique et espacée ; le mode est déclaré dans le `ComDev`.

## 3. Plusieurs pairs sur un lien

Le contrat, l'ACK, la perte et le mode next / stable sont aujourd'hui rangés dans le contexte du lien : **deux pairs par lien**. Principe retenu pour la suite : **un fil, un émetteur, un ou plusieurs récepteurs**. Au niveau ComBus, chaque flux reçu vient d'un seul émetteur ; c'est le transport qui démultiplexe (un `NodeCom` par émetteur) : un lien reçu = un pair = un contexte.

| Transport | Le principe tient | Comment |
|---|---|---|
| UART | oui | un TX vers un ou plusieurs RX ; chaque retour est un fil à part ; RS485 multi-émetteur exclu, sauf adressage ajouté par le transport |
| CAN | oui | l'identifiant CAN dit qui émet ; arbitrage du bus ; trames ComBus découpées en trames CAN de 8 octets par le transport (à la ISO-TP) |
| ESP-NOW | oui | adresse MAC de l'émetteur à la réception : un flux par pair ; émission en diffusion ou adressée |
| I2C | oui, avec réserve | le maître écrit vers chaque esclave et lit chaque esclave ; un esclave ne parle que lu : le maître doit lire chaque esclave au moins toutes les 250 ms ; deux esclaves ne se parlent pas directement |

Réglé par le principe : identité de l'émetteur, contrat / ACK / silence par pair, collisions au démarrage, ensembles différents sur un même médium (deux bouts TX logiques), et en grande partie la liste des pairs attendus (chaque pair attendu est un lien configuré ; les pairs dynamiques, comme l'appairage ESP-NOW d'une télécommande, demandent un cache des pairs).

**Reste à traiter au niveau ComBus quand un TX servira plusieurs récepteurs :**

1. **Décisions d'émission agrégées** : un bout TX doit être relié aux liens RX de ses récepteurs. Émettre des données dès qu'**au moins un** récepteur est `STABLE` (pas de blocage pour tous parce qu'un nouveau pair arrive) ; émettre en next si au moins un récepteur est en next (sans danger : préfixe stable, les autres coupent). Aujourd'hui un bout bidirectionnel partage un seul contexte entre TX et RX : juste pour un seul récepteur.
2. **`both_or` sur une émission en diffusion** : envoyer « valeur de la carte OU autres liens » renverrait à un récepteur sa propre valeur (écho, ping-pong). Règle : sur une émission vers plusieurs récepteurs, `both_or` n'envoie que la valeur de la carte (suffisant si tous s'entendent : bus, diffusion) ; le relais passe par des émissions adressées ou le passthrough `_pt`.
3. **Deux producteurs du même channel** sur deux liens : le dernier gagne ; relève du filtrage par sens et du contrôle des producteurs par le builder.
4. **Nombre d'ACK** sur un médium partagé : N × 4 demandes/s × (N−1) ACK ; acceptable pour quelques pairs ; piste : une « liste des pairs entendus » dans la demande périodique au lieu des ACK.
5. **Champs « pair » du contexte** : contrat, dernier ACK, MD5 next annoncé, mode, refus journalisé, données depuis `STABLE` deviendront un état par lien RX (une structure par pair) ; le reste (salve, demandes) reste côté TX.

## 4. Autres transports

| Étape | Livrable | Preuve |
|---|---|---|
| 4.1 | Ports I2C et CAN (autres variantes de `ComPort`) | un port I2C ou CAN s'ouvre depuis la table de la carte (`comLinks[]`) |

## 5. Contrat `SYSTEM` entre cartes

Le contrat `BASE` (entre nœuds) a ses deux MD5 stable et next et sa négociation de mode. Entre les cartes d'un même nœud, le mécanisme existe mais n'est pas exploité : une carte reflashée seule (par exemple une carte son) doit pouvoir retomber en stable sans casser la sécurité.

| Étape | Livrable | Preuve |
|---|---|---|
| 5.1 | Ensemble stable `SYSTEM` par nœud et son MD5 | contrôles du contrat sur un nœud à deux cartes (`test_node_*`) |
| 5.2 | Tables pour des cartes aux channels différents ; MD5 stable et next de la vue `SYSTEM` | deux cartes avec des `requires` différents restent liées |
| 5.3 | Négociation du mode entre cartes (même code que pour `BASE`) | une carte reflashée seule retombe en stable, la sécurité continue |

## 6. Questions ouvertes

- ESP-NOW : nombre maximal de pairs de l'ESP32, charge utile exacte (documentation ESP-IDF).
- **Identité du pair** : un pair au même MD5 branché sur le mauvais lien (même type de carte) n'est pas détecté par le contrat ; à traiter avec le cache des pairs (« friend cache »).
- Nom du jeu de `ComDev` fournis par le cœur (« catalogue » est provisoire), et forme de leur paramétrage par `-D` (flags par modèle, ou une table) ; à terme, ces choix passeront par les fichiers de configuration YAML.
