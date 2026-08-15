# ComBus v2 — Roadmap d'implémentation

> Roadmap opérationnelle pour le passage de la composition ComBus par `.inc` vers une génération build-time à partir de définitions YAML.
>
> **Référence** : `combus_v2 - YAML implementation.md` (note d'architecture validée).
>
> **Statut** : proto. Aucun engagement de date ; ordre d'exécution indicatif.

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

- **Hook `post:` pour `CPPDEFINES`** : hypothèse à tester en Phase A. Si elle ne tient pas (PlatformIO 6.x résout les `extends` après `post:` aussi, par exemple), un fallback est prévu.
- **Mécanisme de découverte des `CbProc`** : à fixer en seconde passe (regex, AST, table explicite). Pas bloquant pour le proto.
- **Ordre canonique vs legacy** : la stratégie (override, mode compat, bump de version) sera décidée empiriquement en Phase A.

---

## Phase A — Preuve de concept

**Objectif** : démontrer qu'un générateur Python peut produire un header ComBus dont le **contrat protocolaire** (mapping des IDs, valeurs numériques des enums) est identique au legacy, à partir d'un seul thème (recommandation : `vbat`).

> **Important** : ce qui doit être identique est le **contrat protocolaire** (valeurs des enums, mapping ID ↔ nom, ordre des IDs), pas nécessairement le texte ou le format exact du header généré. Le générateur est libre de produire un header syntaxiquement différent du legacy, tant que le runtime voit les mêmes IDs aux mêmes valeurs.

### Tâches

| # | Tâche | Livrable | Dépendances |
|---|---|---|---|
| A1 | Définir le schéma YAML minimal (champs `id`, `type`, `scope`, `theme`, `requires`) | `schemas/cb_v1.schema.json` (ou Pydantic) | — |
| A2 | Écrire un `.cb` pour le thème `vbat` (2-3 channels réels) | `src/core/system/vbat/vbat.cb` | A1 |
| A3 | Implémenter la discovery + parsing YAML minimal (collecte brute, sans tri/canonisation) | `scripts/combus_builder/parser.py` | A1 |
| A4 | Implémenter la résolution des flags (lecture `env["CPPDEFINES"]` — mécanisme à valider) | `scripts/combus_builder/flags.py` | — |
| A5 | Implémenter la canonisation (fusion + tri global `(scope, type, theme, id)`) | `scripts/combus_builder/canon.py` | A3 |
| A6 | Implémenter le générateur de header C++ (enum + tableau + count) | `scripts/combus_builder/generator.py` | A5 |
| A7 | Implémenter le calcul MD5 (représentation canonique JSON) | `scripts/combus_builder/md5.py` | A5 |
| A8 | Écrire le test de cohérence CPPDEFINES (paramétrable par env) | `scripts/combus_builder/coherence.py` | A4 |
| A9 | Comparer le **contrat protocolaire** généré au legacy (mapping IDs, valeurs d'enum) | Script de comparaison + rapport | A6, A7 |
| A10 | Valider le déterminisme (2 runs → contrat identique) | Test pytest | A6, A7 |
| A11 | Documenter les invariants observés (ordre canonique vs legacy) | Note dans `doc/` | A9, A10 |
| A12 | **Valider l'hypothèse `post:`** : tester sur un env PlatformIO réel ; si CPPDEFINES est vide, basculer sur un fallback (recursion `extends` ou autre) | Rapport de validation | A4, A8 |

### Gate Phase A → Phase B

- [ ] Un thème (`vbat`) est généré sans erreur.
- [ ] Le **contrat protocolaire** généré est identique au legacy (mapping IDs, valeurs d'enum).
- [ ] Le MD5 est identique entre deux runs successifs.
- [ ] Le test de cohérence CPPDEFINES passe.
- [ ] L'hypothèse `post:` est validée (ou un fallback est identifié).
- [ ] Le code est commit dans une branche dédiée (ex. `proto/combus-v2-phase-a`).

### Risques Phase A

| Risque | Mitigation |
|---|---|
| Ordre canonique diverge du legacy | Documenter la divergence ; envisager override explicite ou mode compat (§10). |
| `CPPDEFINES` vide en hook `post:` | Tester explicitement (A12) ; fallback temporaire à recurse `extends` si besoin. |
| Parser YAML fragile | Utiliser un validateur strict (Pydantic / JSON Schema) dès le départ. |
| Contrat protocolaire involontairement modifié | Comparer systématiquement les valeurs d'enum et le mapping ID ↔ nom, pas le texte du header. |

---

## Phase B — Intégration PlatformIO

**Objectif** : brancher le générateur dans le build PlatformIO réel, sans rien casser.

> **Note** : le mécanisme exact d'intégration (hook `post:`, autre hook SCons, ou solution alternative) dépend du résultat de A12. Cette phase s'adapte au choix validé en Phase A.

### Tâches

| # | Tâche | Livrable | Dépendances |
|---|---|---|---|
| B1 | Implémenter le mécanisme d'intégration retenu (hook SCons ou équivalent) | `scripts/combus_scons_hook.py` (ou nom adapté) | A6, A12 |
| B2 | Déclarer les dépendances SCons sur les `.cb` / `.cbch` | Dans le hook | B1 |
| B3 | Ajouter le répertoire `.pio/build/<env>/generated/combus/` aux include paths | Dans le hook | B1 |
| B4 | Tester sur un env de test jetable (branche temporaire) | Rapport de test | B1, B2, B3 |
| B5 | Valider compilation + linkage avec un programme trivial | Build successful | B4 |
| B6 | Documenter le hook dans `platformio.ini` | Mise à jour `platformio.ini` | B1 |

### Gate Phase B → Phase C / D

- [ ] Le build PlatformIO exécute le générateur sans erreur.
- [ ] Les include paths sont corrects (compilation OK).
- [ ] Le linkage est OK (pas de symbole manquant).
- [ ] Le contrat protocolaire généré est identique au contrat legacy pour le thème `vbat`.
- [ ] Les `.inc` legacy sont **toujours en place** (double-run transitoire).

### Risques Phase B

| Risque | Mitigation |
|---|---|
| Mécanisme d'intégration non fonctionnel | Reprendre A12 ; tester sur un env minimal ; vérifier les logs PlatformIO. |
| Dépendances SCons non détectées | Forcer la regénération manuelle pour valider ; ajuster `Glob`. |
| Include path non propagé | Vérifier que `env.Append(CPPPATH=[...])` est bien appelé. |

---

## Phase C — Chaînes (`.cbch`)

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
| C7 | Documenter le mécanisme de découverte des `CbProc` (seconde passe — voir note §15) | Note dans `doc/` | C4 |

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

## Phase D — Migration thème par thème

**Objectif** : migrer chaque thème legacy (`.inc`) vers `.cb` un par un, avec validation complète avant suppression.

> **Note** : la Phase D peut démarrer dès que la Phase B est validée, indépendamment de l'état de la Phase C. Les thèmes peuvent être migrés sans que le câblage des chaînes soit encore en `.cbch`.

### Tâches (par thème)

| # | Tâche | Livrable | Dépendances |
|---|---|---|---|
| Dx.1 | Identifier les `.inc` du thème | Liste dans `doc/` | — |
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

## Phase E — Suppression legacy complète

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
| **A** | 2-4 jours | Schéma + parser + générateur pour 1 thème + validation hypothèse `post:`. |
| **B** | 1-2 jours | Intégration PlatformIO + tests d'intégration. |
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
| Validation de l'hypothèse `post:` | Phase A (A12) | Détermine le mécanisme d'intégration en Phase B. |
| Documentation du mécanisme de découverte `CbProc` | Phase C (C7) | Seconde passe, peut être documentée après. |
| Vérification de l'ordre `wire_end` | Avant Phase A | Déterminer son importance réelle dans le code C++. |

---

## Check-list globale (à cocher au fil de l'eau)

- [ ] Schéma `.cb` défini et validé.
- [ ] Schéma `.cbch` défini et validé.
- [ ] Générateur Python opérationnel (parser + canon + generator + md5).
- [ ] Mécanisme d'intégration PlatformIO validé (hook `post:` ou fallback).
- [ ] Test de cohérence CPPDEFINES opérationnel.
- [ ] Au moins un thème migré avec succès (Phase D pour ce thème).
- [ ] Déterminisme validé (MD5 stable entre runs).
- [ ] Contrat protocolaire identique au legacy sur tous les thèmes migrés.
- [ ] Tous les `.inc` supprimés (après validation complète).
- [ ] Tous les artefacts obsolètes nettoyés dans `src/`.
- [ ] Les 3 fichiers `combus_ids_*` générés et utilisés.
- [ ] Documentation à jour (board, core, ecosystem).

---

## Annexe — Ordre d'exécution recommandé

```
Phase A (vbat seul + validation hypothèse post:)
    │
    ▼
Phase B (intégration PlatformIO sur env de test)
    │
    ├──────────────────────────────┐
    ▼                              ▼
Phase C (chaîne failsafe      Phase D (migration thème
en .cbch — indépendant)       par thème — indépendant)
    │                              │
    └──────────────┬───────────────┘
                   ▼
              Phase E (suppression legacy + bascule finale)
```

**Points clés** :

- **A → B** : séquentiel (B dépend de la validation de l'hypothèse `post:` en A).
- **B → C et B → D** : C et D peuvent démarrer en parallèle après B.
- **C et D** : indépendants l'un de l'autre.
- **C + D → E** : E nécessite que C et D soient terminés.

Chaque phase produit un livrable observable (commit, build successful, test pass) avant de passer à la suivante.

---

## Annexe — Cible vs état actuel

Cette annexe décrit la **cible** (ce que le système doit devenir), pas l'état actuel du repo. Les références à des fichiers legacy sont conservées uniquement parce qu'elles font partie du périmètre de migration.

| Cible | Statut | Action |
|---|---|---|
| `.cb` / `.cbch` comme sources de vérité | À créer | Phase A, C |
| Headers générés dans `.pio/build/<env>/generated/combus/` | À créer | Phase B |
| Aucun artefact généré dans `src/` | À atteindre | Phase E |
| Aucun `.inc` legacy | À supprimer | Phase D (par thème), E (reste) |
| Aucun umbrella legacy | À supprimer | Phase E |
| `combus_md5.py` intégré au générateur | À refactorer | Phase E |
| 3 fichiers `combus_ids_*` générés (analog, digital, _remote) | À créer | Phase E |
| Couplage core ↔ machine résolu | À valider | Phase E |

---

## Annexe — Notes pour les revues futures

- **Découverte `CbProc`** : mécanisme à fixer en seconde passe (regex, AST, table explicite). Pas bloquant pour le proto.
- **Version majeure** : pas de semver tant que le système est en proto. `schema_version` et `generator_version` sont des entiers simples.
- **Ordre wire_end** : à vérifier dans le code C++ existant pour confirmer son importance protocolaire.
- **Test d'ordre de scan** : toujours dans un env de test jetable (branche temporaire, copie locale).
- **Hook `post:`** : hypothèse à valider en Phase A (A12), pas une décision figée.
- **Contrat protocolaire** : ce qui doit être identique au legacy, pas le texte du header.
- **Phase C indépendante de Phase D** : les deux pipelines (`.cb` et `.cbch`) peuvent être développés en parallèle.
