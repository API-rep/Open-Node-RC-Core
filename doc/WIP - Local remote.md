# WIP - Local remote

Statut : **en conception** (document `WIP`), créé le 2026-11-11 à la clôture du ComBus link rework (idée retenue le 2026-10-25). Concept revu et décidé le 2026-11-21 : **intercalé avant l'audit final du link rework**, pour que l'audit couvre aussi les manettes. Rien n'est encore implémenté.
Public : le propriétaire et les sessions de travail sur les sources locales de commandes.
Plan : §1 constat, §2 décisions, §3 les trois couches, §4 tables de correspondance, §5 contrat, §6 driver et boucle, §7 configuration, §8 étapes et preuves, §9 questions ouvertes.

Documents liés : [WIP - Remote firmware](<WIP - Remote firmware.md>) (la télécommande, un nœud à part entière) ; [glossaire](glossary.md).

---

## 1. Constat

Une machine reçoit ses commandes par deux chemins différents :

- d'une **télécommande** (un nœud) : un lien ComBus, avec son contrat (MD5), sa table (`kRemoteLink`), son filtrage par sens et sa perte de lien (`NODE_LINK_FAIL`) ;
- d'une **source locale** branchée sur la machine (aujourd'hui la manette PS4 en Bluetooth, demain un récepteur RC PPM / SBUS) : un backend d'entrée (`input_refresh()`), mappé vers le ComBus par `inputs_map` (`input_update()`), avec un contributeur de santé à part (`LOCAL_REMOTE_FAIL`).

Vu de la machine, une manette PS4 **est** une télécommande : deux chemins pour la même chose.

## 2. Décisions (2026-11-21)

| Question | Décision | Écarté, et pourquoi |
|---|---|---|
| Comment la source entre dans la transmission | un **`ComPort` virtuel** et son driver : la source produit de vraies trames ComBus, lues par le RX comme sur un fil ; le TX lui écrit le retour | injecter les positions dans le snapshot du RX : un chemin parallèle (sans anneau, décodeur ni CRC) à maintenir à part, et rien pour le TX |
| Le contrat | **lien de confiance** : STABLE dès le départ, aucune poignée de main, jamais de chute du contrat ; la perte passe par l'âge et le superviseur | imiter la poignée de main de la télécommande : un MD5 forcément égal (même firmware, même table), de la complexité pour rien |
| La table du lien | celle de la vraie télécommande (`kRemoteLink`) : manette et télécommande sont interchangeables | |
| Le retour vers la manette (vibration, barre lumineuse) | le driver le décode dès le départ ; **la table du retour reste vide** tant que la machine ne renvoie rien sur ce lien (retour de santé, Remote firmware §4) | |
| La santé | `LOCAL_REMOTE_FAIL` **retiré** : la perte du lien local lève `NODE_LINK_FAIL`, avec les valeurs sûres de la table | |
| Deux sources | **plus d'un lien vivant sur `kRemoteLink`** (télécommande et manette, ou deux manettes) : la machine bloque, lève `NODE_LINK_FAIL` et logue la cause ; règle générique, sans cas propre à la PS4 | |
| Les capteurs ordinaires | pas concernés : channels `SYSTEM` locaux, sans contrat ni santé de lien | |

## 3. Les trois couches

| Couche | Rôle | Propre à |
|---|---|---|
| **Backend du modèle** | lire sticks et boutons, dire si la manette est connectée, piloter ses actionneurs | chaque modèle (PS4, SBUS, PPM...) |
| **Tables de correspondance** | entrées → channels ; channels → actionneurs (§4) | chaque type de machine × modèle, puis l'instance et l'utilisateur |
| **Driver du port virtuel** | encoder les trames, décoder le retour, tenir le rythme | rien : un seul code |

Le backend existe déjà (`PS4_dualshock.cpp`, son descripteur `inputDev` : min, max et type de chaque axe). Il reste fin : le Bluetooth doit disparaître (une radio par config).

## 4. Tables de correspondance : une configuration personnalisable

