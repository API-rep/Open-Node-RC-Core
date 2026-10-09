# ND2 — Rapport d'investigation (ÉTAPE 0)

**Date** : 2026-09-13
**Branche** : `combus-builder-multiboard`
**Pré-requis** : ND1 commité (85a918b)

---

## Question 1 — Étapes exactes de `canonize()`

**Fichier** : `scripts/combus_builder/canon/__init__.py` (lignes 93-111)

Le pipeline de `canonize()` est composé de **3 étapes**, dans cet ordre :

```python
def canonize(parsed: list[tuple[Path, str, Any]]) -> CanonResult:
    sections = extract_channels_sections(parsed)   # ÉTAPE 1
    active = merge_channels(sections)              # ÉTAPE 2
    canonical = canonize_channels(active)          # ÉTAPE 3
    return CanonResult(
        active_definitions=active,
        canonical_definitions=canonical,
    )
```

### ÉTAPE 1 — `extract_channels_sections(parsed)`
- **Entrée** : `list[tuple[Path, str, Any]]` (sortie de `discover_and_parse()`).
- **Sortie** : `list[tuple[Path, list[Any]]]` — pour chaque fichier, la liste brute des channels (sans validation).
- **Rôle** : extraction par clé (`channels:`), pas de validation, pas de déduplication.
- **Erreurs** : `ChannelValidationError` si `channels:` est `null` ou n'est pas une liste.

