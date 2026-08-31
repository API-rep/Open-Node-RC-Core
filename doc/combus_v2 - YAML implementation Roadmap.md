# ComBus v2 - Roadmap d'implémentation

> Roadmap opérationnelle pour le passage de la composition ComBus par `.inc` vers une génération build-time à partir de définitions YAML.

> **Référence** : `combus_v2 - YAML implementation.md` (note d'architecture validée).

> **Statut** : proto. Aucun engagement de date ; ordre d'exécution indicatif.

---

## Vue d'ensemble

Le travail s'organise en **5 phases** (A → E) déjà définies dans la note d'architecture. Ce roadmap ajoute :

- le **détail des tâches** par phase ;
- les **critères de passage** (gates) entre phases ;
- les **dépendances** entre tâches ;
- une **estimation d'effort** indicative ;
- les **risques** identifiés et leurs mitigations.

Principe général :

> **On ne supprime aucun `.inc` legacy tant que la validation complète n'est pas passée.**

> Chaque phase produit un artefact observable et testable avant de passer à la suivante.

### Note sur le vocabulaire

Ce roadmap décrit la **cible** (ce que le système doit devenir), pas l'état actuel du repo. Les références à des fichiers ou concepts legacy (`.inc`, umbrellas, etc.) sont conservées uniquement parce qu'elles font partie du **périmètre de migration** — c'est-à-dire ce qui doit disparaître à terme. Elles ne décrivent pas un état à reproduire.

### Note sur les hypothèses architecturales

Plusieurs choix techniques sont formulés comme des **hypothèses à valider** dans les premières phases, et non comme des décisions figées :

- **Hook `pre:` (corrigé de `post:`) pour l'ajout du `.cpp` aux sources** : hypothèse validée en **A15** (2026-08-25). Le hook doit être `pre:` pour que `env.Replace(PIOBUILDFILES=...)` soit effectif avant la résolution SCons du `build_src_filter`. Le `BuildContext` est alors acquis via le fallback `GetProjectOption("build_flags")` au lieu de `env["CPPDEFINES"]` directement.
- **Mécanisme de découverte des `CbProc`** : à fixer en seconde passe (regex, AST, table explicite). Pas bloquant pour le proto.
- **Ordre canonique vs legacy** : la stratégie (override, mode compat, bump de version) sera décidée empiriquement en Phase A.

---

## Phase A - Preuve de concept

**Objectif** : démontrer qu'un générateur Python peut produire un header ComBus dont le **contrat protocolaire** (mapping des IDs, valeurs numériques des enums) est identique au legacy, à partir d'un seul thème (recommandation : `vbat`).

> **Important** : ce qui doit être identique est le **contrat protocolaire** (valeurs des enums, mapping ID ↔ nom, ordre des IDs), pas nécessairement le texte ou le format exact du header généré. Le générateur est libre de produire un header syntaxiquement différent du legacy, tant que le runtime voit les mêmes IDs aux mêmes valeurs.

### Tâches

| # | Tâche | Livrable | Dépendances |
|---|---|---|---|
| A1 | Définir le schéma YAML minimal (champs `id`, `type`, `scope`, `theme`, `requires`) | `schemas/cb_v1.schema.json` (ou Pydantic) | - |
| A2 | Écrire un `.cb` pour le thème `vbat` (2-3 channels réels) | `src/core/system/vbat/vbat.cb` | A1 |
| A3 | Implémenter la discovery + parsing YAML minimal (collecte brute, sans tri/canonisation) | `scripts/combus_builder/parser.py` | A1 |
| A4 | Implémenter l'acquisition du contexte de build et la résolution des flags (CPPDEFINES, avec fallback de compatibilité) et récupérer le buildroot | `scripts/combus_builder/flags.py` | - |
| A5 | Implémenter la canonisation (fusion + tri global `(scope, type, theme, id)`) | `scripts/combus_builder/canon.py` | A3 |
| A6 | Implémenter le générateur de header C++ (enum + tableau + count) | `scripts/combus_builder/generator.py` | A5 |
| A7 | Implémenter le calcul MD5 des fichiers combus | `scripts/combus_builder/md5.py` | A5 |
| A8 | Écrire le test de cohérence CPPDEFINES (paramétrable par env) | `scripts/combus_builder/coherence.py` | A4 |
| A9 | Migrer les définitions `.inc` restantes vers `.cb`, nettoyer les incohérences legacy et valider la génération | `.cb` migrés + validation compilation/génération | - |
| A10 | Valider le déterminisme (2 runs → contrat identique) | Test pytest | A6, A7 |
| A11 | Documenter les invariants observés (ordre canonique vs legacy) | Note dans `doc/` | A9, A10 |
| A12 | **Choisir le mécanisme d'intégration PlatformIO** : valider à quel moment du cycle (`pre:` / `post:` / autre) le contexte de build acquis par A4 est effectivement disponible, et confirmer le hook retenu avant l'intégration Phase B | Rapport de validation | A4, A8 |

### Gate Phase A → Phase B

> **Statut au 2026-08-22** (post A12 rev 4) :

> les cases 1, 3, 4, 5 sont cochées. Les cases 2 et 6 sont marquées **N/A par décision mainteneur** (cf. décision ci-dessous).
> Le gate **A → B est levé** sur cette base.

> `python _check_gate.py` (5 checks).

- [x] Un thème (`vbat`) est généré sans erreur. *(voir note 1)*
- [x] ~~Le **contrat protocolaire** généré est identique au legacy + nouvelle implémentation `:direction` (mapping IDs, valeurs d'enum).~~ *(voir note 2 — **N/A par décision mainteneur 2026-08-22** : la réorganisation des champs + édition de certains paramètres rend le travail trivial. Sera validé au gate B → C.)*
- [x] Le MD5 est identique entre deux runs successifs. *(voir note 3)*
- [x] Le test de cohérence CPPDEFINES passe. *(voir note 4)*
- [x] L'hypothèse `post:` est validée (ou un fallback est identifié). *(voir note 5)*
- [x] ~~Le code est commit dans une branche dédiée (ex. `proto/combus-v2-phase-a`).~~ *(voir note 6 — **N/A par décision mainteneur 2026-08-22** : on travaille directement sur `main`.)*

> **Décision 2026-08-22** : les items 2 et 6 sont marqués **N/A par décision mainteneur** :
> - **Item 2 (contrat protocolaire vs legacy)** : la réorganisation des champs (ajout de `:direction`) et l'édition de certains paramètres rendent le travail trivial selon le mainteneur. La validation du contrat protocolaire est repoussée au gate **B → C**, où elle est déjà listée comme case obligatoire (`Le contrat protocolaire généré est identique au contrat legacy pour le thème vbat`).
> - **Item 6 (branche dédiée)** : on travaille directement sur `main`. Pas de branche `proto/combus-v2-phase-a` créée. Le diff cumulatif `combus_v2_A12.diff` (90 400 octets, 7 fichiers) est sauvegardé à la racine pour archivage.
>
> **Conséquence opérationnelle** : le gate **A → B est levé**. Les items B1, B2, B3 sont déjà réalisés (voir `doc/combus_v2 - A12 hook mechanism.md` section 15). Le prochain pas concret est **B4** : tester sur un env PlatformIO jetable (`env:test_phase_b`) avec `extra_scripts = pre:scripts/combus_scons_hook.py`.

#### Notes

1. **`vbat` parsé sans erreur** : `src/core/system/vbat/vbat_failsafe.cb`

   est parsé par `discover_and_parse` + `canonize` (exit 0). Le seul

   channel déclaré est `FAILSAFE_VBAT` (theme=`failsafe`, value=`high`).

   Note : le **thème** du channel est `failsafe` (pas `vbat`) — le

   nom du fichier contient `vbat` mais le theme YAML est `failsafe`.

   Le thème `vbat` (au sens du champ `theme:`) ne contient aucun

   channel dans l'état actuel du repo, ce qui est cohérent avec le

   fait que `FAILSAFE_VBAT` est sémantiquement un contributeur

   failsafe (système), pas un canal vbat.

2. **Contrat protocolaire vs legacy** : **hors périmètre de A12**.

   A9 (comparaison legacy) n'est pas couvert par le travail A12 ;

   le comparateur A9 (`scripts/combus_builder_diff.py` ou

   équivalent) n'existe pas dans ce repo. À valider dans une tâche

   dédiée en Phase A.

3. **Déterminisme MD5** : `_check_gate.py` exécute deux runs

   successifs du hook et compare les MD5 du répertoire généré.

   Les deux runs produisent `5bcdfbc96146668deabf677f83064904`.

   Le pipeline inclut les MD5 par-fichier (`combus_md5.h`,

   `combus_local_md5.h`, `combus_remote_md5.h`) — voir A10.

4. **Cohérence CPPDEFINES** : `pytest scripts/combus_builder/tests/test_coherence.py`

   → **19 passed in 0.52s**. Tous les tests de cohérence entre

   BuildContext et channels déclarés passent.

5. **Hypothèse `pre:` validée en A15** (correction 2026-08-25) :

   initialement `post:`, mais A15 a montré qu'il fallait passer en `pre:`

   pour que `env.Replace(PIOBUILDFILES=...)` soit effectif. Le CPPPATH de

   `common_defs` est aussi ajouté explicitement (le hook en `pre:` tourne

   avant que PlatformIO ne l'ajoute automatiquement via lib_deps). Voir

   `doc/combus_v2_A15_report.md` section 6.

6. **Branche dédiée** : non créé. Le hook A12 vit actuellement

   unstaged sur la branche `main` (HEAD `dc157ea WIP/backup:

   working tree before combus_builder rework`). Le diff cumulatif

   est dans `combus_v2_A12.diff` (58 827 octets, 5 fichiers,

   1 340 insertions). À committer sur une branche

   `proto/combus-v2-phase-a` (ou équivalent) par le mainteneur.

### Risques Phase A

| Risque | Mitigation |
|---|---|
| Ordre canonique diverge du legacy | Documenter la divergence ; envisager override explicite ou mode compat (§10). |
| `CPPDEFINES` vide en hook `post:` | **Résolu en A15** : passer en `pre:`. Le fallback `GetProjectOption("build_flags")` de `flags.py` couvre le cas. |
| Parser YAML fragile | Utiliser un validateur strict (Pydantic / JSON Schema) dès le départ. |
| Contrat protocolaire involontairement modifié | Comparer systématiquement les valeurs d'enum et le mapping ID ↔ nom, pas le texte du header. |

---

## Phase B - Intégration PlatformIO

**Objectif** : brancher le générateur dans le build PlatformIO réel, sans rien casser.

> **Note** : le mécanisme d'intégration (hook `pre:` validé en A15) est désormais figé. Le hook ajoute le `.cpp` généré à `PIOBUILDFILES` et le CPPPATH de `common_defs` pour `pin_defs.h`.

### Tâches

| # | Tâche | Livrable | Dépendances |
|---|---|---|---|
| B1 | Implémenter le mécanisme d'intégration (hook SCons `pre:`) | `scripts/combus_scons_hook.py` | A6, A12 |
| B2 | Déclarer les dépendances SCons sur les `.cb` / `.cbch` | Dans le hook | B1 |
| B3 | Ajouter le répertoire `.pio/build/<env>/combus_generated/` aux include paths | Dans le hook | B1 |
| **B4** | **A15 — Finalisation du build `volvo_A60H_bruder`** : ajouter le `.cpp` généré aux sources + corriger les doubles définitions `Input*Map*` | `combus_v2_A15_linker_fix.diff` | B1, B2, B3 |
| B5 | Valider compilation + linkage avec un programme trivial | Build successful | B4 |
| B6 | Documenter le hook dans `platformio.ini` | Mise à jour `platformio.ini` | B1 |

#### Détail B4 — A15 Linker Fix (2026-08-25)

**Symptôme** : `volvo_A60H_bruder` compile les 16 `.cpp` mais le link échoue avec :
- `multiple definition of 'InputAnalogMapArray'` / `InputDigitalMapArray` / `InputAnalogMapCount` / `InputDigitalMapCount` (entre `nop_map.cpp` et `PS4_dualshock_map.cpp`)
- `undefined reference to 'comBus'`

**Cause réelle** :
1. `nop_map.cpp` et `PS4_dualshock_map.cpp` définissent les 4 symboles `Input*Map*` sans garde `#ifdef`, contrairement à `inputs_map.h` qui dispatchait déjà correctement. PlatformIO compile tous les `.cpp` du `build_src_filter`, d'où le conflit.
2. Le `.cpp` généré par le builder (`combus_generated/combus.cpp`) n'était pas dans le `build_src_filter` de `volvo_A60H_bruder` (seulement pour `test_combus_loopback` via un helper script dédié). Le hook SCons ne touchait que le CPPPATH, pas les sources — d'où `undefined reference to 'comBus'`.

**Solution** (diff `combus_v2_A15_linker_fix.diff`, 8.9 KB, 4 fichiers) :

| Fichier | Modification |
|---|---|
| `platformio.ini` | `post:` → `pre:` (le hook doit tourner avant la résolution SCons des `build_src_filter`) |
| `scripts/combus_scons_hook.py` | Nouvelle fonction `_add_generated_cpp_to_sources()` + bloc CPPPATH `common_defs` dans `main()` |
| `src/core/config/machines/dumper_truck/inputs/inputs_map/PS4_dualshock_map.cpp` | Garde `#if defined(INPUT_PS4_DS4_BT)` autour des définitions |
| `src/core/config/machines/dumper_truck/inputs/inputs_map/nop_map.cpp` | Garde `#if defined(INPUT_NONE)` autour des définitions |

**Validation** :
- `volvo_A60H_bruder` : **SUCCESS en 83.23s** (linker OK, comBus résolu, pas de double définition)
- `test_combus_loopback` : **PASSED en 40.59s** (pas de régression)
- `sound_node_volvo` : inchangé (hors scope, channels legacy non déclarés)
- `remotes` : inchangé (problème de configuration env préexistant, hors scope)

**Rapport détaillé** : `doc/combus_v2_A15_report.md`.

#### Détail B5 — A16 SCons Deferred Task (2026-08-25)

**Problème de l'approche A12/A15** : le hook appelait `acquire_build_context()` et la génération C++ de façon **eager** (au moment du chargement du hook, en `pre:` ou `post:`). Cela créait une double fragilité :
- En `pre:` : `CPPDEFINES` est vide → fallback peu fiable
- En `post:` : `PIOBUILDFILES` est gelé → `combus.cpp` non compilé
- Dans les deux cas : **aucune invalidation SCons** sur changement de flags (le hook tournait toujours, peu importe la signature).

**Solution A16** : enregistrer une **tâche SCons différée** via `env.Command()`, exécutée par SCons au moment du build (pas du hook).

| Aspect | A12/A15 (eager) | A16 (differé) |
|---|---|---|
| Moment d'exécution | Au chargement du hook (pre ou post) | Au build, par SCons, quand la cible est construite |
| `CPPDEFINES` | Vide (pre) ou complet (post) | Toujours complet (env fully built) |
| `PIOBUILDFILES.Replace` | Efficace (pre) ou sans effet (post) | Efficace (pré, à l'enregistrement de la Command) |
| Invalidation SCons | Aucune | Natif : `varlist=["CPPDEFINES", "CPPPATH", "LIBDEPS"]` |
| Idempotence | Risque double déclaration | Garde `_combus_command_registered` |
| Assertions contexte | Faibles / fallback | **Strictes** : `CPPDEFINES` vide ou flag structurel absent ⇒ FATAL |

**Fichiers modifiés** (diff `combus_v2_A16_scons_deferred_task.diff`, 32.2 KB, 2 fichiers) :

| Fichier | Modification |
|---|---|
| `scripts/combus_scons_hook.py` | Réécriture complète : `env.Command(target, source, action, varlist)` + `_generate_combus_action()` + assertions strictes + COMBUS_REPO_ROOT stashé pour le subprocess d'action |
| `platformio.ini` | Aucun changement fonctionnel (`pre:` conservé — le hook fonctionne maintenant correctement en `pre:` grâce au mécanisme différé) |

**Détails techniques notables** :
- `_EXPECTED_ARTEFACTS_FALLBACK` (constante hardcodée) car le namespace d'exec de SCons `SConscript()` ne propage pas les constantes module-level aux fonctions imbriquées.
- `env["COMBUS_REPO_ROOT"]` est stashé à l'enregistrement et réinséré dans `sys.path` au début de l'action (le subprocess d'action n'hérite pas de `sys.path`).
- Garde `_combus_command_registered` pour éviter le warning `Two different environments were specified for target`.

**Validation — 5 tests** :

| Test | Commande | Durée | Résultat | Action combus |
|---|---|---|---|---|
| 1 | `pio run -e volvo_A60H_bruder` | 25.47s | ✅ SUCCESS | Exécutée (24 defines) |
| 2 | `pio test -e test_combus_loopback --without-uploading --without-testing` | 42.59s | ✅ PASSED | OK (test env) |
| 3 | `pio run -e remotes` | 8.40s | ❌ FAILED | `channel 'FAILSAFE' requires ['HAS_FAILSAFE']` — **trou de configuration embryonnaire, identique à l'état pré-A16. Pas une régression.** |
| 4 | Toggle `-D DEBUG_SYSTEM` puis `pio run -e volvo_A60H_bruder` | 88.55s | ✅ SUCCESS | **RÉ-EXÉCUTÉE** (23 defines, varlist détecté) |
| 5a | Build identique (mêmes 24 defines) | 13.61s | ✅ SUCCESS | **NON RÉ-EXÉCUTÉE** (no-op) |
| 5b | Re-toggle (23→24 defines) | 81.46s | ✅ SUCCESS | RÉ-EXÉCUTÉE |
| 5c | Build identique (24 defines) | 14.31s | ✅ SUCCESS | **NON RÉ-EXÉCUTÉE** (no-op) |

**Objectif atteint** : **rebuild quand une entrée pertinente change, absence de rebuild sinon.**

**Preuve de l'invalidation `varlist`** :
- T1 : 24 defines → action exécutée
- T4 : 23 defines (toggle) → action ré-exécutée (signature changée)
- T5a : 24 defines (identique à T1) → action **PAS** ré-exécutée (log silencieux sur `[combus_scons_action]`)
- T5b : 24 defines (re-toggle) → action ré-exécutée
- T5c : 24 defines (identique à T5b) → action **PAS** ré-exécutée

**Diff** : `combus_v2_A16_scons_deferred_task.diff` (32.2 KB, racine du repo).

### Gate Phase B → Phase C / D

- [x] Le build PlatformIO exécute le générateur sans erreur.
- [x] Les include paths sont corrects (compilation OK).
- [x] Le linkage est OK (pas de symbole manquant). *(validé en A15 / B4)*
- [ ] Le contrat protocolaire généré est identique au contrat legacy pour le thème `vbat`.
- [ ] Les `.inc` legacy sont **toujours en place** (double-run transitoire).

### Risques Phase B

| Risque | Mitigation |
|---|---|
| Mécanisme d'intégration non fonctionnel | **Résolu en A15** : `pre:` + `_add_generated_cpp_to_sources()` + CPPPATH `common_defs`. |
| Dépendances SCons non détectées | Forcer la regénération manuelle pour valider ; ajuster `Glob`. |
| Include path non propagé | **Note A15** : le hook ajoute `common_defs/include` explicitement (sinon `pin_defs.h` introuvable en `pre:`). |
| `comBus` indéfini | **Résolu en A15** : le `.cpp` est ajouté à `PIOBUILDFILES` (et le CPPPATH `common_defs` aussi). |

---

## Phase C - Chaînes (`.cbch`)

**Objectif** : introduire le format `.cbch` pour décrire le câblage des `CbProcChain`, avec validation des processors référencés.

> **Note importante** : la Phase C est **indépendante** de la Phase D. Le générateur de `.cbch` peut être développé et testé sans attendre la migration des `.cb`. Les deux pipelines (`.cb` → enums/tableaux, `.cbch` → wiring) sont découplés.

### Tâches

| # | Tâche | Livrable | Dépendances |
|---|---|---|---|
| C1 | Définir le schéma YAML `.cbch` (champs `chain`, `processors`, `in`/`out`) | `schemas/cbch_v1.schema.json` | A1 |
| C2 | Écrire un `.cbch` pour la chaîne `failsafe` | `src/core/system/failsafe/failsafe.cbch` | C1 |
| C3 | Étendre le parser pour reconnaître `.cbch` | Mise à jour `parser.py` | C1, A3 |
| C4 | Implémenter la validation des processors (lookup dans les `CbProc` enregistrés) | `scripts/combus_builder/proc_validator.py` | C3 |
| C5 | Générer le code de wiring (table de processors, init de chaîne) | Extension `generator.py` | C3 |
| C6 | Tester : processor existant → accepté, processor inexistant → erreur | Tests pytest | C4 |
| C7 | Documenter le mécanisme de découverte des `CbProc` (seconde passe - voir note §15) | Note dans `doc/` | C4 |

### Gate Phase C (indépendante de D)

- [ ] Un `.cbch` est parsé et validé correctement.
- [ ] Le wiring généré est compilé et linkage OK.
- [ ] Un processor inexistant provoque une erreur explicite au build.
- [ ] Le mécanisme de découverte des `CbProc` est documenté (même si pragmatique pour le proto).

### Risques Phase C

| Risque | Mitigation |
|---|---|
| Découverte des `CbProc` fragile | Commencer par regex simple ou table explicite ; industrialiser en seconde passe. |
| Wiring généré incompatible avec le runtime | Comparer au wiring legacy existant ; valider structurellement. |

---

## Phase D - Migration thème par thème

**Objectif** : migrer chaque thème legacy (`.inc`) vers `.cb` un par un, avec validation complète avant suppression.

> **Note** : la Phase D peut démarrer dès que la Phase B est validée, indépendamment de l'état de la Phase C. Les thèmes peuvent être migrés sans que le câblage des chaînes soit encore en `.cbch`.

### Tâches (par thème)

| # | Tâche | Livrable | Dépendances |
|---|---|---|---|
| Dx.1 | Identifier les `.inc` du thème | Liste dans `doc/` | - |
| Dx.2 | Écrire le `.cb` équivalent (canal par canal) | `*.cb` | A2 (schéma) |
| Dx.3 | Générer le ComBus v2 pour ce thème | Header dans `.pio/build/...` | Dx.2 |
| Dx.4 | Comparer le **contrat protocolaire** au legacy (mapping IDs, valeurs d'enum) | Rapport | Dx.3 |
| Dx.5 | Comparer le MD5 legacy et le MD5 généré | Rapport | Dx.3 |
| Dx.6 | Tester en double-run (legacy + générateur) sur un env de test | Build OK | Dx.2, B1 |
| Dx.7 | Supprimer les `.inc` du thème (uniquement après validation complète) | Commit de suppression | Dx.4, Dx.5, Dx.6 |

### Thèmes candidats (ordre indicatif)

1. `vbat` (déjà fait en Phase A).
2. `core` (simple, valide le mécanisme).
3. `failsafe` (teste le câblage des chaînes, en synergie avec Phase C si elle est avancée).
4. Autres thèmes métier (au cas par cas).

### Gate par thème

- [ ] Déterminisme validé (2 runs → contrat identique).
- [ ] MD5 legacy et MD5 généré identiques.
- [ ] Builds parallèles (legacy vs générateur) OK.
- [ ] Aucune régression silencieuse détectée (tests fonctionnels passent).

### Risques Phase D

| Risque | Mitigation |
|---|---|
| Régression silencieuse sur un thème | Double-run prolongé avant suppression ; tests fonctionnels obligatoires. |
| Ordre canonique divergent | Documenter ; conserver mode compat ou bump version (§23). |
| Couplage inattendu core ↔ machine | Profiter du découplage `combus_ids_remote.h` (§22 Phase E) pour isoler. |

---

## Phase E - Suppression legacy complète

**Objectif** : supprimer tous les artefacts legacy, basculer le runtime sur les artefacts générés, et nettoyer les artefacts obsolètes.

### Tâches

| # | Tâche | Livrable | Dépendances |
|---|---|---|---|
| E1 | Générer les 3 fichiers `combus_ids_*` (analog, digital, _remote pour core) | Headers dans `.pio/build/...` | D (tous thèmes migrés) |
| E2 | Basculer le runtime sur les headers générés (suppression des includes legacy) | Modifications C++ | E1 |
| E3 | Supprimer les umbrellas legacy | Commits | E2 |
| E4 | Supprimer tous les `.inc` restants | Commits | E2 |
| E5 | Nettoyer les artefacts obsolètes dans `src/` (tout fichier généré qui aurait été commités par erreur) | Commits | E2 |
| E6 | Intégrer `combus_md5.py` au générateur (note Python en tête, sortie dans `.pio/`) | `scripts/combus_builder/md5.py` (refactor) | E1 |
| E7 | Mettre à jour la documentation (board, core, ecosystem) | Doc | E2, E3, E4 |
| E8 | Bumper `schema_version` et `generator_version` (si breaking change) | Header de version | E1 |

### Gate Phase E (final)

- [ ] Plus aucun `.inc` legacy dans le repo.
- [ ] Plus aucun artefact généré dans `src/`.
- [ ] Les 3 fichiers `combus_ids_*` sont générés et utilisés par le runtime.
- [ ] Le MD5 est dans `.pio/build/<env>/generated/combus/`.
- [ ] Le couplage core ↔ machine est résolu (les modules core n'incluent plus de headers machine-specific).
- [ ] Tous les builds passent (toutes machines, tous envs).
- [ ] Documentation à jour.

### Risques Phase E

| Risque | Mitigation |
|---|---|
| Découverte d'un thème non migré | Audit complet du repo avant Phase E ; check-list par machine. |
| Artefact obsolète oublié | Script de détection (`git status` ; recherche de patterns `_md5.h`, `combus_ids_*.h` dans `src/`). |
| Documentation désynchronisée | Mettre à jour la doc **en même temps** que le code (PR atomique). |

---

## Estimation d'effort (indicative)

> **Note** : ces estimations sont très approximatives. Le code est en proto, pas en production. Ajuster au fil de l'eau.

| Phase | Effort estimé | Commentaire |
|---|---|---|
| **A** | 2-4 jours | Schéma + parser + générateur pour 1 thème + validation hypothèse `post:` (corrigée en `pre:` via A15). |
| **B** | 1-2 jours | Intégration PlatformIO + tests d'intégration. **A15** a ajouté ~30 min de travail pour fixer le linker. |
| **C** | 1-2 jours | `.cbch` + validation processors (approche pragmatique). Indépendant de D. |
| **D** | 0.5-1 jour par thème | Migration itérative ; la majorité est de la conversion `.inc` → `.cb`. Indépendant de C. |
| **E** | 1-2 jours | Nettoyage + bascule finale. |
| **Total** | ~1-2 semaines | Pour un proto fonctionnel sur quelques thèmes. |

---

## Tâches parallèles / transverses

Certaines tâches peuvent être menées en parallèle des phases :

| Tâche | Phase cible | Commentaire |
|---|---|---|
| Audit des `.inc` existants | Avant Phase A | Identifier tous les fragments, umbrellas, et artefacts générés. |
| Test de cohérence CPPDEFINES | Phase A (A8) | Critique pour éviter les faux positifs. |
| Validation de l'hypothèse `pre:` | Phase A (A12) / Phase B (B4 / A15) | **Validée en A15** (2026-08-25). Le hook `pre:` est désormais le mécanisme figé. |
| Documentation du mécanisme de découverte `CbProc` | Phase C (C7) | Seconde passe, peut être documentée après. |
| Vérification de l'ordre `wire_end` | Avant Phase A | Déterminer son importance réelle dans le code C++. |
| Finalisation du build `volvo_A60H_bruder` | Phase B (B4 / A15) | **Réalisée** (2026-08-25). Build SUCCESS, diff `combus_v2_A15_linker_fix.diff` (8.9 KB). |

---

## Check-list globale (à cocher au fil de l'eau)

- [x] Schéma `.cb` défini et validé.
- [ ] Schéma `.cbch` défini et validé.
- [x] Générateur Python opérationnel (parser + canon + generator + md5).
- [x] Mécanisme d'intégration PlatformIO validé (hook `pre:`).
- [x] Test de cohérence CPPDEFINES opérationnel.
- [x] Au moins un thème migré avec succès (Phase D pour ce thème).
- [x] Déterminisme validé (MD5 stable entre runs).
- [ ] Contrat protocolaire identique au legacy sur tous les thèmes migrés.
- [ ] Tous les `.inc` supprimés (après validation complète).
- [ ] Tous les artefacts obsolètes nettoyés dans `src/`.
- [x] Les 3 fichiers `combus_ids_*` générés et utilisés (via combus_builder A6.1).
- [ ] Documentation à jour (board, core, ecosystem).

---

## Annexe - Ordre d'exécution recommandé

```
Phase A (vbat seul + validation hypothèse pre:)
    │
    ▼
Phase B (intégration PlatformIO sur env de test)
    │
    ├── A15 (2026-08-25) : finalisation build volvo_A60H_bruder
    │
    ├──┬─────────────────────────┐
    ▼                            ▼
Phase C (chaîne failsafe      Phase D (migration thème
en .cbch - indépendant)      par thème - indépendant)
    │                            │
    └──┬─────────────────────────┘
                   ▼
              Phase E (suppression legacy + bascule finale)
```

**Points clés** :

- **A → B** : séquentiel (B dépend de la validation de l'hypothèse `pre:` en A — corrigée en A15).
- **B → C et B → D** : C et D peuvent démarrer en parallèle après B.
- **B4 / A15** : étape intermédiaire obligatoire pour tout env qui référence `comBus` (machines, sound, remotes) — ajoutée en 2026-08-25.
- **B5 / A16** : refactor du hook en tâche SCons différée (`env.Command` + `varlist`) — ajoutée en 2026-08-25. Permet l'invalidation SCons native sur changement de CPPDEFINES / CPPPATH / LIBDEPS.
- **C et D** : indépendants l'un de l'autre.
- **C + D → E** : E nécessite que C et D soient terminés.

Chaque phase produit un livrable observable (commit, build successful, test pass) avant de passer à la suivante.

---

## Annexe - Cible vs état actuel

Cette annexe décrit la **cible** (ce que le système doit devenir), pas l'état actuel du repo. Les références à des fichiers legacy sont conservées uniquement parce qu'elles font partie du périmètre de migration.

| Cible | Statut | Action |
|---|---|---|
| `.cb` / `.cbch` comme sources de vérité | À créer | Phase A, C |
| Headers générés dans `.pio/build/<env>/combus_generated/` | **Créé** | Phase B (résolu) |
| `.cpp` généré compilé dans le firmware (comBus résolu) | **Résolu en A15** | Phase B (B4) |
| Aucun artefact généré dans `src/` | À atteindre | Phase E |
| Aucun `.inc` legacy | À supprimer | Phase D (par thème), E (reste) |
| Aucun umbrella legacy | À supprimer | Phase E |
| `combus_md5.py` intégré au générateur | À refactorer | Phase E |
| 3 fichiers `combus_ids_*` générés (analog, digital, _remote) | **Créé** | Phase E (déjà opérationnel via combus_builder) |
| Couplage core ↔ machine résolu | À valider | Phase E |

---

## Annexe - Notes pour les revues futures

- **Découverte `CbProc`** : mécanisme à fixer en seconde passe (regex, AST, table explicite). Pas bloquant pour le proto.
- **Version majeure** : pas de semver tant que le système est en proto. `schema_version` et `generator_version` sont des entiers simples.
- **Ordre `wire_end`** : à vérifier dans le code C++ existant pour confirmer son importance protocolaire.
- **Test d'ordre de scan** : toujours dans un env de test jetable (branche temporaire, copie locale).
- **Hook `pre:`** : hypothèse validée en A15 (2026-08-25), pas une décision figée à long terme mais **le seul mécanisme qui marche actuellement** pour les envs non-test.
- **Contrat protocolaire** : ce qui doit être identique au legacy, pas le texte du header.
- **Phase C indépendante de Phase D** : les deux pipelines (`.cb` et `.cbch`) peuvent être développés en parallèle.
- **A15 linker fix** : étape de finalisation du build `volvo_A60H_bruder` ajoutée en Phase B (B4). Le hook `pre:` + `_add_generated_cpp_to_sources()` + gardes `#ifdef INPUT_*` dans les `*_map.cpp` sont la solution figée pour le linkage. Tout env futur qui référence `comBus` doit en bénéficier automatiquement (idempotent).
- **A16 SCons deferred task** : refactor architectural majeur ajouté en Phase B (B5). Le hook ne fait plus que **enregistrer** une `env.Command()` (avec `varlist=["CPPDEFINES", "CPPPATH", "LIBDEPS"]`) ; la génération effective est exécutée par SCons au moment du build, avec le contexte complet. C'est désormais le **mécanisme figé** : invalide proprement, ne régénère pas inutilement, assertions strictes sur le contexte. Tout refactor futur du générateur doit s'appuyer sur ce mécanisme (pas le contourner).
