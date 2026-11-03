# Protocole ComBus

Ce dossier fait voyager les channels du ComBus entre deux cartes ou deux nœuds, sans rien savoir du support physique. Il répond à trois questions : que met-on sur le fil, les deux bouts parlent-ils la même langue, et le lien est-il vivant ?

## Vue d'ensemble

```text
Modules de la machine
        │  lisent et écrivent les channels
        ▼
      ComBus
        │
        ▼
    protocol/        décrit les liens, émet, reçoit, surveille
        │
        ▼
     NodeCom         interface d'écriture et de lecture
        │
        ├── UART
        └── ESP-NOW (à venir)
```

## Qui fait quoi

| Élément | Rôle |
|---|---|
| [`frame/`](frame/README.md) | La trame de données : encoder, décoder, appliquer au bus |
| [`handshake/`](handshake/README.md) | Le contrat du lien : les deux bouts ont-ils le même MD5 ? |
| [`coupling/`](coupling/README.md) | L'appairage : qui est attelé à qui ? |
| `combus_tx.*` | L'émission de chaque lien, à son rythme |
| `combus_rx.*` | La réception de chaque lien : aiguille les octets vers le bon décodeur |
| `combus_links.*` | Construit les liens à partir de leur description, puis les surveille |
| `combus_protocol.*` | Les structures qui décrivent un lien, et son initialisation |
| `combus_peer_link_defs.h` | Les types d'un lien entre nœuds : table et contrat, remplis par le builder |

## Décrire un lien

Un lien se déclare, il ne se programme pas :

- `ComDev` décrit le lien : sa portée, son sens, son activation, son rythme.
- `ComPort` décrit le port physique de la carte.
- `ComDevMap` branche un `ComDev` sur un `ComPort`, et dit le rôle de ce bout (`TX`, `RX` ou `TX_RX`).

Au démarrage, `combus_links_init()` valide chaque `ComDevMap`, ouvre le port, qui donne un `NodeCom`, puis démarre le lien. Tout le stockage appartient à l'appelant et reste statique : rien n'est alloué.

## Les deux portées

| Portée du lien | Relie | Channels transportés |
|---|---|---|
| `INTER_NODE` | deux nœuds (télécommande, remorque…) | `BASE` |
| `INTRA_NODE` | les cartes d'un même nœud | `SYSTEM`, qui inclut `BASE` |

Les channels `BOARD` restent sur leur carte, ils ne voyagent jamais.

Un lien entre nœuds peut aussi avoir **sa propre table** (`ComPeerLink`) : c'est le cas d'un lien à travers un coupleur et du lien avec la télécommande. Il transporte alors exactement les channels de cette table.

## Un tour de boucle

1. **Réception** : `combus_rx_update()` lit les octets et les aiguille vers le bon décodeur.
2. **Surveillance** : `combus_links_update()` suit le couplage et détecte la perte des liens. Une perte est publiée dans `NODE_LINK_FAIL` ou `BOARD_LINK_FAIL`, selon la portée.
3. **Traitements** de la machine.
4. **Émission** : `combus_tx_update()` envoie les handshakes dus, puis une trame de données si c'est l'heure.

## Reconnaître une trame

Toutes les trames commencent par le même en-tête. Deux octets suffisent à les distinguer :

| `seq` | `flags` | Trame | Traitée par |
|---|---|---|---|
| 1 à 255 | 0 | données | `frame/` |
| 0 | bit de couplage levé | couplage | `coupling/` |
| 0 | autre | handshake | `handshake/` |

## Les règles

- **Indépendance du transport** : le protocole ne parle qu'à `NodeCom`, jamais à `Serial` ou à un autre périphérique.
- **Un port, un propriétaire** : ouvrir un port déjà pris est une erreur de configuration, signalée et refusée.
- **Une trame invalide ne coûte qu'un octet** : le récepteur jette ce premier octet et cherche le début de la trame suivante, sans en avaler une bonne.

## Ajouter un transport

Il suffit d'un adaptateur qui fournit un `NodeCom` : `write()`, `readByte()`, `available()`. Le reste du protocole ne change pas.

## Aller plus loin

- [La trame de données](frame/README.md)
- [Le handshake](handshake/README.md)
- [Le couplage et l'appairage](coupling/README.md)
- [La fusion `both_or`](both_or.md) : l'anti ping-pong entre cartes
- [Le workflow UART](uart_workflow.md) : la chaîne d'initialisation, du flag de build à `loop()`
- [Le builder](../../../../../scripts/combus_builder/README.md) : comment les identifiants et les tables sont générés
