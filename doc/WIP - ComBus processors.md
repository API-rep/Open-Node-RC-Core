# ComBus processors

Statut : **en cours de rédaction** (document `WIP`), ouvert le 2026-10-25 pendant le *ComBus link rework* (recadrage avant l'étape 2.4). Rien n'est implémenté de ce document, sauf les deux premiers flags de traitement (§3).
Public : le propriétaire et les futures sessions de travail sur la norme des processors.
Plan : §1 objectif, §2 intentions, §3 premier pas fait, §4 questions ouvertes.

## 1. Objectif

Séparer deux choses que le code mélange aujourd'hui :

| Couche | Ce qu'elle dit | Où |
|---|---|---|
| **Channel** | nom, type, portée (`BASE`, `SYSTEM`, `BOARD`), sens, diffusion | `.cb` — **normé par le link rework** |
| **Traitement** | quelle chaîne de processors calcule quels channels, avec quelle configuration, sur quelle carte | à normer (ce document) |

Le recadrage des `BASE none` (2026-10-25) a montré le besoin. Un channel calculé par une chaîne (par exemple `ESC_RPM_BUS`) est un `SYSTEM uplink`. Seule la carte qui fait tourner la chaîne doit le déclarer, sinon la diffusion voit plusieurs producteurs. La portée était juste ; ce qui manquait, c'est de savoir **qui fait le traitement**.

## 2. Intentions

### 2.1 Un flag de fonction par thème

- Chaque grand **thème** (ceux des `.cb` : `input`, `sim`, `light`, `failsafe`, `vbat`, `com`…) a un flag de traitement, `HAS_<THEME>_PROC`, posé dans l'env de la carte qui fait tourner ses chaînes.
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
