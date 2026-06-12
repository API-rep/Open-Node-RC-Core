# Doxygen conventions

Ce document décrit les conventions internes de documentation Doxygen du projet, avec un focus particulier sur la différence de contenu et de profondeur entre les fichiers `.h` (perspective appelant) et `.cpp` (perspective mainteneur).

---

## 1. En-tête de fichier

- Tout fichier commence par un bloc Doxygen court.
- `@brief` est optionnel : quand il est omis, Doxygen utilise la première phrase/paragraphe comme brief.
- `@details` n'est utilisé que s'il apporte une information utile.

```c
/******************************************************************************
 * @file template_module.h
 * Short module purpose.
 *
 * @details Optional: architecture role, constraints, usage scope.
 *****************************************************************************/
```

---

## 2. Politique Doxygen par type de fichier

### Fichiers `.h`

- Utiliser des sections majeures pour séparer les parties principales.
- Utiliser `///` pour les documentations d'API d'une ligne ou les petits blocs de déclaration.
- Utiliser `///<` pour les membres et les getters/setters inline.
- Utiliser `/** ... */` pour les documentations de classe ou les déclarations principales.

### Fichiers `.cpp`

- Utiliser `/** ... */` pour décrire les comportements de fonctions non triviales (`@details`, `@param`, `@return` quand pertinent).
- Garder les commentaires internes uniquement là où la logique nécessite une clarification.
- Laisser **une ligne vide** entre le bloc Doxygen et le code qu'il documente.
- Entre la fin d'un bloc de code et le prochain bloc Doxygen :
  - **2 lignes vides** par défaut.
  - **3 lignes vides** si le bloc de code précédent dépasse 40 lignes.

---

## 3. Style d'écriture du `@brief` — rôle d'abord, détail ensuite

Chaque `@brief` (dans `.h` ou `.cpp`) suit le même modèle :

```
<Role noun/phrase>. <One-sentence explanation of what it contributes in context.>
```

Règles :

- Commencer par un **nom ou une courte phrase nominale** nommant le rôle, pas par un verbe.
  - ✓ `Upper travel limit selector. Use software margin when configured, otherwise hw hard stop.`
  - ✗ `Returns the upper limit.`
- Une seule phrase après le point — expliquer *pourquoi* cet élément existe dans le contexte du module, pas seulement *ce qu'il fait*.
- Pas de jargon technique qu'un lecteur non anglophone devrait chercher dans un dictionnaire (`clamp` est acceptable, `saturate` ne l'est pas).
- Tenir sur **une seule ligne** — si ce n'est pas possible, le `@brief` est trop long.

Exemples :

```cpp
/** @brief Upper travel limit selector. Use software margin when configured, otherwise hw hard stop. */
/** @brief Lower travel limit selector. Use software margin when configured, otherwise hw hard stop. */
/** @brief Value clamper. Keep @p val within [@p lo, @p hi] to stay within travel limits. */
```

Anti-patterns à éviter :

```cpp
/** @brief Effective upper limit — margin when present, otherwise hw. */   // ✗ pas de nom de rôle, passif
/** @brief Returns the clamped value. */                                    // ✗ verbe en premier, pas de contexte
/** @brief Saturate @p val within range. */                                 // ✗ jargon (saturate)
```

---

## 4. Profondeur Doxygen : `.h` vs `.cpp`

Une fonction est documentée **deux fois** : une fois pour l'appelant, une fois pour le mainteneur.

### Dans `.h` — perspective appelant (what and why)

- `@brief` sur une ligne : rôle + but en une phrase.
- `@details` optionnel : table de sélection d'algorithme, règles de pointeurs, matrice de mode — tout ce qui aide l'appelant à configurer correctement la fonction sans lire l'implémentation.
- `@param` / `@return` : signification de chaque paramètre pour l'appelant ; unités, règles null, ownership.
- Pas de structure de code, pas de séquence d'étapes, pas de noms de variables internes.

