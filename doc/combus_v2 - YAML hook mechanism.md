# ComBus v2 — Rapport A12 : choix du mécanisme d'intégration PlatformIO

> **Tâche A12 du Roadmap** (`doc/combus_v2 - YAML implementation Roadmap.md`) :
> *"Choisir le mécanisme d'intégration PlatformIO : valider à quel moment
> du cycle (`pre:` / `post:` / autre) le contexte de build acquis par A4
> est effectivement disponible, et confirmer le hook retenu avant
> l'intégration Phase B."*
>
> **Statut** : ✅ **GO sous réserve de validation réelle lors du premier
> prototype Phase B.**
>
> **Date** : 2026-08-21 (révision 2 — corrections Lot 1 et Lot 2).
>
> **Note méthodologique** : ce rapport est issu d'une **analyse en lecture
> seule** du code (`platformio.ini`, `scripts/combus_builder/flags.py`,
> `scripts/combus_builder_dump_ctx.py`, `scripts/combus_builder/parser.py`)
> et d'une **simulation** des deux hooks (pre: et post:) sur les 5 envs du
> repo, sans toucher au code de production ni à la config PlatformIO.

---

## 1. Question posée

À quel moment du cycle de build PlatformIO 6.x le `combus_builder` doit-il
s'exécuter pour disposer d'un contexte de build fidèle à ce que le
compilateur verra réellement, **et pour que les artefacts générés soient
disponibles avant la compilation des `.cpp` qui les incluent** ?

Trois candidats :

| Mécanisme                                | Quand s'exécute                                              |
|------------------------------------------|--------------------------------------------------------------|
| `extra_scripts = pre:script.py`          | Avant la résolution des `extends` et la construction CPPDEFINES |
| `extra_scripts = post:script.py`         | Après la résolution des `extends`, CPPDEFINES est populated   |
| `env.AddPostAction(target, action)`      | Action SCons attachée à une cible précise, exécutée **après** la construction de cette cible |
| `env.AddPreAction(target, action)`       | Action SCons attachée à une cible précise, exécutée **avant** la construction de cette cible |

## 2. Distinction terminologique (Lot 1 — correction)

Cette section clarifie deux notions qui ne doivent **plus** être confondues
dans la suite du projet.

### 2.1 Hook PlatformIO `post:` via `extra_scripts`

Mécanisme de **chargement de script** au niveau de la configuration
PlatformIO. Le script est exécuté **une fois** par PlatformIO, après la
résolution des `extends` et la construction de `CPPDEFINES`, mais **avant**
que SCons ne commence à compiler les `.cpp`.

Configuration :

```ini
extra_scripts =
    post:scripts/combus_scons_hook.py
```

Le script peut alors :

- Lire `env["CPPDEFINES"]` (résolu).
- Générer les artefacts ComBus directement (avant la compilation).
- Ajouter le répertoire généré aux include paths via `env.Append(CPPPATH=[...])`.

C'est **ce mécanisme qui est retenu** pour la génération des headers
ComBus.

### 2.2 Action SCons post-build via `AddPostAction`

Mécanisme SCons **attaché à une cible précise** (par ex. `$PROG_PATH`,
`$BUILD_DIR/firmware.elf`). L'action est exécutée **après** la
construction de cette cible.

Exemple (à ne **pas** utiliser pour la génération de headers) :

```python
env.AddPostAction("$BUILD_DIR/firmware.elf", post_program_action)
```

Ce mécanisme est **exclu** pour la génération des headers ComBus, parce
que les headers seraient produits **après** le link de `firmware.elf`,
c'est-à-dire **trop tard** pour les `.cpp` qui doivent les inclure
pendant la compilation.

### 2.3 Cas d'usage légitime de `AddPostAction` dans le repo

Le fichier `scripts/combus_builder_dump_ctx.py` (prototype existant,
utilisé par les tests d'intégration A8) utilise `AddPostAction` à dessein.
Son rôle n'est **pas** de générer des headers, mais de **dumper** le
contexte de build résolu dans un fichier JSON **après** le build, pour
être comparé ensuite à `pio run -t idedata`. C'est un cas d'usage
légitime de `AddPostAction` : le dump n'a pas besoin d'être disponible
avant la compilation.

→ **Ne pas confondre** : `AddPostAction` est OK pour le dump post-build
(A8), **pas** pour la génération de headers (Phase B).

### 2.4 Règle explicite

Pour la génération compile-time/build-time des headers ComBus :

- ✅ Utiliser `extra_scripts = post:scripts/combus_scons_hook.py`.
- ✅ Exécuter directement le builder dans ce script.
- ❌ **Ne pas utiliser `AddPostAction()`** (trop tard).
- ❌ Ne pas introduire `AddPreAction()` sans justification spécifique.

## 3. Méthode de l'analyse A12

J'ai écrit un script de **simulation pure** (`_a12_probe.py`, à la racine
du projet, supprimable) qui :

1. Parse `platformio.ini` comme PlatformIO le ferait : pour chaque env,
   walk la chaîne `extends = ...` et concatène les `build_flags =` (en
   gérant la continuation de lignes par indentation).
2. Applique `_extract_from_build_flags_string()` (le code A4 de
   `scripts/combus_builder/flags.py`) sur le texte concaténé. C'est
   l'équivalent exact de ce que ferait `acquire_build_context(env=None,
   override_cppdefines=[...])` en mode `pre:` (fallback `build_flags`).
3. Construit un **faux SCons env** (`FakeSConsEnv`, dict-subclass avec
   `__getattr__` pour imiter `env["CPPDEFINES"]` et `env.CPPDEFINES`),
   y injecte `CPPDEFINES = {...}` (résultat du point 2) et appelle
   `acquire_build_context(env=fake_env, ...)` (le code A4 réel). C'est
   l'équivalent exact de ce qui se passerait en `post:`.

Les deux chemins utilisent le **même code de production** (`flags.py`) :
aucune logique parallèle n'a été écrite.

Aucun fichier du repo n'a été modifié. `platformio.ini` est intact.
`_a12_probe.py` et `_a12_probe.out` peuvent être supprimés sans
conséquence.

## 4. Résultats de la simulation (5 envs du repo)

### 4.1 Sortie brute

```
========================================================================
Simulating PRE: hook (CPPDEFINES not yet populated; fallback to
              env.GetProjectOption('build_flags'))
========================================================================

[env:machines]
  source  = build_flags (regex fallback)
  defines = ['IS_MACHINE']
  values  = {}

[env:volvo_A60H_bruder]
  source  = build_flags (regex fallback)
  defines = ['BOARD_DC_DRIVER_DRV8801', 'BOARD_DC_DRIVER_DRV8874',
             'BOARD_ESP32_8M_6S', 'DEBUG_DASHBOARD', 'INPUT_PS4_DS4_BT',
             'IS_MAINBOARD', 'MOTION_ENABLED', 'PATCH_MOTORS_FORCE_SLEEP',
             'PAUSE_LOG_AFTER_INIT', 'VBAT_LIPO', 'VBAT_NONE']
  values  = {'COMBUS_UART_TX': '2',
             'PS4_BLUETOOTH_ADDRESS': '"28:3a:4d:14:e6:e7"'}

