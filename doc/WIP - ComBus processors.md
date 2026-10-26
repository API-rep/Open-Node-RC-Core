# ComBus processors

Statut : **en cours de rédaction** (document `WIP`), ouvert le 2026-10-25 pendant le *ComBus link rework* (recadrage avant l'étape 2.4). Rien n'est implémenté de ce document, sauf les deux premiers flags de traitement (§3).
Public : le propriétaire et les futures sessions de travail sur la norme des processors.
Plan : §1 objectif, §2 intentions (dont §2.5 sécurité, décidé le 2026-10-26), §3 premier pas fait, §4 questions ouvertes.

## 1. Objectif

Séparer deux choses que le code mélange aujourd'hui :

| Couche | Ce qu'elle dit | Où |
|---|---|---|
| **Channel** | nom, type, portée (`BASE`, `SYSTEM`, `BOARD`), sens, diffusion | `.cb` — **normé par le link rework** |
| **Traitement** | quelle chaîne de processors calcule quels channels, avec quelle configuration, sur quelle carte | à normer (ce document) |

Le recadrage des `BASE none` (2026-10-25) a montré le besoin. Un channel calculé par une chaîne (par exemple `ESC_RPM_BUS`) est un `SYSTEM uplink`. Seule la carte qui fait tourner la chaîne doit le déclarer, sinon la diffusion voit plusieurs producteurs. La portée était juste ; ce qui manquait, c'est de savoir **qui fait le traitement**.

## 2. Intentions

### 2.1 Un flag de fonction par thème

- Chaque grand **thème** (ceux des `.cb` : `input`, `sim`, `light`, `failsafe`, `vbat`… ; `com` sera fusionné dans `failsafe`, §2.5) a un flag de traitement, `HAS_<THEME>_PROC`, posé dans l'env de la carte qui fait tourner ses chaînes.
- Les flags restent scriptables : un générateur de `platformio.ini` pourra les écrire, ce qui garde la souplesse sans tenue à la main.
- Les channels produits par les chaînes d'un thème ne sont déclarés que là où ce flag est défini. Les autres cartes du nœud les reçoivent par diffusion.

### 2.2 Paquets de traitement

- Une chaîne vit dans le dossier de son thème, sous la forme d'un **paquet** (`.cbproc` ou `.cbprocpkg`). Il contient la chaîne, la configuration de ses processors **et les `.cb` des channels qu'elle produit** (le contenu des `.cb` peut être inclus dans le paquet).
- Le paquet n'est actif que sur la carte qui a le flag de son thème. C'est ce qui garantit **un seul producteur** pour chaque channel calculé.
- `esc_rpm_bus.cb` annonçait déjà ce paquet : « a package that bundles processor + combus together ».

### 2.3 Redéfinition à plusieurs niveaux

Ordre de résolution : **cœur → type de machine → machine, ou machine et env**. Le niveau le plus précis l'emporte. Deux nuances, décidées par le propriétaire :

1. **Redéfinition de la chaîne complète** : le niveau supérieur remplace tout le paquet.
2. **Redéfinition de la configuration seule** : on garde le traitement (la chaîne et ses processors) et on ne redéfinit que sa configuration. On peut ainsi écrire des comportements **types** de traitement, et les personnaliser par machine ou par machine et env sans rééditer toute la chaîne.

Règles à garder :
- un paquet ou une configuration se remplace **en entier**, jamais par morceaux : sinon ça devient ingérable ;
- le journal du build dit, pour chaque chaîne, quel niveau a gagné, et si c'est la chaîne ou seulement la configuration.

### 2.4 Lien avec le link rework

Le link rework garde une seule règle : **un channel `SYSTEM uplink` est déclaré par le traitement qui le produit, actif sur une seule carte**. En attendant les paquets, c'est fait par `requires: [HAS_<THEME>_PROC]` dans les `.cb` de `core/config/machines/<type>/processors/`.

### 2.5 Sécurité : valeur sûre et deux chaînes failsafe (décidé le 2026-10-26)

Côté channels, la règle est dans `doc/WIP - ComBus link rework.md` §2.3 (« Valeur sûre = neutre ») et §2.4 (« `onLoss` supprimé »). Côté traitement, elle impose :

- **Toute chaîne fonctionne de façon sûre aux valeurs sûres** : `CbusNeutral` pour un analogique, `false` pour un numérique, `true` pour un numérique du thème `failsafe`. Exemples : frein ≤ neutre → frein nul (`cb_brake` fait aujourd'hui `>> 1`, donc mi-frein au neutre : à changer) ; rapport, sous-rapport, mode au neutre → point mort ou état sûr ; `RUNLEVEL` au neutre → réaction de `HALT`.
- **Deux chaînes failsafe**, défendables toutes les deux : une **dure** (agrégat `FAILSAFE_HARD` → runlevel `HALT` : stop, sommeil, désactivation des moteurs, clé bloquée tant que le défaut dure) et une **douce** (agrégat `FAILSAFE_SOFT` → runlevel `IDLE`). Le nom de l'agrégat dit la **sévérité**, pas la réaction (qui peut évoluer sans renommage). Les **contributeurs** disent un **fait**, sans sévérité (`*_FAIL`) : le branchement vers l'une ou l'autre chaîne se décide dans la chaîne, et se change en déplaçant une ligne.
- **`RunLevel::FAILSAFE` devient `HALT`** : `TURNING_OFF` n'est pas équivalent (transitoire par construction, prévu pour les sons d'extinction, réécrit `IDLE` dans le même cycle, si bien que les autres cartes verraient `IDLE` ; moteurs réveillés et non endormis ; perte de l'information « arrêt sur défaut »). Le mot `FAILSAFE` devient le nom de famille.

Noms (renommage mécanique, étape S1 du link rework) :

| Aujourd'hui | Rôle | Nouveau nom |
|---|---|---|
| `RunLevel::FAILSAFE` | runlevel de la chaîne dure (valeur 6 inchangée) | `RunLevel::HALT` |
| `FAILSAFE` | agrégat dur | `FAILSAFE_HARD` |
| `REMOTE_LINK_LOST` | agrégat doux (→ `IDLE`) | `FAILSAFE_SOFT` |
| `FAILSAFE_VBAT` | contributeur, batterie | `VBAT_FAIL` |
| `FAILSAFE_COM_LINK` | contributeur, lien entre cartes perdu | `BOARD_LINK_FAIL` (S2 : alimenté par portée) |
| `COM_LINK_LOST` | contributeur, lien vers un autre nœud perdu | `NODE_LINK_FAIL` (S2 : alimenté par portée) |
| `PS4_DS4_BT_LINK_LOST` | contributeur, manette perdue | `PS4_DS4_BT_FAIL` |
| `TEST_COUPLED_FAULT` | défaut d'essai partagé | `TEST_COUPLED_FAIL` |

Flags, plus tard : `HAS_FAILSAFE` reste le parapluie ; `HAS_REMOTE_LINK_LOST_FALLBACK` pourrait devenir `HAS_FAILSAFE_SOFT`.

**Trous du runlevel trouvés le 2026-10-26** (à corriger avec la règle, étape S3) :
1. les feux s'éteignent tous en `FAILSAFE` : `kRunLevelMask` a 7 cases (`RunLevel::COUNT`) mais 6 valeurs écrites (`dumper_truck_lights.h:91-98`), la case `FAILSAFE` vaut `0`, soit « tout éteint de force » ; les warnings de batterie basse (`light_interpreter.cpp:55, 64`) ne peuvent pas clignoter en failsafe ;
2. le tableau de bord affiche `---` pour `FAILSAFE` (pas de `case`, `dashboard.cpp:285-294`) ;
3. `runlevel_procs.h` est mort (voir §4) et décrit encore « `FAILSAFE` (= `IDLE` alias) » ;
4. une valeur inconnue de `RUNLEVEL` ne fait rien (`default: break`, `main.cpp:224-225`) : les moteurs gardent leur dernier état. Avec « neutre = sûr », une carte qui perd son lien verrait `RUNLEVEL` = 32767 : le `default` doit appliquer la réaction de `HALT`.

### 2.6 Audit S0 (2026-10-26, lecture seule)

**Démarrage : sûr.**
- `init.cpp:62-68` écrit `RUNLEVEL` = `DEF_RUNLEVEL` (`IDLE`, `config.h:30`), puis arrête, endort et désactive les moteurs.
- À chaque cycle, `sys_manager_update()` calcule l'agrégat failsafe (`sys_manager.cpp:24-42` : réception, entrées, batterie, `failsafe_update`) **avant** la chaîne runlevel et le `switch` de `main.cpp:57-75`. La valeur de démarrage de `FAILSAFE` n'atteint donc jamais la machine à états.

**Polarité `true` du thème `failsafe` : elle est nécessaire, pas seulement conventionnelle.**
- La chaîne remet l'agrégat à 0 en début de cycle, fait le OU des contributeurs, puis **réarme chaque contributeur à `true`** (`failsafe_chain.cpp:44-78`, « consume + guard »).
- Un écrivain qui cesse d'écrire laisse donc son contributeur à `true` : c'est une preuve de vie.
- Même principe pour la manette (`input_update.cpp:35` : « stays at its failsafe default, true = lost »).
- Ça confirme la décision §2.5 : `true` pour le thème `failsafe`.

**`RUNLEVEL` au neutre (32767) : rien de sûr aujourd'hui.**
- `RunLevel` est un `int8_t` (`machines_defs.h:18`). Converti en `RunLevel`, 32767 (0x7FFF) donne -1 = `NOT_YET_SET`.
- Lecteurs :
  - `main.cpp:70` tombe dans `default: break` (`main.cpp:224-225`) : les moteurs gardent leur état ;
  - les feux (`light_interpreter.cpp:59-60`) prennent la case `IDLE` ;
  - le tableau de bord affiche `---`.
- Correctif S3 : `default` → réaction de `HALT`, déjà noté au trou n° 4 de §2.5.
- Seul écrivain : la carte qui a `HAS_RUNLEVEL_MANAGER`. Une carte qui le reçoit (extension, quand elle aura un firmware : `test_node_extboard` n'est jamais compilé, `platformio.ini:253-259`) **ne fait pas tourner la chaîne runlevel** (`HAS_INPUT_PROC` sur la carte mère seulement). À la perte de son lien, elle ne passerait donc jamais en `HALT` par elle-même. **À concevoir** avant le premier firmware d'extension : la réaction locale d'une carte qui perd la carte mère.

**`BRAKE_BUS` : l'entrée et la sortie du traitement sont le même channel.**
- La manette écrit le frein brut (`PS4_dualshock_map.cpp:38`, gâchette L2).
- `b-in` le lit (`throttle_config.h:133-137`), puis `brake` **réécrit** `BRAKE_BUS` avec le frein traité (`throttle_config.h:139-145`, `cb_brake.cpp:110`).
- Au neutre, `cb_brake_fn` donne `raw` = 16383 et `brakeVal` = raw² / neutre ≈ 8191, soit **environ 25 % de frein** (courbe au carré, `cb_brake.cpp:32-34`).
- Avec une source par lien (trames espacées, pas d'écriture à chaque cycle), le frein traité serait relu comme brut au cycle suivant.
- À S3 : frein ≤ neutre → nul, et **séparer l'entrée brute de la sortie traitée** (deux channels).

**`GEAR_BUS` et `SUBGEAR_BUS` sont des états calculés, déclarés comme des commandes.**
- Les deux sont `BASE downlink_pt`, avec `value` `1` et `0`.
- Mais `GEAR_BUS` est la **sortie** de la chaîne de boîte (`sim/proc_config.cpp:79-84`), et `SUBGEAR_BUS` celle du compteur de boutons (`input/proc_config.cpp:61-66`).
- Ce sont des états calculés par la machine, comme `ESC_RPM_BUS` : le recadrage des `BASE none` (2026-10-25) les a manqués. Ils devraient être `SYSTEM uplink` (`requires: HAS_SIM_PROC` / `HAS_INPUT_PROC`), la commande restant les boutons.
- Au neutre (32767), tout lecteur qui teste `!= 0` voit « actif » :
  - régulateur en maintien de vitesse (`cb_cruise.cpp:52`) ;
  - boîte forcée en 1re (`gear_config.h:101-106`) ;
  - vitesse plafonnée (`traction_config.h:39-44`).
- À traiter avec S3 : recadrage `SYSTEM`, puis codage centré (ci-dessous).

**Décision (2026-10-26) : le codage centré, champ `zeroAt`.**
- Un analogique peut déclarer `zeroAt: neutral` (le défaut est `zeroAt: start`, le codage d'aujourd'hui). Valeur sur le bus = valeur logique + `CbusNeutral`.
- La valeur sûre reste toujours `CbusNeutral` sur le bus ; `zeroAt: neutral` dit seulement que le zéro logique s'y trouve. Donc pas d'exception à la règle de §2.5 : au neutre, `SUBGEAR_BUS` vaut logique 0 (pas de sous-rapport), `GEAR_BUS` logique 0 (point mort), et les valeurs négatives deviennent possibles (marche arrière = −1).
- `zeroAt` est haché dans le MD5 : deux bouts en désaccord sur le codage sont refusés au handshake.
- Mise en œuvre :
  - le stockage reste analogique (un vrai troisième type toucherait la trame, le codec, les tables et les buffers) ;
  - le builder génère un bit `centered` par channel ;
  - **le moteur des chaînes convertit** l'entrée et la sortie des processors pour les channels marqués, si bien que les processors ne changent pas ;
  - le code hors chaîne passe par des accesseurs dédiés ; les accès directs au tableau pour ces channels sont à recenser avant de coder.
- Écartés : un jeton de valeur `value: neutralCenter` (un même mot pour la valeur sûre et le codage) et un type de surface `type: index` (fait croire à un nouveau stockage).
- À trancher en S3 : `RUNLEVEL` en `zeroAt: neutral` (au neutre, logique 0 = `IDLE`, déjà sûr ; ou réordonner l'énumération pour que 0 = `HALT`).

## 3. Premier pas fait (recadrage du 2026-10-25)

| Flag | Chaîne | Code | Channels produits |
|---|---|---|---|
| `HAS_INPUT_PROC` | `inputChain` (boutons vers états, runlevel) | `volvo_A60H_bruder/combus/processors/input/proc_config.cpp` (était sous `IS_MAINBOARD`) | `KEY_ACTIVE` (avec `HAS_IGNIT_KEY`), `DIRECT_DRIVE`, `CRUISE_ACTIVE` — `dumper_truck/processors/input.cb`, thème `input` |
| `HAS_SIM_PROC` | `simChain` (régime, traction, état de conduite) | `volvo_A60H_bruder/combus/processors/sim/proc_config.cpp` (était sous `IS_MAINBOARD`) | `ESC_SPEED_BUS`, `DRIVE_STATE_BUS`, `ESC_RPM_BUS` — `dumper_truck/processors/traction.cb`, `throttle.cb`, thème `sim` (nouveau) |

Les deux flags sont posés sur la carte mère de la Volvo et du nœud d'essai. Le lien « remote envoie l'intention, la machine calcule l'état » a été acté en même temps : `KEY_BTN`, `DIRECT_DRIVE_BTN` et `CRUISE_TOGGLE_BTN` sont des commandes `BASE downlink`.

## 4. Questions ouvertes

- Format du paquet : extension (`.cbproc` ou `.cbprocpkg`), contenu (YAML qui décrit la chaîne, ou YAML qui pointe vers le C++ des processors), place des `.cb` inclus.
- Ce qui est généré à partir du paquet (tableaux `CbProc` / `CbChain`, aujourd'hui écrits à la main dans `proc_config.cpp` et les `*_config.h`), et ce qui reste du C++ écrit à la main (les fonctions des processors).
- Comment se déclare une redéfinition de configuration seule (nom du traitement type, champs remplaçables).
- Liste des thèmes qui ont des chaînes, et nom de leurs flags ; place de la chaîne du runlevel, aujourd'hui dans les chaînes d'entrée.
- Fichier mort à nettoyer : `volvo_A60H_bruder/combus/processors/runlevel/runlevel_procs.h` n'est inclus nulle part (remplacé par `input/runlevel_config.h`).
- Les autres boutons bruts de `inputs/input.cb` (`SUBGEAR_SET_BTN`, `GEAR_UP_BTN`, `GEAR_DOWN_BTN`, `CRUISE_UPDATE_BTN`, `HORN_BTN`) : commandes qu'une télécommande pourra envoyer, donc `BASE downlink` comme `KEY_BTN` ? Les sticks bruts restent `SYSTEM` (périphérique local, avant conditionnement).
