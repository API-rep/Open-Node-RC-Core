# Coding conventions

Ce document définit la manière d'écrire le code C/C++ du projet : nommage, casse, découpe des fonctions en étapes, structure des fichiers, et conventions spécifiques au code.

---

## 1. Naming and layout

| Élément | Convention | Exemple |
|---|---|---|
| Classes / types | `PascalCase` | `TemplateModule`, `TemplateConfig` |
| Types `enum` | `PascalCase` | `TemplateMode` |
| Valeurs d'`enum` | `UPPER_SNAKE_CASE` | `MODE_A`, `MODE_B` |
| Fonctions libres (API module C) | `snake_case` | `pin_reg_init`, `combus_tx_init` |
| Méthodes de classe | `lowerCamelCase` | `setDuty`, `isEnabled` |
| Macros `#define` | `UPPER_SNAKE_CASE` | `MAX_RETRIES` |
| Constantes `constexpr` | `PascalCase` | `kTractionRamp` |
| Champs de structure / variables | `lowerCamelCase` | `pinRegEntry`, `pinEntryCursor` |

- Les noms de variables doivent être explicites et lisibles, sauf pour les indices de boucles courts (`i`, `j`, `k`).

---

## 2. Structure des fichiers

### Ordre des includes

1. Le header correspondant (dans un `.cpp`).
2. Les headers du projet.
3. Les headers externes / bibliothèques.
4. Les headers standards C/C++.

### Espacement vertical

- **1 ligne vide** après les blocs d'include.
- **1 ligne vide supplémentaire** avant chaque bloc de section majeur (`// ===`).
- **1 ligne vide** avant les sections d'accès d'une classe (`public:`, `private:`).
- **2 lignes vides** entre la fin d'un bloc de code et le prochain bloc Doxygen (3 si le bloc précédent dépasse 40 lignes).
- **1 à 2 lignes vides** entre les implémentations de fonctions dans un `.cpp`.

---

## 3. Découpage des fonctions en étapes

- Découper les fonctions en étapes logiques.
- Chaque étape est introduite par un commentaire `// N. Step description` (un espace après `//`).
- Si une seule étape suffit, la numérotation est optionnelle.
- Une seule profondeur de sous-étape est autorisée : `// N.M Sub-step description`.
- La numérotation des étapes dans `@details` doit refléter la numérotation des commentaires `// N.` dans le corps de la fonction.
- Les commentaires d'étape sont indentés d'une tabulation de plus que la ligne de code suivante.

Exemple dans un `.cpp` :

```cpp
bool TemplateModule::begin(const TemplateConfig &config) {
// Basic guards
	if (config.frequencyHz == 0) {
		return false;
	}

// Save configuration
	_frequencyHz = config.frequencyHz;
	_invertPolarity = config.invertPolarity;

// Keep disabled until explicit enable
	_isEnabled = false;

	return true;
}
```

---

## 4. Checklist template header (`.h`)

Un fichier d'en-tête doit contenre, dans l'ordre :

1. Bloc Doxygen de fichier.
2. `#pragma once`.
3. Includes.
4. Sections majeures (`// ===`).
5. Lignes vides lisibles entre les blocs de déclaration.
6. Déclarations de types / classes.
7. Documentation API avec `///`.
8. Documentation membres avec `///<`.
9. Marqueur `// EOF <file>`.

---

## 5. Checklist template source (`.cpp`)

Un fichier source doit contenre, dans l'ordre :

1. Bloc Doxygen de fichier.
2. Include du header correspondant en premier.
3. Sections majeures par sujet.
4. Corps de fonction indentés avec des tabulations.
5. Fonctions non triviales documentées avec `/** ... */`.
6. Commentaires d'étape locaux (`// N. ...`).
7. Lignes vides lisibles entre les fonctions.
8. Marqueur `// EOF <file>`.

---

## 6. Conventions pratiques pour ce dépôt

- Conserver le style de section existant des fichiers `machine/` et `core/`.
- Garder des `@details` riches pour les bibliothèques réutilisables et la logique complexe.
- Préférer des commentaires concis pour le code évident et des notes détaillées pour les comportements non triviaux.

---

## 7. Formatage des sorties debug série

### 7.1 API du système de log (`debug.h`)

Le système de log actif est basé sur des templates (`log_impl<Level, ModuleEnabled>`). Aucune garde préprocesseur n'est nécessaire autour du code : toutes les branches mortes sont supprimées à la compilation via `if constexpr`.

**Wrappers génériques** (toujours actifs, filtrés par niveau uniquement) :

```cpp
log_err(fmt, ...);
log_warn(fmt, ...);
log_info(fmt, ...);
log_dbg(fmt, ...);
```

