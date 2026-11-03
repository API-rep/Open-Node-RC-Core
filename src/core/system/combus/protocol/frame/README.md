# La trame de données

Ce dossier transforme l'état du ComBus en octets, et inversement. Le codec est pur : il ne connaît ni l'état du lien, ni sa perte, ni aucun channel en particulier. Le run level et le failsafe y voyagent comme des channels ordinaires.

## Le format

| Octet | Taille | Champ |
|---|---|---|
| 0 | 1 | `SOF` = `0xAA` |
| 1 | 1 | `nAnalog` : nombre de channels analogiques |
| 2 | 1 | `nDigital` : nombre de channels numériques |
| 3 | 1 | `seq` : compteur de trame |
| 4 | 1 | `flags` : 0 sur une trame de données |
| 5 | `ceil(nDigital / 8)` | channels numériques, 1 bit par channel, bit de poids faible d'abord |
| 5 + d | `2 × nAnalog` | channels analogiques, `uint16` petit-boutiste |
| fin | 1 | CRC-8/MAXIM sur tous les octets précédents |

- Les channels numériques passent **avant** les analogiques.
- Une trame fait 255 octets au plus, car sa longueur tient sur un octet.
- `seq` va de 1 à 255, puis revient à 1. La valeur 0 est réservée au [handshake](../handshake/README.md) et au [couplage](../coupling/README.md) : l'encodeur la refuse.

## Quels channels voyagent

Une position sur le fil doit désigner le même channel des deux côtés. Il y a deux façons de le garantir :

- **Sans table** : la position est l'identifiant du channel. La trame porte le début de la vue de la portée, car le builder range les identifiants pour que `BASE` soit le préfixe de `SYSTEM`.
- **Avec table** (`ComPeerLink`, un lien entre nœuds) : la table donne, pour chaque position, le channel local et dit si ce nœud le reçoit ou l'envoie.

Dans les deux cas, c'est le [handshake](../handshake/README.md) qui prouve que les deux bouts sont d'accord : ils comparent le MD5 de cet ensemble de channels.

## Le chemin d'une trame

```text
Émission    bus → combus_frame_encode() → octets → NodeCom

Réception   NodeCom → octets → combus_frame_decode() → instantané du lien
                                                          │
                                 combus_frame_apply()  ←──┘
                                          │
                                          ▼
                                         bus
```

**`combus_frame_decode()`** valide d'abord, et n'écrit qu'ensuite : `SOF`, tailles annoncées, capacité du lien, CRC. Une trame refusée laisse la sortie intacte. Le résultat reste indexé par position sur le fil.

**`combus_frame_apply()`** écrit au bus les seuls channels que ce nœud reçoit :

- Avec une table, c'est elle qui le dit.
- Sans table, c'est le sens du channel sur ce type de lien (`ChanFlow`).

Un channel que ce nœud produit n'est donc jamais écrasé par son pair. Les écritures passent par `combus_set_*()`, qui refuse un channel hors de la portée du lien : une trame n'écrit jamais un channel `BOARD`.

**Le cas `both_or`** : ces channels ne sont jamais écrits par une trame. La valeur reçue reste dans l'instantané du lien, et `com_rx_update()` la fusionne à chaque tour avec celle de la carte. Voir [la fusion `both_or`](../both_or.md).

## Les fichiers

| Fichier | Contenu |
|---|---|
| `combus_frame.h`, `.cpp` | Encodage, décodage, application au bus, CRC |
| `combus_frame_defs.h` | Les structures : disposition, en-tête, trame décodée |

## Aller plus loin

- [Le protocole ComBus](../README.md) : la vue d'ensemble
- [Le handshake](../handshake/README.md) : le contrat du lien
- [Le builder](../../../../../../scripts/combus_builder/README.md) : l'attribution des identifiants