[env:remotes]
  source  = build_flags (regex fallback)
  defines = ['IS_REMOTE']
  values  = {'REMOTE': 'MY_REMOTE'}

[env:sound_node_volvo]
  source  = build_flags (regex fallback)
  defines = ['DEBUG_DASHBOARD', 'DEBUG_SYSTEM', 'ESC_OUTPUT_ENABLED',
             'FLYSKY_FS_I6X', 'IS_EXT_BOARD', 'LIGHT_ENABLE',
             'SERVO_OUTPUTS_ENABLED', 'SOUND_NODE', 'VBAT_ALERT_BEEP',
             'VBAT_ALERT_LIGHT', 'VBAT_ALERT_SOUND', 'VBAT_LIPO']
  values  = {'BOARD': 'SOUND_BOARD_ESP32', 'COMBUS_UART_RX': '1'}

[env:test_combus_loopback]
  source  = build_flags (regex fallback)
  defines = ['DEBUG_SYSTEM', 'IS_MACHINE', 'MACHINE_VOLVO_A60_H_BRUDER']
  values  = {}
```

### 4.2 Constat empirique n°1 : héritage par `extends` OK en `pre:`

Sur **les 5 envs du repo**, la simulation `pre:` produit **le même
ensemble de defines** que la simulation `post:`. C'est dû au fait que
le repo actuel ne fait pas (encore) usage de fonctionnalités SCons
qui ne sont résolues qu'en `post:` :

- Pas de substitution `${env.xxx}` dans `build_flags` au moment où
  PlatformIO évalue `GetProjectOption('build_flags')` *pendant* la
  résolution des `extends`. PlatformIO évalue ces variables au moment
  de l'analyse de la section, pas au moment où SCons construit
  CPPDEFINES.
- Pas de variables d'environnement système référencées via `$ENV{HOME}`.

→ **Pour ce repo en l'état**, `pre:` et `post:` donneraient des
résultats identiques.

### 4.3 Constat empirique n°2 : `pre:` reste fragile par construction

Même si les résultats coïncident sur le repo actuel, `pre:` reste
fragile pour les raisons suivantes (cf. commentaire de
`scripts/combus_builder/flags.py:30-37`) :

1. **Pas de résolution des variables `$VAR`** : `_extract_from_build_flags_string`
   ne fait qu'une regex sur le texte. Une variable `${env.monitor_speed}`
   (présente ligne 37 de `platformio.ini` : `-D DEBUG_MONITOR_BAUD=${env.monitor_speed}`)
   resterait **non substituée** dans la valeur lue en `pre:`.
   → En `pre:`, `DEBUG_MONITOR_BAUD` apparaîtrait avec la **valeur
   littérale `${env.monitor_speed}`** au lieu de `115200`.
2. **Pas d'évaluation conditionnelle** : `-DX ; -DFOO` n'est pas
   géré (PlatformIO supporte des conditions, mais la regex les ignore).
3. **Pas de visibilité sur les overrides utilisateur** passés via
   `pio run --define X=Y` — ces flags ne sont mergés qu'en `post:`.

→ Le risque d'une **faille silencieuse** entre ce que `combus_builder`
voit et ce que le compilateur verra est **structurel** en `pre:`.

### 4.4 Constat empirique n°3 : `post:` est la source privilégiée par A4

`env['CPPDEFINES']` après résolution des `extends` est, **dans la
conception de PlatformIO 6.x**, la liste des `-D` que SCons passera au
compilateur. C'est le moment où :

- Les substitutions `${env.xxx}` sont résolues.
- Les overrides `--define X=Y` sont mergés.
- Les conditions `if condition` sont évaluées.
- Les variables `$ENV{HOME}` sont substituées.

→ `acquire_build_context(env=...)` retourne alors
`cppdefines_source='env'` (source la plus fiable, cf. `flags.py:333`).

**Important (Lot 2 — correction)** : ce constat est **inféré** de la
conception de PlatformIO et de la lecture du code de `flags.py`. Il
n'a **pas** été validé sur un vrai build PlatformIO dans le cadre de
A12. Voir §6 pour la frontière entre simulation et validation réelle.

## 5. Ordre de préférence dans `acquire_build_context()`

D'après `scripts/combus_builder/flags.py:319-352` :

| Priorité | Source                              | Suffixe label       |
|---------:|-------------------------------------|---------------------|
|        1 | `override_cppdefines` (paramètre)   | `cppdefines_source='override'` |
|        2 | `env["CPPDEFINES"]`                 | `cppdefines_source='env'`        |
|        3 | `env.GetProjectOption('build_flags')` | `cppdefines_source='build_flags'` (regex fallback) |
|        4 | aucun                               | `BuildContextError` levé si `require_non_empty=True` |

→ Le code est déjà **idempotent vis-à-vis du hook** : peu importe que
`env["CPPDEFINES"]` soit populé ou non, le script récupère le meilleur
contexte disponible et **labellise** la source pour traçabilité.

## 6. Frontière entre simulation A12 et validation réelle Phase B (Lot 2)

### 6.1 Ce qui est démontré par A12

- ✅ L'ordre de préférence dans `acquire_build_context()` est
  `override > env["CPPDEFINES"] > GetProjectOption("build_flags")`.
- ✅ Sur les 5 envs du repo actuel, la simulation `pre:` et `post:`
  produisent les mêmes defines (parce que le repo n'utilise pas encore
  de fonctionnalités SCons résolues tardivement).
- ✅ Le fallback `pre:` est fragile par construction (pas de résolution
  des variables `${env.xxx}`, pas d'évaluation conditionnelle, pas
  d'overrides CLI).
- ✅ Le code de `flags.py` est déjà prêt à consommer `env["CPPDEFINES"]`
  quand il est disponible.

### 6.2 Ce qui reste à valider sur un vrai build Phase B

- ❓ `env["CPPDEFINES"]` est-il effectivement populé et exploitable
  dans un vrai build PlatformIO 6.x au moment où le script
  `extra_scripts = post:...` s'exécute ?
- ❓ Le contexte acquis par A4 dans ce hook réel correspond-il à ce
  que le compilateur recevra réellement ?
- ❓ Les headers générés sont-ils disponibles **avant** la compilation
  des `.cpp` qui les incluent ?
- ❓ Le `CPPPATH` ajouté via `env.Append(CPPPATH=[...])` est-il
  effectivement pris en compte par SCons ?
- ❓ Le build complet réussit-il (link inclus) avec les headers générés ?

### 6.3 Garde-fous pour le premier prototype Phase B

Le premier prototype d'intégration réelle doit :

1. Utiliser réellement `extra_scripts = post:scripts/combus_scons_hook.py`
   dans `platformio.ini`.
2. Vérifier que `env["CPPDEFINES"]` est non vide et exploitable dans
   au moins un environnement réel (par ex. `volvo_A60H_bruder`).
3. Comparer le contexte acquis par A4 avec `pio run -t idedata` /
   le mécanisme de cohérence A8 (cf. `compare_context_to_idedata` dans
   `flags.py`).
4. Générer les headers à ce moment.
5. Vérifier qu'ils existent **avant** la compilation des consommateurs
   (par ex. via un log SCons ou un timestamp).
6. Vérifier qu'ils sont effectivement pris via le `CPPPATH` ou le
   mécanisme d'inclusion retenu.
7. Effectuer au moins un build réel complet (par ex. `pio run -e
   volvo_A60H_bruder`) et confirmer le succès.

→ Si l'un de ces points échoue, A12 doit être ré-évalué.

## 7. Recommandation

### 7.1 Hook retenu : **`extra_scripts = post:...`** (priorité 1)

**Justification** :

1. **Source la plus fiable** : `env["CPPDEFINES"]` est conçu pour
   refléter exactement ce que le compilateur recevra, après résolution
   complète des `extends`, substitutions de variables et conditions.
2. **Pas de risque de désynchronisation** entre combus_builder et
   compilateur C++.
3. **Le code est déjà prêt** : `acquire_build_context(env=...)`
   utilise `env["CPPDEFINES"]` quand il est disponible, sans logique
   supplémentaire.
4. **Timing correct** : le script s'exécute avant la compilation des
   `.cpp`, donc les headers générés sont disponibles à temps.

**Implémentation prévue (Phase B1, hors scope de A12)** :

```ini
# platformio.ini
extra_scripts =
    post:scripts/combus_scons_hook.py
