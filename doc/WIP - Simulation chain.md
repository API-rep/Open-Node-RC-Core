# WIP - Simulation chain

Remise en ordre de la chaîne de traitement d'une machine : des entrées (boutons, sticks) jusqu'aux sorties du type de machine (channels `BASE`), en passant par la simulation et le motion. Ouvert le 2026-10-29, à la suite du lot « sécurité » du ComBus link rework (S3), dont il reprend les briques 3 et 4 (rapports, `zeroAt` sur les channels réels, frein).

Documents liés :
- `doc/WIP - ComBus link rework.md` (lignes S3-1 à S3-4 : `HALT`, contrôle du neutre, `zeroAt`) ;
- `doc/WIP - ComBus processors.md` (flags de traitement par thème, paquets de traitement, redéfinition core → type de machine → machine) : le **format** des paquets et des redéfinitions y reste ; ce WIP-ci décide **quoi** chaîner et **dans quel ordre** ;
- `doc/WIP - Light rework.md` (les feux, consommateurs des sorties).

---

## 1. Principe (proposé par le propriétaire le 2026-10-27)

- **Le type de machine (`machine_type`) déclare un contrat**, et seulement lui :
  - **en entrée**, les boutons et sticks du type de véhicule (`GEAR_UP_BTN`, `GEAR_DOWN_BTN`, `THROTTLE_STICK`…) ;
  - **en sortie**, ses channels `BASE` (`GEAR_BUS`, `THROTTLE_BUS`, `BRAKE_BUS`…).
- **Le core fournit par défaut une chaîne minimale** pour chaque sortie : un branchement direct entrée → sortie `BASE`. Une machine sans particularité fonctionne avec ce seul défaut.
- **La machine (qui hérite du type) peut remplacer la chaîne d'une sortie** (overwrite) pour y ajouter de la simulation ou du motion, **à contrat de sortie constant** : même channel, même sens.
- **Les consommateurs lisent la sortie « à l'aveugle »** (boîte mécanique, son, motion, feux) : ils ignorent si elle vient d'un bouton, d'une simulation ou d'un automatisme.

Exemple, `GEAR_BUS` (un même contrat, trois variantes) :

| Variante | Entrées utilisées | Chaîne qui écrit `GEAR_BUS` |
|---|---|---|
| boîte mécanique | `GEAR_UP_BTN` / `GEAR_DOWN_BTN` | branchement par défaut, ou un compteur simple |
| boîte virtuelle | `GEAR_UP_BTN` / `GEAR_DOWN_BTN` | chaîne de boîte simulée (`cb_gear`…) |
| boîte automatique | aucune (boutons non branchés) | module « auto gear » (régime, charge…) |

---

## 2. Décisions déjà prises (à respecter)

| Décision | Date | Source |
|---|---|---|
| Valeur sûre = neutre sur toutes les portées (option A, cohérence entre les couches) : un analogique déclare `CbusNeutral` ; les chaînes doivent être sûres au neutre | 2026-10-26 / 27 | link rework §2.3, S3-3 |
| Liste vivante des analogiques pas encore au neutre (avertissement du builder) : `BRAKE_BUS`, `GEAR_BUS`, `SUBGEAR_BUS`, `ESC_RPM_BUS`, `RUNLEVEL` ; quand elle est vide, `NEUTRAL_VALUE_IS_ERROR = True` | 2026-10-27 | S3-3, `test_neutral_value.py` |
| `zeroAt: neutral` existe (champ `.cb`, MD5, drapeau `zeroAtNeutral`, accesseurs) ; aucun channel réel ne l'utilise encore | 2026-10-29 | S3-4 |
| Deux familles d'accesseurs : brut (`combus_*_raw_analog` : transport, sticks, moteur des chaînes) et logique (`combus_*_analog`, `int32_t`) ; les sticks restent en brut | 2026-10-29 | S3-4 |
| **Les processors ne voient que des valeurs logiques** (acté dans `doc/WIP - ComBus processors.md` §2.7) : le moteur des chaînes convertit à l'entrée et à la sortie, le pipeline passe en `int32_t` ; pas de brut dans une chaîne (gain d'optimisation négligeable, risque de mélange silencieux) ; une exception seulement après une mesure, déclarée par processor | 2026-10-29 | ce WIP |
| Une seule chaîne écrit chaque sortie `BASE` sur un nœud (le défaut **ou** son remplacement) | 2026-10-27 | ce WIP (le builder pourra le vérifier, comme `check_single_source()`) |
| `GEAR_BUS` reste `BASE` (sortie du contrat du type), en `zeroAt: neutral` (point mort = 0 logique, marche arrière = −1) | 2026-10-27 | ce WIP |
| Les chaînes déclarées (`.cbch`) auront un champ `requires`, identique à celui des `.cb` ; d'ici là, une chaîne C++ se garde par un `#if` (forme temporaire, ex. la chaîne d'un coupleur gardée par `COMBUS_COUPLER_<NOM>`, link rework 3.4e) | 2026-10-31 | link rework 3.4e |

