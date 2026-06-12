# IA guidelines

Fichier de démarrage obligatoire pour tout agent IA générant ou modifiant du code, des commentaires ou de la documentation dans ce projet.

**Principe** : ce fichier résume ce qu'une IA doit faire et ne pas faire. Il renvoie systématiquement vers les documents détaillés pour les règles de formatage, de code et de Doxygen. Les seuls contenus propres à ce fichier sont les instructions de comportement spécifiques aux agents IA.

---

## 1. Hiérarchie des documents

Lire et appliquer les documents dans cet ordre :

1. **`formatting.md`** — tout ce qui est visuel : en-têtes, sections, espaces, indentation, commentaires, fin de fichier.
2. **`coding-conventions.md`** — tout ce qui est code C/C++ : nommage, découpe en étapes, structure des fichiers, logs debug.
3. **`doxygen-conventions.md`** — tout ce qui est documentation Doxygen : `@brief`, `@details`, focus `.h` vs `.cpp`.
4. **`ia-guidelines.md`** (ce fichier) — comportement attendu d'un agent IA et résolution des ambiguïtés.

---

## 2. Consignes absolues (non négociables)

Ces points ne sont pas redétaillés ici ; ils sont développés dans les documents ci-dessus. Un agent IA les applique systématiquement.

- **Langue** : tout est en anglais (commentaires, documentation, messages de log).
- **Indentation** : tabulations, y compris dans les blocs préprocesseur.
- **En-tête** : bloc `/* ... */` avec ligne d'astérisques à 80 caractères, contenant `@file` et `@brief`.
- **Sections** : séparateurs `// ===` à 80 caractères, titres en majuscules.
- **commentaires** : `// Step description`, indentés d'une tabulation de plus que le code suivant.
- **Étapes** : commentaires `// N. Step description`, indentés d'une tabulation de plus que le code suivant.
- **Doxygen** :
  - `.h` : `///` pour l'API, `///<` pour les membres.
  - `.cpp` : `/** ... */` pour les fonctions non triviales.
- **`@brief`** : style "rôle d'abord", une ligne, pas de verbe en premier.
- **Fin de fichier** : marqueur `// EOF <file>`.

> Pour les détails et les exemples, voir `formatting.md`, `coding-conventions.md` et `doxygen-conventions.md`.

---

## 3. Règles spécifiques aux agents IA

Ces règles ne figurent pas dans les autres documents. Elles concernent exclusivement le comportement d'un agent IA.

### 3.1 Ne pas inventer

- Ne pas inventer de nom de fonction, de type, de macro ou de constante qui n'existe pas déjà dans le fichier modifié ou dans ses headers inclus.
- Ne pas inventer de sous-module de log non listé dans `coding-conventions.md` §7.4.
- Ne pas inventer de valeur d'`enum` ou de champ de `struct` : utiliser ceux déjà définis.

### 3.2 Préserver le style existant

- Quand un fichier existant est modifié, adopter le style déjà en place (tabulations, nommage, style de commentaires, ordre des sections).
- Ne pas convertir du code existant d'un style correct vers un autre style, même si ce dernier est aussi valide.
- Ne pas réorganiser un fichier sans demande explicite.

### 3.3 Priorité en cas de conflit

Si deux règles semblent contradictoires, appliquer cet ordre de priorité :

1. Style déjà présent dans le fichier modifié.
2. `formatting.md` et `coding-conventions.md` pour les règles de base.
3. `doxygen-conventions.md` pour la documentation.
4. Ce fichier (`ia-guidelines.md`) pour les consignes de comportement IA.

### 3.4 Cas d'ambiguïté

- **Nommage inconnu** : choisir la convention la plus proche définie dans `coding-conventions.md` §1 ; si le contexte est un module existant, suivre le style du module.
- **Longueur de ligne** : les lignes de log debug ne sont jamais coupées (voir `coding-conventions.md` §7.3). Pour le reste, préférer la lisibilité à une limite stricte, sans dépasser massivement 80-100 caractères.
- **Commentaire utile ou superflu** : si le code est évident, ne pas ajouter de commentaire. Si le comportement est non trivial, ajouter un commentaire d'étape ou un `@details`.
- **Documentation d'une fonction existante non documentée** : documenter selon `doxygen-conventions.md`. Si la fonction est triviale, un `@brief` suffit ; si elle est non triviale, ajouter `@details` et `@param`/`@return` pertinents.

### 3.5 Fichiers générés

- Ne jamais écrire de code sans bloc d'en-tête Doxygen.
- Ne jamais laisser de sections vides sans marqueur de section approprié.
- Toujours terminer par `// EOF <file>`.

---

## 4. Checklist IA simplifiée

Avant de valider une modification, vérifier au minimum :

- [ ] En-tête `/* ... */` à 80 caractères avec `@file` et `@brief`.
- [ ] `#pragma once` dans les `.h`.
- [ ] Sections `// ===` à 80 caractères si le fichier en contient.
- [ ] Tabulations pour l'indentation.
- [ ] `@brief` correct : nom de rôle d'abord, une ligne.
- [ ] Pas de doublon de documentation inutile.
- [ ] Pas d'information inventée.
- [ ] Style du fichier existant respecté.
- [ ] `// EOF <file>` en fin de fichier.

> Pour les détails complets, voir :
> - `formatting.md` §1, §2, §5, §8
> - `coding-conventions.md` §1, §2, §3
> - `doxygen-conventions.md` §3, §4

---

## 5. Exemples visuels de référence

Les templates complets se trouvent dans le même dossier :

- `template_module.h`
- `template_module.cpp`

Ces fichiers illustrent un rendu conforme. Un agent IA peut s'y référer pour comprendre l'application concrète des règles.

---

## 6. Ce qu'il faut éviter

- Ne pas répéter dans un fichier les règles déjà présentes dans `formatting.md`, `coding-conventions.md` ou `doxygen-conventions.md`.
- Ne pas ajouter de sections génériques non spécifiques à l'IA dans ce fichier.
- Ne pas ignorer le style déjà en place dans un fichier existant.