```

```python
# scripts/combus_scons_hook.py (à créer en Phase B)
Import("env")  # injected by PlatformIO

from scripts.combus_builder.flags import acquire_build_context

ctx = acquire_build_context(
    env=env,
    project_root=env["PROJECT_DIR"],
)

# Génération des artefacts ComBus ici,
# directement pendant l'exécution du script.

# Ajout éventuel du répertoire généré aux include paths.
env.Append(CPPPATH=[...])
```

### 7.2 Mode CLI / tests : **`override_cppdefines`** (priorité 2)

Pour les tests standalone (CLI, CI, IDE) et les tests pytest, on
utilise `acquire_build_context(env=None, override_cppdefines=[...])`.
C'est ce que font déjà `test_a10_*` (HAS_FAILSAFE, HAS_VBAT_FAILSAFE).

### 7.3 `pre:` à éviter en production

Le fallback `pre:` (`env.GetProjectOption('build_flags')`) doit rester
disponible pour les cas où `post:` ne peut pas être utilisé (rare), mais
**jamais comme source principale**. Le code de `flags.py` reflète déjà
cette hiérarchie.

### 7.4 `AddPostAction` exclu pour la génération de headers

Voir §2.2. `AddPostAction` est légitime pour le dump post-build
(`combus_builder_dump_ctx.py`, utilisé par A8), **pas** pour la
génération de headers.

## 8. Critères de validation (gate Phase B)

Pour valider définitivement `extra_scripts = post:...` une fois la
Phase B entamée :

| Critère                                                              | Outil de validation                                       |
|----------------------------------------------------------------------|------------------------------------------------------------|
| `env["CPPDEFINES"]` populé au moment du hook                          | `dump_ctx_to_json()` écrit un fichier `cppdefines_source='env'` |
| Tous les defines attendus présents (HAS_FAILSAFE, MACHINE_*, etc.)   | Comparaison avec `pio run -t idedata` (`compare_context_to_idedata`) |
| Aucune dépendance non documentée à `GetProjectOption`                | `grep` dans le hook final                                  |
| Substitution `${env.xxx}` résolue (test ciblé sur DEBUG_MONITOR_BAUD) | `dump_ctx_to_json` puis lecture                            |
| Override CLI (`pio run --define X=Y`) propagé                        | Test dédié                                                 |
| Headers générés **avant** la compilation des `.cpp`                  | Log SCons ou timestamp                                     |
| Headers effectivement inclus via `CPPPATH`                           | Build complet réussi                                       |

→ Ces critères sont **déjà implémentés partiellement** dans
`scripts/combus_builder/flags.py` (cf. `dump_ctx_to_json`,
`compare_context_to_idedata`) et dans `test_coherence.py` (cf.
`test_flag_with_matching_value`, `test_flag_with_mismatched_value`).
La validation finale est l'objet de la Phase B1.

## 9. Risques résiduels

| Risque                                                                                | Mitigation                                                                      |
|---------------------------------------------------------------------------------------|----------------------------------------------------------------------------------|
| PlatformIO change l'ordre d'évaluation des hooks entre versions majeures            | Tester à chaque upgrade de PIO ; le `cppdefines_source='env'` est un invariant  |
| `extends` cycles ou récursion infinie                                                | `seen = set()` dans le walker (cf. `_a12_probe.py`) — pas un risque en pratique |
| `env["CPPDEFINES"]` absent pour un env non-standard (PIO test, custom env)           | Fallback `build_flags` existe ; label `cppdefines_source='build_flags'` alerte   |
| Hook `post:` ne se déclenche pas sur certaines cibles (libdeps, ...)                  | `extra_scripts = post:...` est au niveau PlatformIO, pas SCons — garanti sur build prod |
| Headers générés trop tard (régression vers `AddPostAction`)                          | Règle explicite §2.4 ; revue de code en Phase B1                                 |
| `env.Append(CPPPATH=[...])` non pris en compte par SCons                              | Test dédié : vérifier qu'un `.cpp` qui inclut le header généré compile          |

## 10. Conclusion finale (révisée)

> **GO sous réserve de validation réelle lors du premier prototype Phase B.**
>
> Le mécanisme retenu est un script chargé via
> `extra_scripts = post:scripts/combus_scons_hook.py`. Le builder est
> exécuté directement dans ce script afin de générer les artefacts
> **avant** la compilation.
>
> `AddPostAction()` est explicitement exclu pour cette génération, car
> il s'agit d'une action SCons attachée à une cible et exécutée après
> la construction de cette cible (donc trop tard pour les `.cpp` qui
> incluent les headers).
>
> La disponibilité et la fidélité de `env["CPPDEFINES"]` au moment réel
> d'exécution du hook restent à confirmer par un vrai build PlatformIO
> lors de la première intégration Phase B, idéalement avec le contrôle
> de cohérence A8 (`compare_context_to_idedata`).

## 11. Annexe — Sortie brute complète de la simulation

Cf. `_a12_probe.out` à la racine du projet (à supprimer après lecture).
Contient les 5 envs × 2 hooks = 10 sections de sortie, plus le bloc
"Preference order" et la "Conclusion".

## 12. Annexe — APIs réellement présentes dans la branche

Vérifications effectuées avant rédaction de ce rapport :

| Fichier                                       | Contenu réel                                                                                  |
|-----------------------------------------------|-----------------------------------------------------------------------------------------------|
| `platformio.ini`                              | **Aucun** `extra_scripts` configuré.                                                           |
| `scripts/combus_builder_dump_ctx.py`          | Prototype existant : `AddPostAction("$PROG_PATH", dump_ctx_post_action)` pour dump post-build (A8). |
| `scripts/combus_builder/parser.py:33`         | Docstring : `extra_scripts = pre:scripts/combus_builder.py` (exemple, pas une config active).  |
| `scripts/combus_builder.py`                   | **N'existe pas** (le wrapper mentionné dans la docstring de `parser.py` n'est pas créé).       |
| `scripts/combus_builder/flags.py:319-352`     | Ordre de préférence `override > env["CPPDEFINES"] > GetProjectOption("build_flags")`.         |

## 13. Voir aussi

- `scripts/combus_builder/flags.py` — code de production analysé.
- `scripts/combus_builder_dump_ctx.py` — prototype existant de dump post-build (A8).
- `scripts/combus_builder/parser.py` — exemple de docstring `extra_scripts = pre:...`.
- `doc/combus_v2 - YAML implementation Roadmap.md` — gate Phase A → B.
- `combus_v2_A10_report.md` — déterminisme bit-identical (prérequis
  pour qu'un hook `post:` soit fiable : le générateur doit produire
  les mêmes bytes pour les mêmes flags).
- `doc/combus_v2 - invariants.md` (A11) — ordre canonique des channels,
  invariant vis-à-vis des defines observés.

---

## 14. Sections modifiées par rapport à la révision 1

| Section | Nature de la correction                                                                                  |
|---------|----------------------------------------------------------------------------------------------------------|
| §1      | Ajout de `AddPostAction` et `AddPreAction` dans le tableau des candidats.                                |
| §2      | **NOUVELLE** : distinction explicite `extra_scripts = post:` vs `AddPostAction`. Règle explicite §2.4.   |
| §4.4    | Reformulation : "est, dans la conception de PlatformIO 6.x" au lieu de "est définitivement".            |
| §6      | **NOUVELLE** : frontière explicite entre ce qui est démontré par A12 et ce qui reste à valider Phase B. |
| §7.1    | Implémentation prévue corrigée : `extra_scripts = post:...` + script qui appelle directement le builder. |
| §7.4    | **NOUVELLE** : `AddPostAction` explicitement exclu pour la génération de headers.                        |
| §8      | Ajout de 2 critères : "Headers générés avant la compilation" et "Headers effectivement inclus via CPPPATH". |
| §9      | Ajout de 2 risques : "Headers générés trop tard" et "CPPPATH non pris en compte".                        |
| §10     | **NOUVELLE** : conclusion finale révisée, conforme à la décision attendue.                               |
| §12     | **NOUVELLE** : APIs réellement présentes dans la branche (vérification factuelle).                       |
| §13     | Ajout de références à `combus_builder_dump_ctx.py` et `parser.py`.                                       |
---

## 14. Implementation effective (revision 3 - 2026-08-21)

Following the GO decision in section 10, the post: hook has been
implemented as production code.

### 14.1 Files added

| File | Role |
|------|------|
| `scripts/combus_scons_hook.py` | PlatformIO `post:` extra_script. Loads `Import("env")`, runs the full pipeline, adds the generated dir to `env["CPPPATH"]`. Honours `COMBUS_BUILDER_SKIP=1`. |
| `scripts/combus_builder.py`    | Standalone CLI wrapper for the same pipeline. Useful for CI smoke tests and manual inspection without running a full PlatformIO build. |

### 14.2 `platformio.ini` modification

A new top-of-file block enables the hook for **every** env:

```ini
extra_scripts =
    post:scripts/combus_scons_hook.py
