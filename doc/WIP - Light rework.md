# WIP - Light rework

État des lieux du module de feux, relevé pendant le ComBus link rework (lot « sécurité », S3, brique 2, le 2026-10-27). Le propriétaire a décidé de laisser les feux à un rework à part : le sujet n'est pas critique pour le link rework, et une partie du code pourrait migrer vers des processors `CbProc`.

Ce document n'est qu'une liste de constats et de décisions à reprendre. Rien n'y est encore fait.

---

## 1. Ce qui existe

Environ 1 800 lignes, dans `src/core/system/light/` et `src/core/config/machines/dumper_truck/light/` :

- **Modèle en deux couches.** Une table de masques par runlevel (`kRunLevelMask`, `dumper_truck_lights.h`) et un masque calculé à chaque cycle depuis le bus (`light_interpreter.cpp`). Un canal s'allume si l'un des deux le demande (« wired-OR », `light_core.cpp`).
- **Types de canaux** (`light_core.cpp`) : feu simple PWM avec effet xénon, clignotant avec feu de position latéral, feu arrière (position + stop), gyrophare, atténuation pendant le démarrage. Un *null-guard* éteint tout quand le masque du runlevel vaut 0 (`SLEEPING`, `RESET`).
- **Sortie matérielle.** `light_init()` (`light.cpp`) réserve les sorties PWM d'après la board config (`LightPort`, `include/struct/machines_struct.h`) ; `light_servo_beacon` pilote un gyrophare à servo.
- **Barre néopixel** : cinq animations reprises de l'ancien code (`neopixel/neopixel_modes.cpp`).
- **Config complète d'un dumper** : `dumper_truck_lights.h` (canaux de LED, clignotants, néopixel, table par runlevel) et `light.cb` (`LIGHTS`, `INDICATOR_LEFT`, `INDICATOR_RIGHT`, `HAZARDS`, `BRAKING`).

Dernier vrai travail : avril 2026 (`light_core.cpp`, 2026-04-26). Depuis, seulement des renommages mécaniques.

---

## 2. Pourquoi il ne tourne pas

| # | Constat | Où |
|---|---|---|
| 1 | Rien ne l'appelle : ni `light_init()`, ni `light_interp_update()`, ni `light_core_update()` n'ont d'appelant ; aucune board ne déclare de `LightPort` | `src/machines/` |
| 2 | Rien ne le compile : aucun env ne définit `LIGHT_ENABLE` ; l'env `machines` exclut `neopixel/` | `platformio.ini` (`build_src_filter` de `[env:machines]`) |
| 3 | `light_interpreter.h` utilise `ComBus` sans l'inclure (`'ComBus' does not name a type`) | `light_interpreter.h:47` |
| 4 | L'interpréteur lit trois channels qui n'existent plus : `LOW_BEAM` (remplacé par `LIGHTS` dans `light.cb`), `HIGH_BEAM`, `ROOF_LIGHT` | `light_interpreter.cpp:42, 47, 88` |

Les constats 3 et 4 ont été vérifiés le 2026-10-27 par une compilation d'essai (`LIGHT_ENABLE` ajouté à l'env `test_runlevel`, retiré ensuite) :

```text
src/core/system/light/light_interpreter.h:47:32: error: 'ComBus' does not name a type
src/core/system/light/light_interpreter.cpp:42:84: error: 'HIGH_BEAM' is not a member of 'DigitalComBusID'
src/core/system/light/light_interpreter.cpp:47:84: error: 'ROOF_LIGHT' is not a member of 'DigitalComBusID'
src/core/system/light/light_interpreter.cpp:88:60: error: 'LOW_BEAM' is not a member of 'DigitalComBusID'
```

---

## 3. Défauts de fond

1. **Warning de batterie basse permanent.** L'interpréteur lit `VBAT_FAIL` (`light_interpreter.cpp:55, 64`). C'est un contributeur du failsafe : la chaîne dure le remet à `true` après chaque lecture (preuve de vie, `src/core/system/failsafe/failsafe_module.md` §4), et les feux passent après elle. Les warnings clignoteraient donc en permanence.
2. **Tout s'éteint en `HALT`.** `kRunLevelMask` a 7 cases (`RunLevel::COUNT`) mais 6 valeurs écrites (`dumper_truck_lights.h:91-98`) : la case `HALT` vaut 0, le *null-guard* éteint tout, warnings compris.
3. **Lecture hors du tableau.** `light_interpreter.cpp:59-61` indexe `mod.runLevelMask[rl]` sans borne haute : une valeur de `RUNLEVEL` ≥ `COUNT` lit hors du tableau. Une valeur négative (32767 converti en `int8_t` donne -1) prend la case `IDLE`.
4. **`hazardsWhile5thWheelOpen`** est marqué TBD (`light_state.h:239`).

---

## 4. Règles validées pendant le link rework, à appliquer ici

Tableau validé le 2026-10-27 (règles R1 à R3 de la brique 2 de S3, reportées) :

- **R1. En `HALT`, les warnings clignotent**, comme sur un véhicule en panne : la case `HALT` de `kRunLevelMask` reçoit `IND_L | IND_R`, et seulement ça. Le *null-guard* ne s'applique plus ; les feux commandés restent possibles (ils ne mettent rien en mouvement).
- **R2. Les feux ne lisent plus `VBAT_FAIL`.** Batterie basse → `FAILSAFE_HARD` → `HALT` → warnings par R1. Il reste `forcedHazard = hazard`. Règle générale : on ne lit jamais un contributeur du failsafe, seulement les agrégats `FAILSAFE_HARD` / `FAILSAFE_SOFT` ou le runlevel.
- **R3. Une valeur hors de l'énumération** (négative ou ≥ `COUNT`) prend la case `HALT`.

| # | Situation | Attendu |
|---|---|---|
| 1 | `HALT`, rien de commandé | warnings qui clignotent |
| 2 | `HALT`, `LIGHTS` commandé | warnings + codes |
| 3 | `RUNNING`, batterie saine | pas de warnings |
| 4 | `RUNNING`, `HAZARDS` commandé | warnings |
| 5 | `RUNLEVEL` = 32767 (-1 une fois converti) | case `HALT` |
| 6 | `RUNLEVEL` = 100 | case `HALT` |
| 7 | `IDLE`, `RUNNING`, `SLEEPING` | inchangés |

Une version Unity de ce tableau a été écrite (env `test_runlevel`, interpréteur réel et table réelle du dumper) puis retirée, faute de pouvoir compiler le module. Elle se retrouve dans l'historique de la session du 2026-10-27 ; à réécrire au premier build du module.

---

## 5. Pistes pour le rework

- Décider ce qui migre vers des processors `CbProc` (masques par runlevel, interpréteur) et ce qui reste du pilotage matériel (`light_core`, `statusLED`, néopixel).
- Channels manquants : feux de route, gyrophare, appel de phares (`flasher`, TBD) ; à déclarer dans `light.cb` (change le MD5 `BASE` du dumper).
- Brancher l'appel dans la boucle, déclarer un `LightPort` dans une board config, poser `LIGHT_ENABLE` dans un env.
- Thème `light` : flag de traitement `HAS_LIGHT_PROC` (règle des thèmes, `doc/WIP - ComBus processors.md` §1).
