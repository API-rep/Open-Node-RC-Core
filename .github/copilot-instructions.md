# Copilot Session Rules
!!!! A modifier suivant rework doc!!!!
> Chargé à chaque session. Lire §0 puis §1.
Pour les détails d'architecture

## 0. Orientation rapide

**Projet :** firmware ESP32 multi-nœuds pour véhicules RC de chantier (modélisme) + remote control.  
Deux nœuds : **machine node** (input PS4 + simulation + drivers) et **sound node** (ComBus RX + moteur son). Les deux noeuds ont un vocation à être fusionner. Garder une structure "mirror" entre les deux pour faciliter l'opération.

Build : PlatformIO `espressif32@6.7.0` (IDF 4.4). Env actif : `volvo_A60H_bruder`.

---

## 1. WIP — état à date (2026-06-10)

### En cours / sessions récentes (à completer en fin de session) :

| Session | Sujet | État |
|YYYY-MM-DD | Sujet de la session | En cours / terminé

---

## 2. Winter 2026 — Roadmap

**Architecture (winter 2026 — ne pas commencer avant fin saison) :**
7. `TRACTION_BUS` packed + `GEAR_SHIFTING` wire — ComBus v2 (voir `doc/deferred_features.md §2–4`)
8. `SimDev` + `SrvDevType::ESC_RC` (voir `doc/deferred_features.md §1`)
9. `SoundDevice` table-driven — après DiYGuy removal (voir `doc/deferred_features.md §5`)
10. Excavator / Loader combus + motion + inputs_map (stubs en place)
11. Migration RPM-primary motion model (voir `doc/deferred_features.md §8`)

---

## 3. Fichiers de référence

| Fichier | Contenu |
|---------|---------|
| `doc/code_style_synthesis.md` | Style C++ du projet — **lire avant tout code** |
| `doc/template_module.h` / `.cpp` | Templates fichiers — **lire avant tout nouveau fichier** |
| `doc/deferred_features.md` | Toutes les features différées avec struct et plan de migration |
| `src/core/system/combus/processors/README.md` | Inventaire des processors + travaux en suspens |
| `src/core/system/combus/processors/modules/README.md` | Convention modules vs processors |

---

## 4. Avant de coder — obligatoire

Avant de créer ou modifier du code C/C++ :
1. Lire `doc/code_style_synthesis.md`
2. Lire `doc/template_module.h` et `doc/template_module.cpp`
3. Si le fichier modifié appartient à une architecture documentée → lire la section concernée dans ...

Si un fichier généré ne correspond pas au style documenté, corriger avant de finaliser.

---

## 5. Règles de session

### Convention de commit
```
<scope> (<type>): <short description>
```
Types : `feat` / `fix` / `refactor` / `chore` / `docs` / `test`  
Scopes : `hw_init`, `input`, `combus`, `bt`, `drv`, `srv`, `vbat`, `debug`, `motion`, `sound`, `sim`  
Description : < 72 caractères, minuscules, sans point final.

### Debug flags
Utiliser uniquement les flags partagés : `DEBUG_INPUT`, `DEBUG_HW`, `DEBUG_SYSTEM`, `DEBUG_COMBUS`, `DEBUG_ALL`.  
Pas de flag ad-hoc par module. Centralisation dans `src/core/system/debug/debug.h`.

### `static_assert` placement
- Constantes d'un seul header → dans ce header.
- Croise deux chaînes d'include distinctes → dans le `.cpp` init où les deux chaînes sont résolues.

### Changements — politique
- Édits minimaux et focalisés. Ne pas reformater du code non touché.
- Cohérence avec le style existant du fichier.

### Checklist fin de session
1. Proposer un message de commit (convention ci-dessus).
2. Si build exit 0 valide : *"Le dernier build (Xs, exit 0) est valide — tu peux uploader directement."*
3. PS4 lib check (voir ci-dessous).


### PS4_Controller_Host — vérification périodique
Fork `API-rep/PS4_Controller_Host` @ `b58a05d` — fix BT init IDF 4.4.  
Projet **bloqué sur IDF 4.4** (`espressif32@6.7.0`) : `ps4.c` SPP incompatible IDF 5.x.  
En fin de session ou avant upgrade platform : vérifier si `pablomarquez76/PS4_Controller_Host` upstream
inclut la compatibilité IDF 5.x dans `ps4.c`/`ps4_spp.c`. Si oui → évaluer migration platform.