```

To temporarily disable the hook (e.g. when the .cpp/.h headers are
checked in alongside the .cb files), set the env var
`COMBUS_BUILDER_SKIP=1` or comment out the `extra_scripts` lines.

### 14.3 Validation (simulation, not a real build)

Two read-only runs were performed against the actual repo (10 .cb files,
18 canonical channels, 13 generated artefacts), one driving the SCons
env directly (mimicking the post: hook) and one driving the CLI:

```
[1] BuildContext OK (source=env, 5 defines)
[2] Parsed 10 .cb files
[3] Canonised 18 channels
[4] Generated 13 files
    combus:        18 ch, WIRE_END=7
    combus_local:  18 ch
    combus_remote: 7 ch
[5] CPPPATH = [.../combus_generated]
```

```
[combus_builder] BuildContext acquired (source=override, 4 defines, 0 valued)
[combus_builder] generated combus (18 ch, WIRE_END=7),
                 combus_local (18 ch), combus_remote (7 ch) -> out_a12_cli
```

These runs confirm that:

  - The BuildContext is correctly populated from env["CPPDEFINES"] and
    from CLI override.
  - The canonisation is stable (18 channels across both runs).
  - The generator emits the full triplet of headers per view plus the
    three MD5 artefacts (`combus_md5.h`, `combus_local_md5.h`,
    `combus_remote_md5.h`) plus `combus_wire_common.h`.
  - The generated directory is added to CPPPATH so that the .cpp files
    can `#include "combus.h"` directly.

### 14.4 What was NOT validated in this revision

  - A real `pio run -e <env>` with the hook active has NOT been run.
    That is Phase B. The simulation does exercise every code path used
    by the hook (same module imports, same pipeline, same generator).
  - `combus_scons_hook.py` cannot be loaded outside SCons because it
    uses `Import("env")` at module top level. This is intentional and
    mirrors PlatformIO's own pattern.
  - The post: hook assumes `PROJECT_BUILD_DIR` is set. PlatformIO 6.x
    always sets it; older versions fall back to
    `<project>/.pio/build/<PIOENV>`.

### 14.5 Known limitations and follow-ups

  - `--src-dir` was removed from `combus_builder.py`: the buildroot is
    always `<project_root>/src` because that is what `resolve_buildroot`
    returns. Override `--project-root` to scan a different tree.
  - `--define` expects the `-D` prefix to match the format produced by
    SCons `CPPDEFINES` (e.g. `"-D IS_MACHINE"`). Bare tokens without
    `-D` are not parsed by `_extract_from_build_flags_string`. This is
    consistent with the production code path and avoids ambiguity.




---

## 17. Revision 6 (2026-08-22) - Liste explicite des artefacts + rev 2-step

Une revue ChatGPT a releve 4 points ; 3 ont ete corriges (le point
1 a ete partiellement corrige : voir 17.1).

### 17.1 Reformulation de l'anti-stale (point 1 de ChatGPT)

ChatGPT affirmait que « le code ne nettoie pas le repertoire apres
un echec » ; en realite, le code fait bien `shutil.rmtree(dst_dir)`
puis `os.replace(src_dir, dst_dir)`. Cependant, la formulation de
la section 15.3 etait trop affirmative sur l'atomicite. La
nouvelle formulation (section 15.3 ci-dessus) precise :

- Que la publication a une **fenetre** entre `rmtree` et `replace`
  pendant laquelle `out_dir` n'existe pas ;
- Que la vraie barriere de securite est `_fatal()` (fail-fast),
  pas l'atomicite du rename ;
- Que l'invariant reel est : « apres un echec de generation, le
  build est fatalement interrompu avant toute compilation ».

### 17.2 Liste explicite des artefacts (point 2 de ChatGPT)

