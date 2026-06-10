# ComBus Processors — Roadmap & Inventory

Dernière mise à jour : 2026-06-10 (fin de saison 2026).

---

## 1. Inventaire — implémenté ✅

### `base/`
| Fichier | Fonction | Rôle |
|---------|----------|------|
| `cb_bypass` | `cb_bypass_fn` | Court-circuit conditionnel : claim si canal digital actif → valeur brute transmise au `outCh` sans autre proc |
| `cb_io` | `cb_io_fn` | Lecture / écriture de canal secondaire (side-channel dans une chaîne) |
| `cb_runlevel` | `cb_runlevel_fn` | Bloque la chaîne sous un RunLevel minimum |

### `math/`
| Fichier | Fonction | Rôle |
|---------|----------|------|
| `cb_abs` | `cb_abs_fn` | Valeur absolue (bipolaire → unipolaire simple) |
| `cb_center` | `cb_center_fn` | Recentrage : soustrait le neutre pour ramener à 0 |
| `cb_scale` | `cb_scale_fn` | Rescale linéaire `[0..inMax] → [0..outMax]` |

### `input/`
| Fichier | Fonction | Rôle |
|---------|----------|------|
| `cb_btn` | `cb_btn_fn` | Agrégateur boutons digitaux → canal unique (toggle/momentary) |

### `motion/`
| Fichier | Fonctions | Rôle |
|---------|-----------|------|
| `cb_ramp` | `cb_sym_ramp_fn` | Inertie symétrique bipolaire (accél + décel) avec `accelSteps`/`brakeSteps` |
| `cb_ramp` | `cb_uni_ramp_fn` | Inertie unipolaire (valeur 0..max), même mécanique ; `neutralBand` snap |
| `cb_dir` | `cb_dir_fn` | Détecteur de direction (bipolaire → `DRIVE_STATE_BUS`) — observer, ne modifie pas `value` |
| `cb_brake` | `cb_brake_fn` + `cb_rev_brake_fn` | Injection frein L2 + frein de direction inversée ; side-write `BRAKE_BUS` + `extBrakeSteps` |
| `cb_cruise` | `cb_cruise_fn` + `cb_cruise_sync_fn` + `cb_cruise_upd_fn` | Régulateur de vitesse : mode `holdSpeed` (SUBGEAR_BUS) + mode normal (CRUISE_ACTIVE) |

### `modules/gear/`
| Fichier | Fonctions | Rôle |
|---------|-----------|------|
| `gear_fsm` | `gear_fsm_update()` | FSM pure N rapports (RPM → rapport) — zéro dépendance ComBus, unit-testable |
| `cb_gear` | `gear_fsm_fn`, `gear_ratio_fn`, `gear_ratio_inv_fn`, `gear_subgear_cap_fn`, `gear_dir_fn`, `gear_dyn_ramp_fn`, `gear_upshift_damp_fn` | Wrappers CbProc : boîte de vitesses virtuelle complète |

---

## 2. Travaux en suspens — winter 2026


### 2a. Combus processor INPUT formating

**Contexte :**
Actuellement, aucune structure (convention) n'est définie pour les valeurs d'input brute analogique/digital.
Chaque input device importe ses données "à sa sauce" et les chaines de processeurs combus en patissent 
Il s'avère donc qu'un préformatage normé des inputs soit nécessaire pour garder un cohérence dans le projet.


**Proposition d'implémentation :**
Le souci est donc "input device focus", et doit faire partie de la config du device plutôt que de traiter ça au niveau véhicule. Une phase préalable de préprocessing signal est donc indispensable.

Je propose donc de :
- 1. D'ajouter' un champ de config de préprocessing input devices dans src\core\config\inputs\device.cpp
  -> ex : un pointeur vers une fonction simproc "lite" par input (fonction sans paramètre, nullptr "passthrough" si besoin)
- 2. Conserver la seconde phase de input proc côté machine (pour permettre un processing spécifique "machine combus side")
- 3. Exécuter les processing simulation actuel
- 4. Ajouter un formateur de données de sorties pour les combus de pilotage hardware

Après plusieur test, il s'avère que l'encodage suivant sera le plus afficace :
- Digital : actif = 1, inactif = 0
- Analogique :
  -> 0 à combusMaxVal/2 + un bit de sens (back/fv) pour le bi-directionel (encodage à convenir).
  -> idem pour une valeur uni-directionelle, mais avec un bit de sens constant (bloqué à 0 ou 1 par convention)


**note :**
- L'implémentation de l'encodage analogique (c.f. core/system/combus/combus_acces.h - CbAnalog) est ébauchée. Vérifier la structure de codage (bitset) pour une efficacité de traitement optimale des proc de formatage.
- Les processors déjà implémentés devront être modifiés pour satifaire à ce nouvel encodage analogique


### 2b. `GEAR_SHIFTING` + `BREAK` → canal WIRE (winter 2026 — ComBus v2)

**Contexte :** `GEAR_SHIFTING`et `BREAK` sont actuellement machine-local digital.
Doivent être promu en canaux WIRE avant `WIRE_END` pour que le sound node puisse le lire
directement (remplace la détection heuristique dans le FSM son).

**Voir :** `combus_ids.h` → `GEAR_SHIFTING` + `@todo` en place.

### 2c. Hydraulique — motion preset manquant

`kMotion_Hydraulic_Slow` n'a jamais été créé.
Trivial : `MotionRamp { .rampTimeMs = 200u, ... }` dans `simulation_presets.h`.
Créer quand un canal hydraulique utilisera la chaîne motion.

---

## 3. Règles d'ajout d'un processor

1. **Un processor = une responsabilité.** Si la logique implique un FSM multi-états
   ou plusieurs fonctions cohérentes → créer un module dans `modules/`.

2. **Fichiers :** `<name>.h` + `<name>.cpp` dans le sous-dossier approprié.
   Struct séparée dans `include/struct/combus/processors/<folder>/<name>_struct.h`
   si elle dépasse quelques champs.

3. **Signature :** `void cb_<name>_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner chainOwner)`

4. **Observer vs. mutateur :**
   - Observer (side-write uniquement) → ne pas modifier `value`, utiliser `proc->outValue` + `proc->outCh`.
   - Mutateur → modifier `value` directement.

5. **Pas de `#include` Arduino ni hardware** dans un processor.
   Exception : `millis()` pour les timers de ramp (`#include <Arduino.h>` autorisé).

6. **Config `const`, state RAM :**
   - `proc->cfg` → pointeur vers `constexpr` struct en flash.
   - `proc->state` → pointeur vers variable RAM initialisée à `{}`.
   - `proc->dynCfg` → RAM mutable (modifiable par une proc précédente dans la chaîne).
