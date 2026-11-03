# Le couplage et l'appairage

Un lien qui passe par un coupleur n'existe que lorsque deux éléments sont attelés (`LinkActivation::ON_COUPLING`). Quand plusieurs engins sont à portée, reste à savoir **qui est attelé à qui**. Ce dossier répond à cette question avant d'ouvrir le lien.

Ce document décrit la mécanique. Pour le principe (coupleur, embase, fiche, ce qui se passe à l'attelage), commencer par [Relier les nœuds](../../../../../../doc/~combus_v2.md#relier-les-nœuds).

Vocabulaire : l'**embase** (`SOCKET`) est le côté qui porte, la **fiche** (`PLUG`) le côté porté.

## L'idée

Le déclencheur du coupleur, par exemple un capteur d'attelage, monte des deux côtés au même moment. Chaque bout mesure donc l'**âge de son attelage**, et deux engins vraiment attelés annoncent presque le même. L'appairage repose sur cet accord : l'embase pose la question plusieurs fois, et seules les réponses dont l'âge concorde comptent.

## La trame de couplage

Elle est à part du handshake : elle ne rend pas un lien vivant et ne touche pas au contrat. Longue de 19 octets, elle occupe l'emplacement `seq == 0`, repérée par le bit de couplage de `flags` :

| Octet | Taille | Champ |
|---|---|---|
| 0 | 1 | `SOF` = `0xAA` |
| 1, 2 | 2 | `nAnalog` = 0, `nDigital` = 0 |
| 3 | 1 | `seq` = 0 |
| 4 | 1 | `flags` : bit 2 = couplage |
| 5 | 1 | `msg` : 1 = requête, 2 = réponse |
| 6 | 1 | `round` : numéro du tour |
| 7 | 1 | `addr.type` : 0 = aucune, 1 = MAC48 |
| 8 à 13 | 6 | `addr.bytes` : l'adresse (MAC de l'ESP32) |
| 14, 15 | 2 | `hitchAgeMs` : âge de l'attelage en ms, saturé à 65 535, petit-boutiste |
| 16, 17 | 2 | `features` : masque des fonctions, réservé, à 0 pour l'instant |
| 18 | 1 | CRC-8 sur les octets précédents |

Un firmware qui ne connaît pas cette trame la prend pour un handshake de mauvaise longueur : le CRC échoue, il jette un octet et se resynchronise, sans toucher au contrat.

## L'appairage

```text
         déclencheur vrai pendant engageMs
   DETACHED ───────────────────▶ PAIRING
       ▲                           │
       │                           ├── un seul candidat, assez de tours ──▶ COUPLED
       │                           ├── deux candidats ───────────────────▶ CONFLICT
       │                           └── pas assez de réponses (embase) ───▶ FAILED
       │
       └── déclencheur faux pendant releaseMs, depuis n'importe quel état
```

| État | Le lien |
|---|---|
| `DETACHED` | est coupé |
| `PAIRING` | ne décode que les trames de couplage, tout autre octet est jeté |
| `COUPLED` | fonctionne normalement |
| `CONFLICT` | reste coupé jusqu'à ce que le déclencheur retombe puis remonte |
| `FAILED` | reste coupé ; l'embase retente tant que le déclencheur reste vrai |

**Côté embase** : elle envoie 5 requêtes, espacées de 75 à 125 ms, puis écoute une dernière période de 100 ms. Une réponse compte si son âge concorde, à `ageToleranceMs` près.

- Aucune identité, ou moins de 4 tours pour la seule entendue : `FAILED`, retenté 3 fois toutes les 2 s.
- Deux identités : `CONFLICT`, jamais retenté.
- Une seule identité sur au moins 4 tours : `COUPLED`.

**Côté fiche** : elle écoute et répond à chaque requête dont l'âge concorde, après un délai tiré entre 0 et 10 ms pour que deux fiches ne se répondent pas par-dessus.

- Une seule embase sur au moins 4 requêtes : `COUPLED`.
- Deux embases : `CONFLICT`, et elle ne répond plus.
- Moins de requêtes : elle se remet à écouter.

Un déclencheur qui retombe moins longtemps que `releaseMs` est seulement journalisé : un faux contact ne dételle pas.

## Une fois couplé

L'adresse du pair est gardée et transmise au transport (`NodeCom::setPeer`). Les trames de couplage venant d'une autre identité sont comptées. Sur le fil, cette adresse n'a pas d'effet ; elle servira à filtrer les émetteurs avec ESP-NOW.

Le lien démarre alors comme les autres : [handshake](../handshake/README.md), puis données.

## Le coupleur passif

Un coupleur sans nœud de l'autre côté, comme un outil, n'a personne à qui parler. Son déclencheur est son détecteur de présence : `COUPLED` une fois celui-ci stabilisé, sans appairage (`combus_pairing_update_passive()`).

## Les fichiers

| Fichier | Contenu |
|---|---|
| `combus_coupling.h`, `.cpp` | La trame de couplage : encoder, décoder, transmettre à l'appairage |
| `combus_pairing.h`, `.cpp` | La machine à états de chaque bout, pilotée par `combus_links` |

La trame ne dépend pas de l'appairage : celui-ci s'enregistre auprès d'elle pour recevoir les trames.

## Aller plus loin

- [Le protocole ComBus](../README.md) : la vue d'ensemble
- [Le handshake](../handshake/README.md) : ce qui suit l'appairage
