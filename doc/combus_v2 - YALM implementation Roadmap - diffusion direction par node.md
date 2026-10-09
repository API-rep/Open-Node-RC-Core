# Roadmap — combus-builder : diffusion `direction` par node

> Chantier générateur, détaché du roadmap combus-handshake pour rester traitable indépendamment. Référencé depuis le roadmap handshake comme dépendance externe de `LY2`.

---

## Le problème

`direction` (`uplink`/`downlink`/`both`/`none`) est défini **une seule fois par combus**, dans le combus général (remote+local) — donc à ce stade, un `uplink` est `uplink` pour tous les nodes, ce qui n'a pas de sens en soi (vu de l'autre bout, ce doit être un `downlink`).

Complication : certains combus sont **optionnels par board** (activation conditionnelle d'un module — ex. un sensing batterie présent sur une seule carte). Sans mécanisme de diffusion, deux boards du même node peuvent finir avec des jeux de canaux différents à la compilation → **dérive silencieuse**, détectable seulement au runtime (voire pas du tout).

## Contrainte actée

Le combus **`LOCAL` doit être identique sur toutes les cartes d'un même node, par définition**. La complétion automatique n'est donc **pas optionnelle, elle est impérative** — sinon un module consommateur sur la carte B ne pourrait jamais lire un canal produit par un module actif seulement sur la carte A (ex. `BAT_LOW` produit par le sensing batterie sur une carte, consommé pour déclencher une alerte sur la carte son, **sans avoir à activer le sensing sur chaque carte**).

Conséquence structurelle : **un seul `combus_ids(REMOTE,LOCAL).h` par node** (jeu d'IDs commun), mais **`combus.h/.cpp` différents par board** au sein du même node (pour adapter `direction` à chacune). Un combus qui doit rester strictement privé à une carte est déclaré en scope **`SYSTEM`**, pas `LOCAL` — c'est ce scope qui sert d'échappatoire à la diffusion, pas une règle spéciale sur `LOCAL`.

---

## Roadmap

| # | Étape | Contenu | Effort | Dépend de |
|---|---|---|---|---|
| 1 | **ND1** | Formaliser explicitement la dimension "node" dans `combus_builder.py` (le regroupement de boards existe déjà implicitement via le layer LOCAL, mais n'a jamais été pensé pour piloter une diffusion) | à chiffrer | — |
| 2 | **ND2** | Diffusion automatique des combus `LOCAL` entre boards d'un même node, règle : `none → none`, `uplink → downlink`, `downlink → uplink`, `both → both`. Un combus absent d'une board est ajouté automatiquement avec la direction résultante | ~1 j | ND1 |
| 3 | **ND3** | Détection de conflits à la génération (erreur de build) : même combus `LOCAL` défini en `uplink` sur 2+ boards → conflit ; idem `downlink` → conflit ; `both`/`none` sur plusieurs boards → OK ; combus en `downlink` sans **aucun** `uplink` producteur dans le node → warning (canal mort) | ~0,5 j | ND2 |
| 4 | **ND4** | Scope `SYSTEM` : vérifier qu'il exclut déjà bien un combus de la diffusion ND2 (reste strictement privé à sa board), sinon l'implémenter avec cette sémantique | ~0,5 j | ND1 |
| 5 | **ND5** | Diffusion **REMOTE** : plus simple — générer l'instance combus de la télécommande en **inversant** `uplink`/`downlink` des canaux REMOTE du node. Pas de logique de conflit multi-board ici (un seul récepteur en face) | ~0,5 j | ND1 |
| 6 | **ND6** | Documenter/valider le mode **`BOTH_OR`** (déjà existant, développé pour le FAILSAFE partagé) : à l'import, un canal `both` reste `true` dès qu'au moins une board source le passe à `true` — extensible en `AND`, etc. Vérifier que ND2/ND3 restent compatibles : un `BOTH_OR` porté par 3+ boards n'est **pas** un conflit, c'est le comportement attendu | ~2 h (vérif + doc si déjà codé) | ND2, ND3 |
| 7 | **ND7** | ⚠️ **Bug découvert (session en cours), correctif validé** : le MD5 inclut `direction` comme input, or `ND2` inverse `uplink`/`downlink` entre boards d'un même node par construction — deux boards correctement configurées obtiendraient donc un MD5 `LOCAL` **systématiquement différent** (faux mismatch garanti, pas un cas limite, dès qu'un canal LOCAL asymétrique existe). **Correctif validé** : canoniser `direction` dans `combus_builder/md5.py` avant hash — remplacer `uplink`/`downlink` par une catégorie symétrique unique (`ONE_WAY`), garder `both`/`none` littéraux. Un canal qui change réellement de catégorie (`both`↔`uplink`, `none`↔`downlink`) reste détecté ; le miroir de rôle entre boards ne l'est plus (il ne doit pas l'être, `ND2` le garantit déjà par construction) | à chiffrer | ND1 — n'impacte pas le handshake (`LY4` inchangé, consomme juste les 3 MD5 déjà corrigés) |

**Ordre** : ND1 → ND2 → ND3 → (ND4, ND5 en parallèle, dépendent seulement de ND1) → ND6 → **ND7 (à faire avant tout usage réel du MD5 LOCAL en prod — sans lui, tout lien LOCAL entre deux boards distinctes échoue systématiquement au handshake dès qu'un canal asymétrique existe)**.

## Point ouvert

Le regroupement "quelles boards appartiennent à quel node" existe déjà implicitement (via le layer LOCAL), mais n'a jamais servi à piloter une diffusion automatique. À vérifier au moment de coder **ND1** que ce regroupement est suffisant tel quel, ou s'il doit être rendu plus explicite dans la config de build.

---

## Sortie de ce chantier — ce que le handshake attend en retour

⚠️ **Correction** : `LY2` (paramètre `layer`, borne `CH_COUNT`) ne dépend **pas** de ce chantier — il ne touche pas à `direction`, seulement au layer/scope, et les vues sont déjà stables aujourd'hui (confirmé sur les fichiers générés). C'est un ticket distinct côté handshake — **`DIR1` : filtrage TX/RX par `direction`** (à créer, voir roadmap handshake) — qui dépend de `ND2`/`ND3` : il a besoin d'un `direction` cohérent et sans conflit entre boards du même node pour filtrer correctement à l'émission/réception.

**Nuance sur le risque de dérive (mise à jour après découverte `ND7`)** : `direction` fait partie des inputs du MD5 généré (confirmé : "Hash inputs: id, type, scope, theme, direction, infoName"). Avant `ND7`, ça allait au-delà d'un simple filet de sécurité — **sans `ND7`, le MD5 `LOCAL` échoue systématiquement** entre deux boards d'un même node dès qu'un canal asymétrique existe, même en l'absence de toute dérive réelle (c'est le miroir uplink/downlink voulu par `ND2` qui le déclenche). `ND7` doit donc être fait **avant** toute utilisation réelle du MD5 `LOCAL`, pas juste "par confort de diagnostic" comme envisagé initialement. Une fois `ND7` fait, la lecture initiale reste valable : une vraie dérive de contrat (pas un simple miroir de rôle) continue à provoquer un échec de handshake sûr.