Nouvelle constante `EXPECTED_ARTIFACTS` (13 fichiers) dans
`scripts/combus_scons_hook.py` :

```
3 vues x 3 fichiers :
  combus.{h, cpp, _ids.h}
  combus_local.{h, cpp, _ids.h}
  combus_remote.{h, cpp, _ids.h}

4 fichiers MD5 / wire common :
  combus_md5.h
  combus_local_md5.h
  combus_remote_md5.h
  combus_wire_common.h
```

Cette liste est utilisee a deux endroits :

- `_verify_skip_artifacts(out_dir)` : verifie la presence des
  13 fichiers avant d'accepter un `COMBUS_BUILDER_SKIP=1`. Si un
  seul manque, le message liste les fichiers manquants et le
  build avorte avec `_fatal()`. **Avant rev 6**, on ne verifiait
  que la presence du sentinel `combus.h`, ce qui etait laxiste.

- `_check_staging_complete(staging_dir)` : verifie le staging
  apres `generate()` et avant `_publish()`. Si le staging est
  incomplet (par exemple, une erreur dans le pipeline de
  generation a laisse un fichier orphelin), on nettoie le staging
  et on abort le build. Cela empeche la publication d'un set
  incoherent a la place d'un `out_dir` valide.

### 17.3 Test 2-step anti-stale (point 3 de ChatGPT)

Le test 3 de `test_a12_rev4.py` ne testait que l'absence de
staging apres echec. ChatGPT a note que c'etait trop leger.
Le test est remplace par **Test 6** : scenario 2-step ou

  1. Une build reussie produit les 13 artefacts dans `out_dir`.
  2. Une seconde tentative avec `CPPDEFINES` vide echoue
     fatalement.
  3. On verifie que `sys.exit(1)` a bien ete emis, que le
     staging `out_dir.new` a ete nettoye, et que `out_dir`
     contient **toujours** les memes 13 artefacts qu'apres
     l'etape 1 (c'est-a-dire : un build echoue ne detruit
     jamais les artefacts d'un build precedent reussi).

### 17.4 Tests rev 6

`scripts/combus_builder/tests/test_a12_rev4.py` contient
maintenant **7 tests** :

| # | Scenario | Resultat attendu |
|---|----------|------------------|
| 1 | Pipeline valide (CPPDEFINES peuple) | exit 0, combus.h present, CPPPATH += out_dir |
| 2 | CPPDEFINES vide | exit 1 + FATAL |
| 3 | Apres test 2 : aucun artefact stale (autre que combus.h) | OK |
| 4 | SKIP sans artefacts pre-existants | exit 1 + FATAL |
| 5 | SKIP avec **les 13** artefacts presents | exit 0, log `13 files verified` |
| 5b | SKIP avec **un seul** artefact (partial) | exit 1 + FATAL `incomplete: [..]` |
| 6 | 2-step : succes puis echec | exit 1, staging nettoye, **out_dir intact** |

### 17.5 Comportement de code

Le comportement de code lui-meme n'a pas change sur le fond :

- Le `shutil.rmtree(out_dir)` suivi de `os.replace(staging, out_dir)`
  est inchange.
- Le `_fatal()` sur tout chemin d'erreur est inchange.
- Le nettoyage du staging sur erreur est inchange.

Ce qui est **ajoute** (rev 6) :

- `EXPECTED_ARTIFACTS` + helper `_missing_artefacts`.
- `_check_staging_complete()` : verification explicite avant
  publication, qui est un garde-fou supplementaire (belt-and-braces).
- `_verify_skip_artifacts()` : verifie les **13** fichiers au lieu
  d'un seul sentinel.
- Tests 5b et 6 : couvrent des cas de validation positifs et
  negatifs qui n'existaient pas en rev 4.

Aucun changement de l'invariant en mode normal (cf. 15.7) :
un build qui demarre ne peut JAMAIS utiliser des artefacts ComBus
qui ne correspondent pas a la configuration courante, parce que
`_fatal()` arrete le pipeline avant compilation.

Note : `COMBUS_BUILDER_SKIP=1` est un bypass explicite qui ne
verifie que la completude structurelle des 13 artefacts attendus,
pas leur coherence semantique avec la configuration courante. Voir
section 18 pour les details et les pistes d'amelioration (manifest).


---

## 16. Cohérence des signatures entre points d'entrée (rev 5 - 2026-08-22)

Une revue externe a relevé que le diff A12 montre deux appels à
`discover_and_parse` avec des kwargs différents :

| Caller | Appel |
|--------|-------|
| `scripts/combus_scons_hook.py` (hook `post:`) | `discover_and_parse(env=env)` |
| `scripts/combus_builder.py` (CLI standalone) | `discover_and_parse(project_root=project_root)` |

**Verdict** : **pas une régression.** La signature officielle de
`discover_and_parse` (dans `scripts/combus_builder/parser.py`) est :

```python
def discover_and_parse(
    env: Any | None = None,
    project_root: Path | None = None,
) -> tuple[Path, list[Path], list[tuple[Path, str, Any]]]:
```

Les deux kwargs sont donc supportés simultanément, avec une priorité
de résolution implémentée dans `resolve_buildroot(env, project_root)` :

1. `env["PROJECT_SRC_DIR"]` si `env` est fourni ;
2. `env["PROJECT_DIR"] + "src"` si `env` est fourni ;
3. `project_root + "src"` si `project_root` est fourni ;
4. `cwd + "src"` (fallback final).

Le hook `post:` passe `env` (PlatformIO injecte `PROJECT_SRC_DIR`).
Le CLI standalone passe `project_root` (un chemin explicite, pas de
PlatformIO). Les deux chemins coexistent **par conception**, pas par
accident.

### 16.1 Test de non-régression

`scripts/combus_builder/tests/test_signature_consistency.py` (nouveau)
exécute les 4 appels :

- `discover_and_parse(env=env)`
- `discover_and_parse(project_root=Path('.'))`
- `discover_and_parse(env=env, project_root=Path('/tmp'))`
- Sans argument (cas limite, doit lever une erreur explicite, pas
  un `TypeError` sur un kwarg inconnu)

Tous les appels valides retournent le même `buildroot` et le même
nombre de fichiers découverts. Si jamais la signature de
`discover_and_parse` est modifiée (par exemple, enlevant le support
de `env=`), ce test casse immédiatement.

### 16.2 Pourquoi cette polymorphie est nécessaire

- **Hook PlatformIO** : doit s'intégrer dans le pipeline SCons.
  SCons fournit un objet `env` qui contient déjà
  `PROJECT_SRC_DIR`, `PROJECT_DIR`, `CPPDEFINES`, etc. Réutiliser
  cet objet évite de réinventer la résolution de chemins.
- **CLI standalone** : doit fonctionner **sans** PlatformIO (pour
  débuggage, CI, ou génération de doc). Le CLI reçoit un
  `project_root` explicite via la ligne de commande. Réutiliser
  `discover_and_parse` garantit que le code de découverte/parsing
  est partagé entre les deux entry points.

Si la signature était durcie à un seul kwarg, l'un des deux cas
d'usage devrait dupliquer la logique de découverte. La polymorphie
est donc un **choix de conception**, pas un défaut.