```cpp
/**
 * @brief Validate a MotionConfig before first use.
 *
 * @details Algorithm selection by pointer pattern (ramp XOR gear+inertia):
 *
 *   Mode         | ramp  | gear  | inertia
 *   -------------|-------|-------|--------
 *   Simple ramp  |  set  |  null |  null
 *   Traction     |  null |  set  |  set
 *
 *   Value checks: hw->maxHwVal > minHwVal, band within effective limits, ...
 *
 * @param cfg  Config to validate. Must not be null.
 * @return     True when valid. False + fatal log on first violation.
 */
bool motion_check(const MotionConfig* cfg);
```

### Dans `.cpp` — perspective mainteneur (how and why it works)

- Même `@brief` que dans `.h` — cela garde les deux fichiers auto-contenus.
- `@details` **obligatoire pour les fonctions non triviales** : séquence d'étapes numérotées correspondant aux commentaires `// N.` du corps. Lire ce bloc seul doit donner une carte mentale complète de la fonction sans ouvrir le code.
- Les détails techniques vont ici : arithmétique de pointeurs, modèle de timing, transitions de machine à états, hooks moteur sonore, etc.
- Objectif : naviguer dans le code d'un coup d'œil, même pour quelqu'un qui ne connaît pas le module.

```cpp
/**
 * @brief Validate a MotionConfig before first use.
 *
 * @details Checks in order, stops on the first failure:
 *   1. hw and band are non-null (mandatory for all modes).
 *   2. Algorithm coherence: ramp XOR (gear + inertia).
 *   3. hw->maxHwVal > hw->minHwVal.
 *   4. margin (when present) within hw limits, max > min.
 *   5. dead-band within the effective limits (margin or hw).
 */
bool motion_check(const MotionConfig* cfg)
{
	// ...
}
```

La liste numérotée reflète les commentaires `// N.` à l'intérieur du corps de la fonction — le lecteur peut passer de la documentation au code sans friction.

---

## 5. `@brief` + `@details` pour les fonctions séquentielles

Quand une fonction est un pipeline séquentiel, le `@brief` reste sur une ligne (rôle d'abord) et la séquence numérotée va dans `@details` :

```cpp
/**
 * @brief Validate a MotionConfig before first use.
 *
 * @details Checks in order, stops on the first failure:
 *   1. hw and band are non-null (mandatory for all modes).
 *   2. Algorithm coherence: ramp XOR (gear + inertia).
 *   3. hw->maxHwVal > hw->minHwVal.
 *   4. margin (when present) within hw limits, max > min.
 *   5. dead-band within the effective limits (margin or hw).
 */
```

---

## 6. `@details` dans l'en-tête de fichier (`.h` uniquement) — vue d'ensemble module

Le `@details` au niveau fichier (en haut d'un `.h`) décrit le module dans son ensemble : son thème, son intention de design, son architecture et ses concepts clés. C'est le point d'entrée pour quiconque ouvre le fichier pour la première fois.

- Mettre l'accent sur les **responsabilités et les relations entre composants**, pas sur les détails d'implémentation.
- Structurer en courts paragraphes de prose, un par sujet, dans l'ordre de lecture naturel :
  1. **Paragraphe 1 — But / thème** : ce que fait ce module et pourquoi il existe.
  2. **Paragraphe 2 — Architecture / concepts** : types clés, règles d'ownership, patterns utilisés (pattern de pointeur, registre, machine à états…).
  3. **Paragraphe 3 — Usage / contraintes** : qui appelle quoi, dans quel ordre, règles de cycle de vie.

Règles :

- Toujours utiliser des `` `backticks` `` pour les identifiants (fonctions, champs, types, tableaux).
- Pas de listes à puces — rédiger des phrases en prose.
- Ne pas décrire *comment* le code fonctionne ligne par ligne ; décrire *quel rôle il joue* et *qui en dépend*.

Exemple :

```cpp
/**
 * @details `uart_com_init()` claims one entry in the static `ports[]` registry
 *   and fills its metadata, then returns a `NodeCom*` pointing into that entry.
 *
 *   Each `NodeCom` embeds a `ctx` pointer back to its `UartCtx`, so the
 *   port callbacks can retrieve the correct `HardwareSerial*` at runtime.
 *
 *   Because the registry is statically allocated, the returned `NodeCom*`
 *   remains valid for the lifetime of the program.
 */
```

---

## 7. Langue

- Toute la documentation Doxygen est rédigée en **anglais**.
