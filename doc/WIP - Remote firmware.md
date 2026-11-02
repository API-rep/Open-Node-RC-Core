# Firmware de la télécommande

Statut : **en cours de rédaction** (document `WIP`), ouvert le 2026-11-01. Sorti du chantier 4 du [ComBus link rework](<WIP - ComBus link rework.md>) : c'est un développement à part entière.
Public : le propriétaire et les futures sessions de travail sur la télécommande.
Plan : §1 pourquoi un WIP à part, §2 ce qui existe, §3 point bloquant connu, §4 ce qui reste à construire, §5 feuille de route, §6 questions ouvertes.

Vocabulaire : voir le glossaire de [doc/README.md](README.md#glossary) (nœud, machine, télécommande, coupleur, embase, fiche).

---

## 1. Pourquoi un WIP à part

Décidé le 2026-11-01 : sans firmware de télécommande (config, init, entrées), rien ne peut être essayé sur un véhicule. Continuer le multi-machine ou les entrées depuis le rework des liens reviendrait à coder dans le vide, sans pouvoir déboguer. Ce WIP prend donc la suite de l'embryon 4.0, sur sa propre branche, **après** le merge du rework des liens.

## 2. Ce qui existe (embryon 4.0, commit `3b284d9`)

| Élément | État | Où |
|---|---|---|
| Catégorie du nœud | `IS_REMOTE`, un coupleur y est refusé | `links.node_category()` |
| Type piloté | `-D MACHINE_TYPE_<TYPE>`, erreur sans lui ; `dumper_truck` pour l'instant | `generator._remote_view()` |
| ComBus | la vue BASE du type **en miroir** : ce que la machine reçoit (`downlink`, `downlink_pt`), la télécommande l'émet (`uplink`) ; ce que la machine émet, elle le reçoit (`downlink`, un retour) ; `both`, `both_or` inchangés ; ni les SYSTEM ni les BOARD du type | `links.remote_channels()`, `_remote_view()` |
| Contrat | le MD5 du lien, propre à ce lien (§3) ; chaque bout annonce son côté et attend l'autre | `links.compute_remote_link()`, `combus::peer::kRemoteLink` |
| Table des machines pilotées | nom, ComBus, lien ; une entrée | `src/remotes/config/driven_machines.h` |
| Lien | entre nœuds, sa propre table, sans appairage ; UART2, GPIO 17 et 16 | `kComDevMachine`, `kPortMachine` |
| Boucle | réception, surveillance, émission | `src/remotes/com/remote_links.*`, `main.cpp` |
| Env | `remotes` : filtre de sources explicite (protocole ComBus, accès, transport, journaux ; ni failsafe, ni batterie, ni runlevel), `INPUT_NONE` | `platformio.ini` |
| Preuves | pytest `test_remote_view.py` ; carte `test_remote_link` : miroir vu du C++, lien STABLE et vivant sur le strap (la télécommande s'entend elle-même, ce qui ne prouve pas l'accord avec une vraie machine) | |

## 3. Contrat du lien (point bloquant résolu le 2026-11-02, link rework 3.6)

**Le défaut relevé le 2026-11-01** : l'injection d'un coupleur actif change le MD5 `BASE` de la machine (`A8 A7 BC 04` devient `C3 17 65 32`), et la télécommande, qui comparait ce MD5, refusait le handshake.

**La décision** : sur le lien télécommande ne circule que ce que la télécommande envoie. Le lien a sa table et son MD5 (`ComPeerLink`, `kRemoteLink`), et la télécommande ne compare plus le MD5 `BASE`.

- `<type>.mtype` (dossier du type) déclare les coupleurs **supportés** (`couplers: socket, plug`). Ajouter un support est un changement cassant du type ; une instance (`HAS_COUPLER_*`) sans support est une erreur explicite.
- Le contenu du lien : les entrées du type, plus les commandes relayées (`downlink_pt`) par les coupleurs supportés, identique avec ou sans la remorque. Ces commandes existent sur la machine sans le coupleur, inertes.
- Jamais sur ce lien : ce qu'une fiche émet (`TILT`) et ce que la fiche du type reçoit.
- Les channels de lien ont des **noms génériques** partagés entre coupleurs (`TRAILER_RAMP`, un seul channel, pas d'octet de plus) ; les channels locaux restent préfixés (`FRONT_COUPLER_CMD`).
- Le builder refuse : un même nom en désaccord entre coupleurs (type, valeur, `zeroAt`), un channel envoyé par la télécommande et par une fiche, deux fiches sur le même channel.

**Reste ouvert** :

- **Qui commande une machine portée laissée autonome** (le trommel tracté, détaché) ? Le choix actuel est prudent : ce que sa fiche reçoit ne passe pas sur le lien télécommande. Un type qui n'a aucune autre commande n'a pas de lien télécommande (`test_trailer`). **Idée farfelue, plus ludique qu'utile pour l'instant, mais qui fonctionne sur le papier (2026-11-02) : l'hybride**, une part de remorque (la fiche, pour les fonctions du coupleur) et une part de machine (le lien télécommande, pour les fonctions autonomes), sans deux sources sur un même channel. Soit les deux parts cohabitent dans un firmware, soit il y a deux firmwares et deux cartes. Une fonction commune aux deux (les feux) passe par deux channels de noms distincts, réunis par une chaîne. Deux firmwares : une fiche au plus par nœud, donc la part machine porte la fiche d'un coupleur interne et la part remorque son embase ; le câble étant permanent, le lien n'a pas besoin d'appairage (`ALWAYS`).
- Le retour de la machine vers la télécommande (télémétrie) ne circule pas sur ce lien.
- Le MD5 `BASE` reste différent sur une machine à coupleur actif : sans effet sur la télécommande, mais la règle « un MD5 par type » ne vaut plus que pour la vue du type.

## 4. Ce qui reste à construire

- **Côté machine** : le lien « remote » (`kRemoteLink`, déjà généré) (un `ComDev` et un `ComDevMap` sur la carte de la Volvo), sans lequel aucun essai n'est possible. La PS4 reste active tant que ce n'est pas fait.
- **Config et init de la télécommande** : carte, broches, `sys_init`, journaux, tableau de bord (la vue « links » s'y greffera : le lien vers la machine y apparaîtra comme les autres).
- **Entrées physiques** (boutons, sticks) et leur mapping par type de machine. Le contrat bougera avec le [WIP Simulation chain](<WIP - Simulation chain.md>) : la télécommande enverra les sticks, la machine calculera les `*_BUS` ; aujourd'hui, la vue BASE du `dumper_truck` compte 13 channels, tous `downlink_pt`.
- **Plusieurs machines pilotées** (repris du link rework §2.7) :
  - un espace de noms par type accepté, généré pour l'env télécommande (les mêmes noms existent dans plusieurs types) ;
  - choix à l'exécution par la table des machines pilotées ; la variable d'exécution est l'indice de la machine active ;
  - une instance de ComBus par machine pilotée, rattachée à son lien, en réception comme en émission ;
  - la portée globale reste le ComBus propre du nœud (batterie, failsafe, entrées) ;
  - `combus_links_init()` et `combus_tx_update()` prennent aujourd'hui un seul ComBus : ils passeront à un ComBus par lien (un `static_assert` de `remote_links.cpp` le rappelle).
- **Sécurité** : le failsafe propre de la télécommande ; la **vraie perte du lien** (le bouton d'attelage reprend sa valeur sûre sans dételer, link rework 3.4e, prouvé seulement par simulation sur une carte).
- **La manette PS4 en lien ComBus** (le « nœud virtuel » du link rework §4) : vue de la machine, une manette PS4 ou un récepteur RC **est** une télécommande ; son backend remplit directement la trame d'un lien, appliquée par le même chemin qu'une trame reçue. Blocage complet si deux sources sont actives.
- **Transport radio** : ESP-NOW (link rework, chantier 5) ; une radio par config, le Bluetooth disparaît. **TODO** : le lien télécommande n'a pas d'appairage, donc pas d'appel à `NodeCom::setPeer` ; en radio, il lui faudra une identité de pair, sinon une autre télécommande pourrait piloter la machine.

## 5. Feuille de route (proposée, à valider)

| Étape | Livrable | Preuve |
|---|---|---|
| R0 | ~~Trancher le point bloquant du §3~~ fait (2026-11-02) | `test_remote_contract.py`, carte `test_remote_link` |
| R1 | Lien « remote » côté Volvo, config et init de la télécommande, carte réelle | deux cartes : handshake STABLE entre une vraie télécommande et une vraie machine |
| R2 | Entrées physiques, mapping par type | la télécommande pilote la Volvo sur l'établi |
| R3 | Failsafe de la télécommande, perte réelle du lien | la perte met la machine en attente et ne dételle pas |
| R4 | La PS4 en nœud virtuel | une seule source active, blocage sinon |
| R5 | Plusieurs machines (repris de l'ancienne étape 4.1 et 4.2) | une télécommande pilote deux machines |

## 6. Questions ouvertes

- Où définir un `ComDev` entre nœuds partagé par la machine et la télécommande, sans le dupliquer (repris du link rework §4).
- Bibliothèque de channels communs (« patterns », link rework §4) : à concevoir avant les entrées, dont elle serait la grande utilisatrice.
- Thème `remote` : seul `TEST_COUPLER_BTN` l'utilise ; les boutons et sticks existants y passeront avec le WIP Simulation chain.
- Carte matérielle de la télécommande (broches, écran éventuel).
