# Les liens

Ce dossier répond à trois questions : comment décrit-on un lien, comment démarre-t-il, et comment sait-on qu'il est perdu ?

## Décrire un lien

Un lien se déclare dans la configuration, il ne se programme pas :

| Structure | Rôle | Où |
|---|---|---|
| `ComPort` | le port physique d'une carte | `core/system/hw/node_com.h` |
| `ComProtocolCfg` | débit, rythme des trames, délai de perte ; identiques aux deux bouts | `combus_link_defs.h` |
| `ComDev` | le lien logique : sa portée, son type, son activation | `combus_link_defs.h` |
| `ComDevMap` | le câblage d'un bout : un `ComDev` sur un `ComPort`, en `TX`, `RX` ou `TX_RX` | `combus_link_defs.h` |
| `ComPeerLink` | la table d'un lien entre nœuds, générée par le builder | `combus_peer_link_defs.h` |

Un même `ComDev` est partagé par les deux bouts ; chacun le câble sur son propre port.

## Démarrer

`combus_links_init()` part de la table des `ComDevMap` d'une carte :

```text
Pour chaque ComDevMap :
    combus_link_check()    refuse une mauvaise description, avant de prendre le port
    ouverture du port      donne un NodeCom
    combus_link_build()    remplit le ComBusLink

Enregistrement des états TX et RX
combus_link_start()        contrat du lien, puis côtés TX et RX, pour chaque lien
combus_links_watch()       met les liens sous surveillance
```

Un lien en échec reste inactif et est signalé dans le journal ; les autres démarrent quand même. Tout le stockage appartient à l'appelant et reste statique.

`combus_link_check()` et `combus_link_build()` ne touchent pas au matériel : on peut les tester sur la carte sans aucun câblage.

## Surveiller

`combus_links_update()` tourne à chaque tour de boucle. Un lien qui reçoit a trois états :

| État | Sens |
|---|---|
| `NEVER_SEEN` | aucune trame depuis le début de la surveillance |
| `ALIVE` | une trame récente, sous un contrat valide |
| `LOST` | vu, puis silencieux plus longtemps que `linkTimeoutMs` |

### Quand un lien compte comme perdu

Cela dépend de sa portée, et le résultat alimente un contributeur du failsafe. C'est la chaîne qui le lit qui décide de la gravité.

| Lien | Compte comme perdu | Contributeur |
|---|---|---|
| entre cartes | `LOST`, ou jamais vu une fois le délai de démarrage écoulé : les cartes démarrent ensemble | `BOARD_LINK_FAIL` |
| vers un autre nœud | pas `ALIVE`, sans délai : l'autre nœud peut ne pas être là | `NODE_LINK_FAIL` |
| par coupleur | voir [le couplage](../coupling/README.md) | celui de son `CouplingDev` |

Un lien qui ne fait qu'émettre ne compte jamais : il ne peut rien savoir de l'autre bout.

### Les valeurs sûres

Quand un lien avec table cesse d'être vivant, ce que ce nœud recevait par ce lien reprend sa valeur sûre : `CbusNeutral` pour un analogique, `false` pour un numérique. Ce que le nœud envoie ne bouge pas, et un `both_or` n'est pas concerné, car une trame ne l'écrit jamais. Un lien sans table ne change rien.

## Les fichiers

| Fichier | Contenu |
|---|---|
| `combus_link_defs.h` | Les structures de description, et `ComBusLink` |
| `combus_peer_link_defs.h` | Les types d'une table de lien |
| `combus_links.h`, `.cpp` | Valider, construire, démarrer ; le stockage de chaque lien |
| `combus_link_watch.h`, `.cpp` | État, perte, valeurs sûres, vues du tableau de bord |

## Aller plus loin

- [Le protocole ComBus](../README.md) : la vue d'ensemble
- [Le couplage et l'appairage](../coupling/README.md) : les liens qui n'existent qu'une fois attelés
- [Le handshake](../handshake/README.md) : le contrat d'un lien
- [Le workflow UART](uart_workflow.md) : la chaîne d'initialisation, du flag de build à `loop()`