### ÉTAPE 2 — `merge_channels(sections)`
- **Entrée** : `list[tuple[Path, list[Any]]]` (sortie de l'étape 1).
- **Sortie** : `list[ChannelDefinition]` — liste validée, dédupliquée par `id`, non triée.
- **Rôle** : pour chaque channel brut, appelle `validate_channel()` (qui produit un `ChannelDefinition` normalisé), puis insère dans un `dict[str, ChannelDefinition]` indexé par `id`. Si un `id` est déjà présent, lève `ChannelConflictError` (ou `ChannelValueConflictError` si les `value` diffèrent).
- **Erreurs** : `ChannelValidationError`, `ChannelConflictError`, `ChannelValueConflictError`.

### ÉTAPE 3 — `canonize_channels(active)`
- **Entrée** : `list[ChannelDefinition]` (sortie de l'étape 2).
- **Sortie** : `list[ChannelDefinition]` — même liste, triée par `(scope, type, theme, id)`.
- **Rôle** : tri canonique stable. Pas de validation, pas de déduplication.

---

## Question 2 — `merge_channels()` est-elle la SEULE fonction qui applique la contrainte d'unicité ?

**Réponse : OUI.**

J'ai vérifié les trois étapes de `canonize()` :

| Étape | Fonction | Applique l'unicité par `id` ? |
|-------|----------|-------------------------------|
| 1 | `extract_channels_sections()` | NON — extraction pure, pas de déduplication. |
| 2 | `merge_channels()` | **OUI** — utilise `dict[str, ChannelDefinition]` indexé par `id`, lève `ChannelConflictError` sur doublon. |
| 3 | `canonize_channels()` | NON — tri pur, pas de déduplication. |

**Confirmation** : `merge_channels()` (dans `canon/channels.py`, lignes 636-675) est la **SEULE** fonction qui :
1. Maintient un index par `id` (`by_id: dict[str, ChannelDefinition]`).
2. Lève `ChannelConflictError` sur doublon d'`id`.

Aucune autre étape de `canonize()` ne suppose "un seul `id` dans le pool brut". Le pipeline actuel est conçu pour que `merge_channels()` soit le point de déduplication unique.

---

## Question 3 — `resolve_requires()` opère-t-il sur une liste déjà dédupliquée ?

**Réponse : OUI.**

**Fichier** : `scripts/combus_builder/generator.py` (lignes 263-286)

```python
def resolve_requires(
    channels: list[ChannelDefinition],
    ctx: BuildContext,
) -> list[ChannelDefinition]:
    resolved: list[ChannelDefinition] = []
    for ch in channels:
        if not ch.requires:
            resolved.append(ch)
            continue
        missing = sorted(req for req in ch.requires
                         if not (ctx.has(req) or ctx.value_of(req) is not None))
        if missing:
            raise RequiresResolutionError(ch.id, missing, ch.source_path)
        resolved.append(ch)
    return resolved
```

**Confirmation** :
- `resolve_requires()` est appelé par `generate()` (ligne 889) avec `active = resolve_requires(channels, ctx)`, où `channels` est la sortie de `canonize()` (donc déjà passée par `merge_channels()`).
- À ce stade, il n'existe **littéralement qu'UNE `ChannelDefinition` par `id`** (garanti par construction par `merge_channels()`).
- `resolve_requires()` ne fait que **filtrer actif/inactif** (canal dont les `requires` ne sont pas satisfaits par le `BuildContext`). Il ne choisit **jamais** entre plusieurs candidats pour un même `id` — il n'y en a qu'un seul.

---

## Conclusion — Compatibilité avec le design proposé à l'étape 1

**Le design proposé est COMPATIBLE avec la structure réelle.**

Le point d'insertion proposé est :
- **AVANT** `canonize()` : insérer `diffusion.py` qui prend les données brutes parsées (`list[tuple[Path, str, Any]]`) et le `NodeContext`, et produit `dict[board_id, list[raw_channel_decl]]`.
- **Pour chaque board** : appeler `canonize()` sur SA liste résolue (au lieu d'un `canonize()` global unique).

### Pourquoi c'est compatible :

1. **`canonize()` prend `list[tuple[Path, str, Any]]`** (la sortie de `discover_and_parse()`). On peut construire une telle liste par board à partir des données brutes + diffusion.

2. **Chaque liste par board contient au plus une déclaration par `id`** (par construction, après diffusion). Donc `merge_channels()` ne voit jamais de doublon et reste **INCHANGÉ** — sa contrainte d'unicité est respectée par construction, elle est juste vérifiée après diffusion au lieu d'avant.

3. **`resolve_requires()` opère déjà sur une liste dédupliquée** (garanti par `merge_channels()`). Aucune modification nécessaire.

4. **Le tri canonique (`canonize_channels()`) est par-board**, ce qui est correct car chaque board peut avoir un set différent de channels actifs (selon son `BuildContext`).

### Point d'attention — `extract_channels_sections()`

`extract_channels_sections()` est appelé en interne par `canonize()`. Il prend `list[tuple[Path, str, Any]]` et extrait la section `channels:`. Pour le mode node, on doit construire une `list[tuple[Path, str, Any]]` par board, où chaque tuple contient un `Path` (fichier source) et un `dict` avec une clé `channels:` contenant la liste résolue pour ce board.

**Note** : le `Path` dans le tuple est utilisé par `validate_channel()` pour les messages d'erreur. Pour les channels diffusés (synthétiques), on peut utiliser un `Path` virtuel (ex. `<diffusion:board_id>`) pour tracer l'origine.

---

## Question ouverte — Cas hétérogène (à remonter)

Le cahier des charges mentionne :
> combinaison hétérogène (ex. un `uplink` + un `both`) : à signaler dans le rapport de l'étape 0/1 comme cas non couvert par la table du roadmap fourni — ne pas inventer de règle, me remonter la question avant de choisir un comportement.

**Cas concret** : channel LOCAL `BAT_LOW` déclaré `uplink` sur `mainboard` ET `both` sur `soundboard`.

**Question** : quelle règle appliquer ?
- Option 1 : erreur fatale (comme deux `uplink`).
- Option 2 : `both` "absorbe" `uplink` → résultat = `both` sur les deux boards.
- Option 3 : `both` "absorbe" `uplink` → résultat = `both` sur `mainboard`, `both` sur `soundboard` (déjà le cas).
- Option 4 : autre ?

**Recommandation** : à confirmer avec le mainteneur avant de coder. Le comportement par défaut prudent serait **Option 1 (erreur fatale)** car la sémantique de `both` vs `uplink` est différente (unidirectional vs bidirectionnel), et autoriser une fusion silencieuse pourrait masquer un vrai conflit de design.

---

## Validation — Pas de modification de `merge_channels()`

Conformément au cahier des charges :
- `merge_channels()` dans `canon/channels.py` reste **INCHANGÉ**.
- La contrainte d'unicité est respectée par construction (chaque board a au plus une déclaration par `id` après diffusion).
- Aucune autre étape de `canonize()` ne suppose "un seul `id` dans le pool brut".

---

## Prochaine étape

Si le design proposé est validé (et la question hétérogène tranchée), on peut passer à l'**ÉTAPE 1** (implémentation) :
1. Créer `scripts/combus_builder/diffusion.py`.
2. Modifier `scripts/combus_scons_hook.py` pour insérer l'appel à `diffusion.py` avant `canonize()` dans la branche node mode, et déplacer l'appel à `canonize()` pour qu'il tourne par board.
3. Ajouter les tests demandés à l'étape 3.