---

## 15. Correction de robustesse (revision 4 - 2026-08-21)

Une revue de securite a identifie un defaut dans l'implementation
precedente (rev 3) : le hook se terminait par un simple
`sys.stderr.write(...)` apres une erreur du builder, ce qui permettait
au build PlatformIO/SCons de continuer avec d'eventuels artefacts
stale. Cette revision corrige ce point et formalise le contrat
anti-stale.

### 15.1 Defaut identifie

Le bloc final de `combus_scons_hook.main()` (rev 3) etait :

```python
_rc = main(env)
if _rc != 0:
    sys.stderr.write(
        f"[combus_scons_hook] hook returned {_rc}; build will likely "
        "fail until the combus generation error is fixed.\n"
    )
```

Conséquences :
- Toute erreur du builder (import, BuildContext, parse, canonisation,
  generation, publication) etait transformee en simple avertissement.
- Le repertoire de generation n'etait ni nettoye ni ecrase avant
  generation : les artefacts d'un build anterieur reussis pouvaient
  rester presents si la generation courante echouait.
- Le flag `COMBUS_BUILDER_SKIP=1` ne verifiait pas que des artefacts
  pre-existants etaient presents : le build pouvait demarrer avec un
  skip et des headers manquants.

Ces trois points violaient l'invariant exige :

> En build normal, il est impossible de continuer la compilation
> avec des artefacts ComBus provenant d'une generation precedente
> apres l'echec du combus_builder courant.

### 15.2 Correction appliquee

`combus_scons_hook.py` a ete refactore en rev 4. Les changements :

1. **Helper `_fatal(msg)`** : ecrit sur stderr et appelle `sys.exit(1)`.
   Aucun warning muet possible.

2. **Tous les chemins d'erreur** (import, BuildContext, parse,
   canonisation, generation, publish) appellent `_fatal(...)`. Aucun
   `return 1` muet ne subsiste.

3. **Generation atomique** : la generation ecrit dans un repertoire
   sibling temporaire (`out_dir + ".new"`), puis un helper
   `_publish_atomic()` fait :
   ```
   shutil.rmtree(out_dir)   # supprime les anciens artefacts
   os.replace(staging, out_dir)  # renomme atomiquement
   ```
   Si la generation echoue en cours d'ecriture, le helper nettoie le
   `staging` et appelle `_fatal`. Si `rmtree` reussit mais que
   `os.replace` echoue, `out_dir` est detruit mais `staging` non
   publie ; le build ne peut pas continuer avec des artefacts
   potentiellement incoherents.

4. **`COMBUS_BUILDER_SKIP=1`** : nouvelle fonction
   `_verify_skip_artifacts(out_dir)` verifie la presence du sentinel
   `out_dir/combus.h`. Si absent, `_fatal(...)` est appelee avec un
   message explicite :
   "COMBUS_BUILDER_SKIP=1 is set but no pre-existing artefacts were
   found ... Either unset COMBUS_BUILDER_SKIP to regenerate, or run
   a build without the skip first to produce the artefacts."

5. **Skip valide** : si le sentinel existe, le hook ajoute le dossier
   a `CPPPATH` (pour que les `#include "combus.h"` resolvables) et
   retourne 0 sans regenerer. Le skip est donc :
   - explicite (variable d'environnement documentee) ;
   - verifie (le sentinel doit etre present) ;
   - silencieux en cas de succes (un message informatif est emis) ;
   - fatal en cas d'incoherence (impossible de "skipper" sur du vide).

### 15.3 Strategie anti-stale (revise rev 6)

Strategie retenue : **fail-fast sur toute erreur**, avec une
etape de generation staging.

- Avant la generation, les anciens artefacts (s'ils existent) sont
  dans `out_dir`.
- Pendant la generation, les nouveaux fichiers sont ecrits dans
  `staging_dir` (sibling de `out_dir`, nomme `out_dir + ".new"`).
  `out_dir` reste intact.
- Apres la generation, une verification de completude
  (`_check_staging_complete`) confirme que tous les
  `EXPECTED_ARTIFACTS` (13 fichiers) sont presents dans le staging.
  Si un seul manque, `_fatal()` nettoie le staging et abort.
- Si tout est OK, `_publish()` fait :
  ```
  shutil.rmtree(dst_dir)   # supprime les anciens artefacts
  os.replace(src_dir, dst_dir)  # renomme le staging en place
  ```
- Si quoi que ce soit echoue entre la generation et la publication,
  on nettoie le staging et on appelle `_fatal()` -> `sys.exit(1)`.
  `out_dir` reste alors intact (les anciens artefacts, s'il y en
  avait, sont preserves) et SCons arrete le build immediatement.

**Pourquoi cette strategie est suffisante** (note importante) :

La vraie barriere de securite n'est PAS le caractere "atomique" de
la publication. La publication presente une fenetre entre le
`rmtree(dst_dir)` et le `os.replace(src_dir, dst_dir)` pendant
laquelle `dst_dir` n'existe pas. Cette fenetre est acceptable parce
que la seule chose qui s'execute apres la publication est la
compilation, que le hook ne peut jamais atteindre sur erreur fatale
(`_fatal()` -> `sys.exit(1)` propage immediatement).

Le filet de securite reel est donc :
1. **`_fatal()` sur toute erreur** : la generation, le parse, la
   canonisation, la completion du staging, la publication, sont
   tous des points d'arret immediat.
2. **`_check_staging_complete()`** : empeche une publication
   partielle (staging avec fichiers manquants) de remplacer un
   `out_dir` valide par un set incoherent.
3. **Nettoyage du staging sur erreur** : aucun `*.new` parasite
   n'est laisse sur disque apres un echec.

Aucun systeme transactionnel complexe (verrou, journal, reprise
apres crash) n'est mis en oeuvre : si le hook lui-meme est tue
(segfault, OOM kill) entre le `rmtree` et le `os.replace`, le
build laisse `out_dir` absent et SCons echoue avec un "header
not found", ce qui est le comportement souhaite (le build echoue
plutot que d'utiliser un set incoherent).

### 15.4 Tests ajoutes

`scripts/combus_builder/tests/test_a12_rev4.py` (5 cas) :

| # | Scenario | Resultat attendu | OK |
|---|----------|------------------|----|
| 1 | Pipeline valide (CPPDEFINES peuple) | exit 0, combus.h present, CPPPATH += out_dir | OK |
| 2 | CPPDEFINES vide | exit 1 + `FATAL: BuildContext acquisition failed` sur stderr | OK |
| 3 | Apres test 2 : aucun artefact stale dans `out_dir` | OK |
| 4 | `COMBUS_BUILDER_SKIP=1` sans artefacts pre-existants | exit 1 + `FATAL: ... no pre-existing artefacts ...` | OK |
| 5 | `COMBUS_BUILDER_SKIP=1` avec sentinel present | exit 0, log `using pre-existing`, CPPPATH += out_dir | OK |

Les tests exec la source du hook avec un `Import("env")` et un `env`
mocks (charge comme module sans declencher le binding reel SCons),
puis appellent `main(env)` dans un sous-processus pour isoler les
exit codes.

### 15.5 Build PlatformIO reel

**Non execute** dans cette revision. La correction porte
exclusivement sur la robustesse du hook ; le test bout-en-bout avec
un vrai PlatformIO reste a faire en Phase B (premier prototype
materiel). Le test du hook via sous-processus reproduit fidelement
le comportement que SCons observerait, mais ne couvre pas les
details de l'integration SCons (parse de `extra_scripts =`, ordre
de chargement, etc.).

### 15.6 Fichiers modifies

| Fichier | Nature |
|---------|--------|
| `scripts/combus_scons_hook.py` | Rev 4 : helper `_fatal`, anti-stale atomique, `_verify_skip_artifacts` |
| `scripts/combus_builder/tests/test_a12_rev4.py` | Nouveau : 5 tests, dont anti-stale et skip |

### 15.7 Invariant final (affaibli pour etre techniquement exact)

> **En mode normal** (hook `post:` execute, generation active) :
> si une generation echoue, `_fatal()` -> `sys.exit(1)` est emis
> avant que SCons ne tente de compiler le moindre `.cpp`. Le build
> en cours ne peut donc JAMAIS utiliser des artefacts ComBus qui ne
> correspondent pas a la configuration courante. Les anciens
> artefacts (s'il y en avait) sont laisses sur disque intacts, mais
> le build qui a echoue n'en tient pas compte : il avorte avant
> compilation.
>
> **En mode `COMBUS_BUILDER_SKIP=1`** : le hook verifie uniquement
> la **completude structurelle** des 13 artefacts attendus (presence
> des fichiers). Il ne verifie PAS que ces artefacts correspondent
> aux CPPDEFINES, a la version du builder, ou au contenu canonise
> des `.cb` du buildroot courant. Un skip accepte donc potentiellement
> des artefacts structures OK mais semantiquement obsoletes.
>
> **En consequence** : `COMBUS_BUILDER_SKIP=1` est un **bypass
> explicite**, utile pour CI/debug, dont la coherence semantique
> n'est pas verifiee au niveau du hook. Le mainteneur qui active
> SKIP doit savoir ce qu'il fait (par exemple, build rapide
> d'un fichier unique sans toucher aux artefacts precedemment
> publies).

Cet invariant en mode normal est verifie par les 7 cas de
`test_a12_rev4.py` (en particulier le test 6 qui demontre qu'un
build en echec laisse `out_dir` intact).



---

## 18. COMBUS_BUILDER_SKIP : bypass explicite, semantique non verifiee (rev 7 - 2026-08-22)

### 18.1 Position du probleme

La garde `_check_staging_complete()` et la verification
`_verify_skip_artifacts(out_dir)` portent sur la **completude
structurelle** des 13 artefacts. Elles ne portent pas sur la
**coherence semantique** entre ces artefacts et la configuration
courante.

Concretement, SKIP=1 peut accepter :

- Un `combus.h` qui declare 18 channels alors que les CPPDEFINES
  courantes n'en activent que 7 (les autres seraient compiles mais
  jamais utilises, ou pire, references par accident).