La table est une **configuration**, en trois niveaux. Décidé le 2026-11-21 : **pas de surcouche ni de fusion**. Un niveau qui personnalise réécrit le fichier complet, qui remplace celui du niveau précédent ; une surcouche ligne à ligne deviendrait vite illisible.

### 4.1 Les niveaux

| Niveau | Où | Ce qu'il fait |
|---|---|---|
| **Type de machine** (défaut) | `core/config/machines/<type>/inputs/<modèle>_map.*` (comme aujourd'hui) | la table complète ; écrite avec les channels du `combus_set` du type, elle vaut pour toutes ses instances |
| **Instance de machine** | `machines/config/machines/<instance>/inputs/` | facultatif : la table complète, qui remplace celle du type |
| **Utilisateur** | plus tard : fichier de config (builder YAML, Master §3), puis en direct (§4.3) | même format, la table complète |

Format d'une ligne, celui d'aujourd'hui : `{ entrée de la manette, channel, inversion }`. Une seule table est retenue, celle du niveau le plus personnalisé qui en déclare une.

La table reste **un simple aiguillage** (indices, inversion, mise à l'échelle par le min et le max du modèle) : courbes, expo et conditionnement appartiennent aux processors de la machine ([Simulation chain](<WIP - Simulation chain.md>)).

La table du retour (channels → actionneurs) suit les mêmes niveaux ; vide pour l'instant (§2).

### 4.2 Contrôles, au boot

Le driver copie la table retenue dans une **table en RAM**, puis la traduit en positions sur le fil (`kRemoteLink.analog.ids`, `.digital.ids`). Il refuse le lien, avec la cause au log, si :

- un channel est absent du lien, ou la machine ne le reçoit pas (`rx[]` à false) ;
- deux channels sortent de la même entrée, ou un channel apparaît deux fois.

Contrôle au boot, pas de `static_assert` (même choix que la charge de ligne). Une position du lien sans entrée part à la valeur de repos de son channel.

### 4.3 Modification à chaud : hors de ce chantier, la partie statique conçue pour

La modification à chaud n'est pas construite ici. La partie statique est faite pour l'accueillir sans refonte :

- le driver ne lit **que la table en RAM**, jamais les tables en flash directement ;
- une seule porte d'entrée, celle du boot : `local_map_apply(table)` passe les contrôles du §4.2, puis remplace la table d'un coup entre deux trames ; refusée, l'ancienne reste. Le boot l'appelle avec la table retenue ; un changement à chaud l'appellera plus tard avec une autre table complète ;
- plus tard : quand un changement est permis (lien local non vivant, ou machine à l'arrêt : une touche réaffectée en roulant pourrait déclencher une commande), qui appelle la porte (tableau de bord, télécommande, page web), et la persistance (NVS ; aucun stockage persistant n'existe aujourd'hui dans le firmware).

## 5. Le contrat : un lien de confiance

Les deux bouts sont dans le même firmware et lisent la même table : l'accord est garanti à la compilation.

- `combus_link_build()` marque le lien « de confiance » quand son port est de type `LOCAL`.
- Le handshake lit ce marqueur à deux endroits : au démarrage (STABLE d'emblée) et dans ses délais de chute (ignorés). Le TX n'émet aucune poignée de main sur ce lien.
- La perte : la manette déconnectée, le driver n'émet plus rien ; le lien tombe par son silence (`linkTimeoutMs`), le superviseur remet les valeurs sûres et lève `NODE_LINK_FAIL`. Au retour, les trames reprennent et le lien redevient vivant aussitôt.
- Au boot, une manette pas encore connectée se comporte comme une télécommande éteinte : `NODE_LINK_FAIL` jusqu'à la première trame.

## 6. Le driver dans la boucle

Aucun appel de plus dans la boucle : le driver se cale sur le RX et le TX.

- **`available()`** (appelé par le RX à chaque tour) : une période passée et la manette connectée, il lit le backend, applique la table en RAM et dépose une trame dans son tampon interne.
- **`write()`** (appelé par le TX) : il ne reçoit que des trames de données, qu'il décode (`combus_frame_decode()`) et passe à la table du retour.
- `input_refresh()` et `input_update()` sortent de `sys_manager`.
- Il manque un **encodeur à partir de positions brutes** : `combus_frame_encode()` lit un `ComBus`, le driver n'a que des valeurs. Une petite fonction dans `frame/`, avec le CRC existant.

## 7. Configuration : port, ComDev, carte

```cpp
enum class ComPortType : uint8_t { NONE = 0, UART, LOCAL };

struct LocalPortCfg {
	const LocalInputDriver* backend;   // le modèle : PS4, SBUS...
	const InputAnalogMap*   inMap;     // table d'entrées retenue (celle du type, ou celle de l'instance)
	const FeedbackMap*      outMap;    // table du retour (vide pour l'instant)
};

const ComPortCfg comPorts[] = {
	ComPortCfg::makeUart ("COM/GPIO", { 2, TxdExtPin, RxdExtPin, UartMaxBaud }),
	ComPortCfg::makeLocal("BT/PS4",   { &kPs4Backend, ps4InMap, ps4OutMap }),
};

const ComProtocolCfg kProtoLocal = { .rate = 0, .txHz = 50, .linkTimeoutMs = 300 };

	// comLinks[] de la carte : seul le port change entre manette et télécommande
{ &kComDevRemote, &comPorts[1], LinkEnd::TX_RX },   // manette PS4
```

- `hw/transport/local_port.cpp`, à côté d'`uart_port.cpp`, et un `case LOCAL` dans `setupPort()`.
- Le backend : un petit jeu de pointeurs de fonction (`begin`, `connected`, `read`, `actuate` optionnel).
- `rate = 0` : pas de débit de ligne, le contrôle de charge saute ce port ; le contrôle du délai face à la période s'applique tel quel.
- Le ComDev est celui de la vraie télécommande (`kRemoteLink`) ; seul le protocole est propre au port virtuel.
- Autres modèles : un récepteur SBUS = un backend qui lit un UART, plus sa table d'entrées ; une autre manette = un autre backend et ses tables. Le choix du backend reste un flag (`INPUT_*`), car les bibliothèques le sont.

Le changement dans le code existant se limite à : le marqueur de confiance et ses deux lectures dans le handshake ; le saut du contrôle de charge à `rate = 0` ; l'encodeur à positions brutes. Le reste est du code nouveau, à côté.

## 8. Étapes et preuves (proposées)

| Étape | Livrable | Preuve |
|---|---|---|
| L1 | Encodeur à positions brutes (`frame/`) | Unity loopback : même trame que `combus_frame_encode()` sur le même contenu |
| L2 | Lien de confiance, saut du contrôle de charge à `rate = 0` | Unity : un lien virtuel reste STABLE sans ACK, un vrai lien sans ACK tombe toujours ; mutant (marqueur sur tous les liens) qui fait échouer le test |
| L3 | `ComPortType::LOCAL`, `local_port`, backend factice scripté pour les tests | Unity : les valeurs du backend arrivent sur le bus ; silence → valeurs sûres et `NODE_LINK_FAIL` ; retour de la manette → lien vivant |
| L4 | Tables : celle du type, celle de l'instance qui la remplace, table en RAM, contrôles du boot, `local_map_apply()` | Unity : la table de l'instance remplace celle du type ; refus (channel hors lien, doublons) ; une table refusée laisse l'ancienne en place |
| L5 | Règle des deux sources | Unity : deux ports locaux sur `kRemoteLink`, les deux vivants → blocage et `NODE_LINK_FAIL` ; un seul → normal |
| L6 | Bascule de la Volvo : la PS4 par le port virtuel, `input_refresh()` / `input_update()` hors de `sys_manager`, `LOCAL_REMOTE_FAIL` retiré (`.cb`, chaîne failsafe) | builds, pytest ; sur la carte, avec une vraie manette (le propriétaire) : pilotage, perte et reconnexion |
| — | Puis l'audit final du link rework, manettes comprises | |

## 9. Questions ouvertes

- Tranché le 2026-11-21 : type de port `LOCAL` ; pas de surcouche ni de fusion (§4.1) ; la modification à chaud hors de ce chantier (§4.3).
- Plus tard, avec la modification à chaud : quand un changement est permis, qui l'appelle, la persistance.