---

## 3. La télécommande n'envoie que les entrées (décidé le 2026-10-29)

Les `*_BUS` restent `BASE` (sorties du contrat du type), mais ne passent pas sur le fil de la télécommande : seules les commandes de stick et de bouton y descendent. La machine seule tient l'état.

- **Pour** : télécommande sans état (rien à stocker, pas de simulation) ; reprise par une autre télécommande sans conflit, l'état vit en un seul endroit (la machine) ; « neutre = aucune action » tombe juste (stick centré, bouton relâché).
- **Écarté** : transférer l'état au handshake et laisser la télécommande le gérer (deux propriétaires d'un même état, resynchronisation à chaque reprise).
- **À préciser** :
  - « pas sur le fil » veut dire pas sur le fil **de la télécommande** : un `*_BUS` peut encore aller du véhicule vers un attachement (`_pt`, modèle en étoile) ;
  - un retour d'information vers la télécommande (afficher le rapport) serait en lecture seule ;
  - comment le builder sait que le lien télécommande–machine ne porte que les entrées : rejoint le chantier 4 (télécommande comme nœud, ESP-NOW).
- Conséquence : les entrées deviennent des channels du contrat transportables (aujourd'hui `inputs/input.cb` les déclare `SYSTEM none`, sauf `KEY_BTN`, `DIRECT_DRIVE_BTN`, `CRUISE_TOGGLE_BTN` passés `BASE downlink` le 2026-10-25).

---

## 4. État des lieux (relevé du 2026-10-29, Volvo A60H)

### 4.1 Chaînes d'entrée (`HAS_INPUT_PROC`, `volvo_A60H_bruder/combus/processors/input/proc_config.cpp`)

| Chaîne | Entrée → sortie | Nature |
|---|---|---|
| `subgear` | `SUBGEAR_BUS` → `SUBGEAR_BUS` (boutons lus par les procs) | état (compteur), réécrit sur lui-même |
| `direct_drive` | `DIRECT_DRIVE` → `DIRECT_DRIVE` | état (bascule) |
| `key_runlevel` | `KEY_ACTIVE` → `KEY_ACTIVE` | état (bascule de clé) |
| `runlevel` | `RUNLEVEL` → `RUNLEVEL` | machine à états de sécurité (S3-1) |
| `cruise_normal` | `CRUISE_ACTIVE` → `CRUISE_ACTIVE` | état (bascule) |
| `throttle` | `THROTTLE_STICK` → `THROTTLE_BUS` | branchement direct (sans proc) |
| `steering` | `STEERING_STICK` → `STEERING_BUS` | branchement direct |
| `dump` | `DUMP_STICK` → `DUMP_BUS` | branchement direct |

### 4.2 Chaînes de simulation (`HAS_SIM_PROC`, `.../processors/sim/proc_config.cpp`)

| Chaîne | Entrée → sortie |
|---|---|
| `throttle` | `THROTTLE_BUS` → `ESC_RPM_BUS` (lit aussi `BRAKE_BUS`, `DIRECT_DRIVE`, `SUBGEAR_BUS`, `CRUISE_ACTIVE`) |
| `traction` | `ESC_RPM_BUS` → `ESC_SPEED_BUS` |
| `gear` | `ESC_RPM_BUS` → `GEAR_BUS` |
| `steering` | `STEERING_BUS` → `STEERING_BUS` (réécrit sur lui-même) |
| `dump` | `DUMP_BUS` → `DUMP_BUS` (réécrit sur lui-même) |

### 4.3 Processors du core (`src/core/system/combus/processors/`)

`base/` (`cb_bypass`, `cb_io`, `cb_reset`, `cb_runlevel`), `input/` (`cb_btn`), `logic/` (`cb_or`), `math/` (`cb_abs`, `cb_center`, `cb_scale`), `motion/` (`cb_brake`, `cb_cruise`, `cb_dir`, `cb_ramp`), `modules/gear/` (`cb_gear`).

### 4.4 Ce qui fait « foutoir »

1. **Tout est écrit dans la machine** (`volvo_A60H_bruder/`) : aucun défaut au niveau du type ou du core ; une seconde machine du même type recopierait tout.
2. **Des chaînes réécrivent leur propre entrée** (`STEERING_BUS`, `DUMP_BUS`, et `BRAKE_BUS` dans la chaîne `throttle`). C'était une **convention voulue** (précision du propriétaire, 2026-10-29) : le channel d'entrée fait office de valeur brute et il est réécrit de traitement en traitement jusqu'à la sortie. L'idée tient pour une chaîne seule, mais devient bancale dès qu'une autre chaîne lit la même valeur (laquelle lit-elle : brute ou traitée ?), et quand la source n'écrit pas à chaque cycle (la sortie traitée est relue comme brute au cycle suivant : audit S0, `doc/WIP - ComBus processors.md` §2.6, frein à environ 25 % au neutre). Avec le principe du §1, l'entrée (stick, gâchette) et la sortie `BASE` deviennent deux channels distincts.
3. **Entrée et simulation mêlées** : les passages directs stick → `*_BUS` sont dans les chaînes d'entrée, la simulation des mêmes sorties dans les chaînes `sim`, sans notion de « défaut remplacé ».
4. **Les états calculés** (`SUBGEAR_BUS`, `GEAR_BUS`) déclarés `BASE downlink_pt`, comme des commandes reçues.
5. **Boutons de deux natures** : `KEY_BTN`, `DIRECT_DRIVE_BTN`, `CRUISE_TOGGLE_BTN` en `BASE downlink`, les autres (`SUBGEAR_SET_BTN`, `GEAR_UP_BTN`, `GEAR_DOWN_BTN`, `CRUISE_UPDATE_BTN`, `HORN_BTN`) en `SYSTEM none` (question ouverte, processors §4).
6. **Les chaînes ne sont pas sûres au neutre** : la liste vivante de S3-3 (frein, rapports, régime, runlevel).

---

## 5. Questions ouvertes

1. **Où vit le branchement par défaut** : un `.cbproc` (ou équivalent) du type de machine dans `core/config/machines/<type>/`, généré par le builder ? Format : `doc/WIP - ComBus processors.md` §4.
2. **Comment la machine déclare un remplacement** (par nom de sortie ?) et comment le builder vérifie « une seule chaîne par sortie ».
3. **La télécommande n'envoie que les entrées** (§3) : à confirmer, et ses conséquences sur les directions.
4. **Pipeline signé** (`int32_t`) : relire les processors qui supposent 0..65535 (bornes, `CbusMaxVal`, `CbAnalog` signe/grandeur sur 16 bits) ; chiffrer la RAM (estimé ~120 octets pour la Volvo).
5. **Lectures directes `analogBus[…].value`** dans le code applicatif (tableau de bord : `dashboard_simulation.cpp:160` pour `GEAR_BUS`…) : à faire passer par `combus_get_analog` quand leur channel devient centré.
6. **`RUNLEVEL` dans la liste vivante** : au neutre (32767) il tomberait dans le `default` (→ `HALT` verrouillé). Piste du propriétaire (2026-10-29) : un zéro en début de variable, le souci étant l'énumérateur −1. Relevé : `RunLevel::NOT_YET_SET` (−1) n'a qu'un usage, la valeur de départ de `lastRunLevel` (`main.cpp:66`, forcer « nouveau runlevel » au premier tour), facile à retirer. Mais le −1 vu au neutre vient surtout de la troncature : `RunLevel` est un `int8_t`, et 32767 (0x7FFF) converti donne 0xFF = −1, quel que soit l'énumérateur. Options : `zeroAt: neutral` (0 logique = `IDLE` au neutre, lecture par `combus_get_analog`), ou décaler l'énumération (et changer son type sous-jacent). À trancher en SC.
7. **`ESC_RPM_BUS` dans la liste vivante** : le propriétaire (2026-10-29) distingue une **commande mécanique** (comme une commande servo d'ESC, 0 au centre : préférée par sécurité, plus parlante pour l'utilisateur) d'une **commande logique** (son…). Relevé : `ESC_RPM_BUS` est le régime moteur après rapport, une grandeur sans signe (le sens est dans `DRIVE_STATE_BUS`, `cb_gear.cpp:31`), lue par la traction, le tableau de bord et le son : c'est une valeur **logique**. La commande mécanique des moteurs est `ESC_SPEED_BUS` (sortie de la traction), déjà au neutre en convention servo (centre = arrêt). À trancher : régime signé en `zeroAt: neutral`, ou grandeur au neutre = ralenti.


**Pistes du propriétaire (2026-10-29), à reprendre ici :**
- **`RUNLEVEL` décalé franchement** : `NOT_SET = 0` (le « YET » tombe), naturel puisque la variable vaut 0 à l'init ; `IDLE` et la suite décalés de 1. `NOT_YET_SET` n'a qu'un usage aujourd'hui (`main.cpp:66`). À régler avec : ce que la machine fait en `NOT_SET`, et le neutre (32767 converti en `int8_t` reste −1, donc inconnu → `HALT`).
- **`ESC_RPM_BUS` logique, zéro au début** : vérifié, jamais négatif (la chaîne passe par `cb_abs` puis `cb_scale`, `throttle_config.h:172-176` ; le sens est dans `DRIVE_STATE_BUS`).
- **Conséquence sur la règle de S3-3** (« un analogique déclare `CbusNeutral` ») : un channel à zéro au début (grandeur sans signe : régime, frein, sous-rapport, runlevel décalé) y contredit. **Décidé (2026-10-29) : valeur sûre = zéro logique**, soit `CbusNeutral` pour un channel `zeroAt: neutral` et `0` pour un channel `zeroAt: start` ; les channels bipolaires (sticks, `THROTTLE_BUS`, `STEERING_BUS`, `DUMP_BUS`, `ESC_SPEED_BUS`) déclareraient alors `zeroAt: neutral`, et le contrôle du builder vérifierait `value` = zéro logique.
---

## 6. Feuille de route (proposée, à valider)

**Ordre décidé (2026-10-29)** : ce qui relève des processors (pipeline signé, conversion dans le moteur des chaînes) est acté dans `doc/WIP - ComBus processors.md` ; on finit d'abord le ComBus link rework, en ne convertissant que le nécessaire et en gardant toutes les chaînes fonctionnelles ; ce WIP-ci vient ensuite.

| Étape | Contenu |
|---|---|
| SC0 | Audit en lecture seule : chaque chaîne et chaque processor, ses entrées et sorties, ce qu'il suppose de la plage (brut 0..65535, neutre), ses lectures directes du bus |
| SC1 | Pipeline signé : moteur des chaînes en logique (`int32_t`), processors adaptés, sans changer aucun comportement (aucun channel centré encore). Décision et détail : `doc/WIP - ComBus processors.md` §2.7 |
| SC2 | Séparer entrées et sorties : chaque `*_BUS` réécrit sur lui-même reçoit son entrée propre (stick, gâchette) ; frein ≤ neutre → frein nul ; `BRAKE_BUS` repasse à `CbusNeutral` |
| SC3 | `GEAR_BUS` et `SUBGEAR_BUS` en `zeroAt: neutral`, chaînes de boîte et de sous-rapport adaptées ; sortie de la liste vivante |
| SC4 | Défauts au niveau du type de machine et remplacement par la machine (format avec le WIP processors) ; vérification « une seule chaîne par sortie » |
| SC5 | `ESC_RPM_BUS`, `RUNLEVEL` : codage décidé, liste vivante vide, `NEUTRAL_VALUE_IS_ERROR = True` |