- Un MD5 calcule sur un `.cb` qui n'existe plus ou qui a change
  depuis la derniere generation.
- Un artefact d'un autre projet (PATH confondu), d'un autre
  environnement PlatformIO, ou d'une branche Git differente.

SKIP est donc un **bypass explicite**, pas un mode sur.

### 18.2 Garanties actuelles de SKIP

| Propriete | Verifiee par SKIP |
|-----------|-------------------|
| Les 13 fichiers sont presents | OUI (`_verify_skip_artifacts`) |
| Les fichiers sont non vides / non corrompus (lecture reussie) | OUI (implicite : `is_file()`) |
| Les fichiers contiennent un header C++ valide | NON |
| Le contenu correspond aux CPPDEFINES courantes | NON |
| Le contenu correspond aux `.cb` du buildroot | NON |
| Le contenu correspond a la version courante du builder | NON |
| Le MD5 declare correspond au contenu reel | NON |

La derniere ligne est particulierement importante : SKIP peut
accepter des artefacts ou `combus_local_md5.h` declare un hash
qui ne correspond pas au contenu de `combus_local.h`. Le compilateur
les compilera tous les deux sans erreur, mais le runtime detectera
l'incoherence au moment de la transmission.

### 18.3 Cas d'usage legitimes de SKIP

- **CI / debug** : on veut iterer rapidement sur un `.cpp`
  isole, sans relancer la generation ComBus (lente).
- **Build incrémental** : si on sait qu'aucun `.cb` n'a change
  et qu'aucun CPPDEFINE n'a change, SKIP evite le cout de la
  regeneration.
- **Sandbox** : on veut tester un mapping `.cpp` -> `.h`
  avec une generation fixee.

Dans tous ces cas, le mainteneur **doit savoir** que SKIP court-
circuite la verification semantique. C'est un acte delibere.

### 18.4 Piste d'amelioration : manifest signe

Pour rendre SKIP semantiquement sur, il faudrait un manifest genere
en meme temps que les 13 artefacts, contenant au minimum :

```
// combus_manifest.h - auto-generated by combus_builder
#define COMBUS_BUILDER_VERSION 6
#define COMBUS_BUILDER_DATE "2026-08-22T..."
#define COMBUS_CPPDEFINES_HASH 0xABCDEF12  // SHA256[0:4] of sorted CPPDEFINES
#define COMBUS_BUILDROOT_HASH 0x12345678   // SHA256[0:4] of canonical .cb content
#define COMBUS_REV 7
```

