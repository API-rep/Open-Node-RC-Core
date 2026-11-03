# Le handshake

Deux firmwares flashés séparément peuvent ne pas avoir le même ComBus. Si l'un croit que la position 3 est le frein et l'autre la direction, la machine fait n'importe quoi. Le handshake le détecte avant le moindre mouvement : **tant que les deux bouts ne sont pas d'accord, aucune donnée n'est appliquée.**

C'est un contrôle de cohérence, pas une mesure de sécurité.

## Le contrat

Le contrat d'un lien est le MD5 de l'ensemble des channels qu'il transporte, calculé par le builder.

| Lien | MD5 comparé |
|---|---|
| portée `BASE` | celui de la vue `BASE` |
| portée `SYSTEM` | celui de la vue `SYSTEM` |
| avec une table (`ComPeerLink`) | celui du lien, propre à chaque côté |

Sur un lien avec table, chaque bout annonce le MD5 de son côté et attend celui de l'autre. Deux embases, ou deux fiches, reliées par erreur se refusent donc.

La version majeure du projet fait aussi partie du contrat. La mineure et la version de développement sont seulement journalisées.

## La trame

Longue de 25 octets, elle occupe l'emplacement réservé `seq == 0` :

| Octet | Taille | Champ |
|---|---|---|
| 0 | 1 | `SOF` = `0xAA` |
| 1, 2 | 2 | `nAnalog` = 0, `nDigital` = 0 |
| 3 | 1 | `seq` = 0 |
| 4 | 1 | `flags` : bit 0 = ACK, sinon demande ; bit 1 = NEXT, sinon legacy |
| 5 à 20 | 16 | MD5 annoncé |
| 21, 22, 23 | 3 | version du projet : majeure, mineure, développement |
| 24 | 1 | CRC-8/MAXIM sur les octets précédents |

## Le déroulé

Un lien démarre toujours `INCONNU`. Chaque bout envoie une **rafale** de 3 demandes à 100 ms d'intervalle, puis une demande toutes les 250 ms, quel que soit l'état du contrat.

```text
Bout A                                 Bout B
  │ ── demande (MD5 de A) ───────────▶ │  arme un ACK, d'accord ou non
  │ ◀─────────────── ACK (MD5 de B) ── │
  │  compare : A passe STABLE          │
  │                                    │
  │ ◀─────────── demande (MD5 de B) ── │
  │ ── ACK (MD5 de A) ───────────────▶ │
  │                                    │  compare : B passe STABLE
```

Pour passer `STABLE`, chacun a besoin d'une preuve :

- **Un bout bidirectionnel** attend l'ACK de sa propre demande : il sait alors que l'autre le reçoit.
- **Un bout qui ne fait que recevoir** se contente de la demande de l'émetteur.

Un bout qui ne fait qu'émettre n'attend rien : ses demandes périodiques permettent à un récepteur démarré plus tard de s'établir.

## Les états

| | `INCONNU` | `STABLE` |
|---|---|---|
| Trames de données reçues | ignorées | appliquées |
| Le lien compte comme vivant | non | oui, s'il reçoit |
| Un bout bidirectionnel émet des données | non | oui |

Un bout qui ne fait qu'émettre envoie toujours ses données.

Le contrat retombe à `INCONNU` sur :

- un handshake refusé : MD5 et version majeure sont comparés à chaque fois, pas seulement au début ;
- un silence plus long que le délai du lien, après des données ;
- sur un bout bidirectionnel, aucun ACK pendant 1 s, soit 4 demandes : l'autre ne nous reçoit plus ;
- une réinitialisation du transport.

Le lien n'étant alors plus vivant, sa perte est publiée selon sa portée (`NODE_LINK_FAIL` ou `BOARD_LINK_FAIL`).

## Les channels candidats : stable, next, legacy

Un channel candidat est un channel ajouté à l'essai, derrière les channels stables. Il permet à deux firmwares de versions voisines de se parler. Le MD5 *stable* ne le compte pas, le MD5 *next* le compte. Ces modes ne concernent que les liens `BASE` : un lien `SYSTEM` transporte ses candidats dans son unique ensemble.

| Mode | Quand | Channels transmis |
|---|---|---|
| **next** | les deux bouts ont les mêmes candidats | tous |
| **stable** | les candidats diffèrent | les stables seulement ; les candidats gardent leur valeur par défaut |
| **legacy** | ce bout n'a aucun candidat | tous : il n'y a que des stables |

Comment ils se mettent d'accord :

- Un bout avec candidats commence par des demandes NEXT : la rafale, puis 3 relances. Il passe ensuite aux demandes legacy, ou dès qu'un ACK NEXT n'est pas conforme.
- Un handshake NEXT décide **du mode, jamais du contrat** : il ne peut pas faire tomber un lien.
- Le mode suit ce que l'autre annonce, sans couper le lien. Une perte ramène au mode stable et réarme les demandes NEXT.

Les candidats suivent toujours les stables sur le fil : une trame next lue en mode stable reste juste sur sa partie stable.

## Les interrupteurs de compilation

Réservés à la mise au point, ils sont à 0 par défaut :

| Macro | Effet |
|---|---|
| `COMBUS_MD5_CHECK_DISABLE` | Accepte tout handshake. Une bannière très visible le signale au démarrage. |
| `COMBUS_HANDSHAKE_BURST_DISABLE` | Supprime la rafale ; les demandes périodiques restent. |

## Les fichiers

| Fichier | Contenu |
|---|---|
| `combus_handshake.h`, `.cpp` | Constantes, contexte par lien, aides communes |
| `combus_handshake_rx.h`, `.cpp` | Décoder une trame, comparer, faire évoluer le contrat |
| `combus_handshake_tx.h`, `.cpp` | La rafale, les demandes périodiques et les ACK |

## Aller plus loin

- [Le protocole ComBus](../README.md) : la vue d'ensemble
- [La trame de données](../frame/README.md) : ce que le contrat protège
- [Le couplage et l'appairage](../coupling/README.md) : l'autre trame de l'emplacement `seq == 0`