**Wrappers filtrés par module** (aussi filtrés par un flag module, retirés du binaire quand le flag n'est pas défini) :

```
hw_log_*(...)      → actif quand -D DEBUG_HW     (ou DEBUG_ALL)
input_log_*(...)   → actif quand -D DEBUG_INPUT  (ou DEBUG_ALL)
sys_log_*(...)     → actif quand -D DEBUG_SYSTEM (ou DEBUG_ALL)
combus_log_*(...)  → actif quand -D DEBUG_COMBUS (ou DEBUG_ALL)
```

**Niveaux** : `LogNone=0`, `LogError=1`, `LogWarn=2`, `LogInfo=3`, `LogDebug=4`. Seuil actif via `-D LOG_LEVEL=3` (défaut = 3 = info).

- Utiliser `hw_log_*` / `sys_log_*` / etc. pour toutes les traces liées à un module.
- Utiliser les wrappers génériques `log_err` / `log_info` uniquement pour des sorties non attachées à un module.
- Privilégier un texte lisible par un humain, puis machine-grep friendly quand c'est pertinent.
- Cette section est évolutive : ajouter des règles concrètes au fur et à mesure que les modules debug convergent.

### 7.2 Préfixe de ligne

Chaque ligne de debug doit commencer par un préfixe de module fixe :

```
[<MODULE>] <message>
```

- `<MODULE>` : identifiant court en majuscules (exemples : `INPUT`, `COMBUS`, `HW`, `SYSTEM`).
- Le tagging sous-module abandonne le module externe quand le contexte est déjà établi par l'indentation ou une ligne d'en-tête parente :
  - Ligne de premier niveau : `[HW] Hardware init start`
  - Ligne de sous-niveau (indentée) : `[DRV] DC drivers config check ... OK` (pas de répétition `[HW]`)
  - Utiliser `[MODULE][SUBMODULE]` uniquement quand la ligne apparaît isolée sans contexte parent (par exemple des lignes d'erreur pouvant être grepées hors contexte).
- Garder un style cohérent sur tout le projet quand c'est possible (préférer `[MODULE][SUBMODULE]` pour les lignes isolées).
- Ne pas inclure `<STAGE>` dans chaque ligne ; le contexte d'étape est fourni par l'introduction / l'en-tête de séquence.
- La sévérité (`INFO` / `WARN` / `ERROR`) est transmise par la couleur ANSI quand `SerialAnsi` est actif :
  - couleur par défaut : `INFO`
  - jaune (`\033[33m`) : `WARN`
  - rouge (`\033[31m`) : `ERROR`
- Si la couleur terminal n'est pas disponible (`SerialAnsi=0`), utiliser des tags explicites dans le corps du message (`[WARN]`, `[ERROR]`).
- Garder les tags de module stables et en majuscules pour simplifier le filtrage série et le grep.

Exemples :

```
[INPUT] PS4 controller setup started
[HW][DRV] Driver wakeup sequence started
[HW] Servo count exceeds recommended limit   (tag jaune)
[COMBUS] Invalid channel index               (tag rouge)
```

### 7.3 Formatage des appels de log

Chaque appel de log doit tenir sur **une seule ligne** — ne jamais couper la chaîne de format et ses arguments sur plusieurs lignes. Cela s'applique quelle que soit la longueur de ligne ; la lisibilité de l'appel de log prime sur la limite de colonnes.

```cpp
// correct
sys_log_warn("[VBAT] \"%s\" low! %.2f V < %.2f V\n", vBatSense->cfg[idx].infoName, vBatSense->state[idx].voltage, cutoff);

// wrong — split across lines
sys_log_warn("[VBAT] \"%s\" low! %.2f V < %.2f V\n",
  vBatSense->cfg[idx].infoName, vBatSense->state[idx].voltage, cutoff);
```

### 7.4 Carte des sous-modules recommandée (premier draft)

| Module | Sous-modules recommandés | Usage typique |
|---|---|---|
| `INPUT` | `PS4`, `MAP`, `WATCHDOG` | lecture device, mapping entrée, perte signal / failsafe |
| `COMBUS` | `AN`, `DG`, `SYNC` | bus analogique, bus digital, chemins sync/reset |
| `HW` | `DRV`, `SRV`, `CFG` | drivers DC, servos, vérifications config matérielle |
| `SYSTEM` | `BOOT`, `STATE`, `LOOP` | séquence démarrage, transitions runlevel, snapshots runtime |

- Garder les tags de sous-module courts, en majuscules et stables dans le temps.
- Si un nouveau sous-module est introduit, l'ajouter ici pour garder une nomenclature cohérente.

### 7.5 Sortie hybride (mode normal uniquement)

- Pour l'instant, ne définir que le format de sortie en mode normal.
- Reporter les règles super-verbeuses à une itération ultérieure.
- Garder l'ordre des champs stable pour la lisibilité :
  1. title line
  2. board/parent
  3. config
  4. pins
  5. max speed
  6. Com channel
- `Mode` doit inclure un nom lisible plus la valeur numérique (exemple : `TWO_WAY_NEUTRAL_CENTER (1)`).
- `Parent` est optionnel et n'est affiché que s'il est présent.
- Les valeurs héritées doivent être marquées avec ` [INHERITED]` (et peuvent être grisées quand la couleur est disponible).
- Pour les lignes d'attachement d'init DRV, préférer un format de phrase lisible plutôt qu'un bloc compact clé/valeur.
  - Préféré : `[DRV] DRV_0 attached to pin 32 at 16000Hz frequency`
  - Suffixe clone : n'ajouter que ` (clone mode)`
  - Garder les lignes de détails matériels optionnels (états enable/sleep) hors du flux d'init ; les réserver pour un bloc final / verbeux.
- Pour l'installation du helper LEDC fade (`ledc_fade_func_install`), l'installer une seule fois par runtime (état statique gardé) pour éviter le spam d'erreur `fade function already installed`.

Exemple (mode normal) :

```
[HW][DRV] DC_DEV #6 - trailer rear left motor
  > Board Port: M-3B (ID:6) | Parent: DC_DEV #1 (CLONE)
  > Config: Freq:16000 Hz [INHERITED] | PolInv:YES [INHERITED] | Mode:TWO_WAY_NEUTRAL_CENTER (1)
  > Pins: PWM:0 BRK:15 EN:33 SLP:25 FLT:34
  > Max Speed FW: 100.0% | BK: 100.0%
  > Com Ch: THROTTLE (ID:1)
```

---

## 8. Langue

- Tous les commentaires, la documentation Doxygen et les messages de log sont rédigés en **anglais**.