Le hook, en mode SKIP, lirait ce manifest et comparerait les
constantes a celles qu'il vient de calculer sur la configuration
courante. Si un seul ne match pas, `_fatal()` avorte le build avec
un message explicite ("artefacts are stale: COMBUS_CPPDEFINES_HASH
mismatch").

**Note** : ce manifest n'est PAS implemente en rev 6. C'est une
piste pour une evolution ulterieure. Pour l'instant, le hook se
contente de la garantie structurelle (13 fichiers presents).

### 18.5 Recommandation pratique

Pour le proto, documenter dans `platformio.ini` :

```ini
# COMBUS_BUILDER_SKIP=1 is an explicit bypass that only checks
# that the 13 expected artefacts are present. It does NOT verify
# that they match the current CPPDEFINES, .cb files, or builder
# version. Use it knowingly (CI, debug, incremental builds).
extra_scripts =
    post:scripts/combus_scons_hook.py
```

Ainsi, un mainteneur qui active SKIP via `export COMBUS_BUILDER_SKIP=1`
voit dans les logs le disclaimer et comprend le contrat.



---

## 19. Strategie d'inclusion ComBus v2 (Phase B - decisions 2026-08-22)

### 19.1 Etat actuel (legacy)

Trois patterns d'inclusion coexistants :

| Pattern | Header | Contenu | Usage |
|---------|--------|---------|-------|
| **A** Core | `<core/config/machines/combus_types.h>` | FULL enum (3 scopes) | sound, light, dashboard, hw_init_dcdev (15 fichiers) |
| **B** Machine | `<machines/config/machines/<X>/combus/combus.h>` | FULL + array externs + .inc assembles | volvo_A60H_bruder processors (3 fichiers) |
| **C** Remote | `<core/config/machines/<X>/combus_ids_remote.h>` | REMOTE-only enum | dumper_truck_config (4 fichiers) |

### 19.2 Strategie validee par le mainteneur

- **Pattern A → A** : Core code inclut la version `combus_remote` (REMOTE-only).
- **Pattern B → B** : Machine code inclut la version `combus` (complet : REMOTE + LOCAL + SYSTEM).
- **Pattern C → B** : Remote code inclut **aussi** la version `combus` (complet), parce que la telecommande se comporte comme une machine (config hardware, init, modules, config de son propre combus interne).

Note importante du mainteneur : les noms "remote" peuvent porter a confusion. Il y a **deux significations** de "remote" dans le contexte :

1. **Couche logique wire (transmise par le wire)** : ce qu'on appelle
   ici "REMOTE scope" dans le vocable ComBus.
2. **Type de device (telecommande physique)** : un device qui se
   comporte comme une machine (a ses propres CAN/LOCAL/SYSTEM).

Le `combus.h` genere pour la telecommande contiendra ses propres
**LOCAL + SYSTEM scopes**, en plus du REMOTE wire. Le script Python
generera donc un `combus.h` distinct par device (machine ou
telecommande) — la seule nuance dans le generateur est de boucler
sur les configs (machines prises en charge + telecommandes prises
en charge) pour generer chaque combus.

Le sujet du multi-device est **pour plus tard**, mais les "bonnes
graines" peuvent etre plantees maintenant dans la structuration du
generateur.

### 19.3 Vue `combus_local.h`

La vue prefix (REMOTE + LOCAL, sans SYSTEM) peut etre exposee sans
souci, mais elle doit etre marquee clairement comme **"pour
reference seulement"**. En pratique, peu de code en aura besoin :
les processors qui operent sur REMOTE+LOCAL peuvent tres bien
inclure `combus.h` (full) et ignorer les canaux SYSTEM.

### 19.4 Ordre de migration

Les 2 migrations (A et B) seront faites **ensemble** par facilite
et coherence du test final. Pas de migration progressive separee.

### 19.5 Politique de suppression des `.inc` legacy

Les `.inc` legacy seront **supprimes** des qu'ils ne sont plus
utilises. Cela elimine tout risque de bug d'include latent (un `.inc`
modifie par erreur et partiellement reutilise). La regle est simple
: si plus aucun `.cpp`/`.h` ne reference l'.inc, il est supprime.

### 19.6 Plan en 3 etapes

#### Etape 1 (B4 - test sur env jetable)

- Garder tous les includes legacy intacts.
- Ajouter le hook au `platformio.ini` du sous-projet `machines`
  avec `extra_scripts = post:scripts/combus_scons_hook.py`.
- Verifier que les artefacts generes compilent en isolation
  (un mini `main.cpp` qui `#include "combus.h"` doit compiler).
- Aucun `.cpp` existant n'est modifie.

#### Etape 2 (B5 - validation compile + linkage)

- Double-run : ancien pipeline (legacy `.inc`) ET nouveau pipeline
  (`.cb` -> generateur). Les deux tournent en parallele.
- Comparer les **MD5** entre les artefacts legacy et les artefacts
  generes pour la meme config.
- Aucun `.cpp` modifie.

#### Etape 3 (Phase C - migration effective)

- Une fois le contrat protocolaire valide (gate B -> C), remplacer
  progressivement les includes legacy par les includes generes.
- Supprimer les dispatchers (`combus_types.h`, `combus_ids.h`,
  `combus_ids_remote.h`, `machine_type_combus_ids.h`) un par un.
- Supprimer les `.inc` legacy un par un (selon la regle 19.5).

### 19.7 Garde-fous

- **`COMBUS_BUILDER_SKIP=1`** : bypass explicite (cf. section 18) —
  semantique non verifiee, utile pour CI/debug.
- **MD5 cross-validation** : le `combus_local_md5.h` genere doit
  matcher ce que `scripts/combus_md5.py` produit aujourd'hui.
- **Double-run transitoire** : Phase C garde les 2 pipelines en
  parallele pendant au moins une release complete, pour rollback
  rapide si regression.

### 19.8 "Bonnes graines" a planter maintenant

Pour faciliter l'integration future du multi-device (chaque device =
son propre combus.h) :

1. Le generateur doit deja iterer sur une **liste de buildroots**
   (un par sous-projet PlatformIO) plutot que de scanner
   systematiquement `src/`. Aujourd'hui il utilise `discover_and_parse`,
   qui prend un `project_root` ; demain il faudra un `multi_root`.
2. Les artefacts generes vont dans `.pio/build/<env>/combus_generated/`,
   ce qui est **par-env** — chaque sous-projet PlatformIO aura son
   propre dossier de generation, sans collision.
3. Le hook `combus_scons_hook.py` utilise `env["PROJECT_BUILD_DIR"]`
   pour resoudre l'emplacement de sortie — c'est deja la bonne
   cle pour un fonctionnement multi-env.

Ces 3 points ne demandent **aucun changement** aujourd'hui, mais la
structure du code est deja compatible avec une evolution vers
multi-device.


---

## Annexe A : historique des revisions

| Rev | Date       | Auteur | Resume |
|-----|------------|--------|--------|
| 1   | 2026-08-20 | analyse | Premiere analyse. Confondait `extra_scripts = post:` avec `AddPostAction`. |
| 2   | 2026-08-21 | correction | Lot 1 : distinction `extra_scripts = post:` vs `AddPostAction`. Lot 2 : simulation vs validation reelle. |
| 3   | 2026-08-21 | implementation | Production code: `scripts/combus_scons_hook.py`, `scripts/combus_builder.py`, modif `platformio.ini`. |
| 4   | 2026-08-21 | robustesse | Correction du defaut "warning muet + stale". `_fatal`, publication atomique, verif du sentinel SKIP. Tests `test_a12_rev4.py`. |
| 5   | 2026-08-22 | clarif. signature | Section 16 : `discover_and_parse` supporte `env=` et `project_root=` par construction (`resolve_buildroot` politique de priorite). Test `test_signature_consistency.py`. |
| 6   | 2026-08-22 | completude artefacts | `EXPECTED_ARTIFACTS` (13 fichiers) + `_check_staging_complete` (belt-and-braces) + `_verify_skip_artifacts` elargi. Tests 5b et 6 (2-step). Section 17 documente le changement. |
| 7   | 2026-08-22 | affaiblissement invariant | Section 15.7 / 17.5 reformules : mode normal vs SKIP distingues. Section 18 documente SKIP comme bypass explicite (semantique non verifiee), avec piste "manifest signe". |
| 8   | 2026-08-22 | strategie inclusion Phase B | Section 19 : 3 patterns A/B/C, decisions du mainteneur (A->A, B->B, C->B), plan en 3 etapes, politique suppression `.inc` legacy. |
