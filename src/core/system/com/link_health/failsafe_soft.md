# Chaîne douce du failsafe (`FAILSAFE_SOFT`)

Ce document décrit la chaîne douce. La référence du module (les deux chaînes, les noms, les règles) est `src/core/system/failsafe/failsafe_module.md`.

Mis à jour le 2026-10-27 (link rework, lot « sécurité »). Avant S1, ce document s'appelait `remote_link_lost.md` et l'agrégat `REMOTE_LINK_LOST`.

---

## 1. Rôle

La chaîne douce agrège les fautes **légères**, aujourd'hui des pertes de commande : la télécommande locale ne répond plus, ou un lien vers un autre nœud est tombé. Sa réaction est la mise en attente, runlevel `IDLE`, et non l'arrêt complet `HALT` de la chaîne dure.

**Pourquoi deux chaînes** : perdre la télécommande un instant (changer de machine pilotée, sortir de portée) ne doit pas déclencher la séquence de sécurité complète. La machine s'arrête et attend.

**Reprise : manuelle.** Quand la faute disparaît, rien n'est écrit : la machine reste en `IDLE`, et c'est l'opérateur qui la redémarre avec la clé (Triangle). Même chose au premier branchement de la manette après le démarrage.

---

## 2. Architecture

```text
LOCAL_REMOTE_FAIL ──┐
                    ├──► reset + OR (cb_or_fn) ──► FAILSAFE_SOFT
NODE_LINK_FAIL ─────┘                                   │
                                                        ▼
                                             chaîne runlevel (front montant)
                                                        │
                                                        ▼
                                                 RunLevel::IDLE
```

La chaîne dure passe toujours avant (voir `failsafe_module.md` §6).

---

## 3. Contributeurs

| Contributeur | Fait | Écrit par | Flag |
|---|---|---|---|
| `LOCAL_REMOTE_FAIL` | la télécommande locale ne répond plus (manette PS4 aujourd'hui) | `input_update()`, `src/machines/system/input/input_update.cpp`, quand le backend est connecté (`INPUT_PS4_DS4_BT`) | `HAS_LOCAL_REMOTE_FAILSAFE` |
| `NODE_LINK_FAIL` | un lien qui reçoit, vers un autre nœud, n'est pas vivant (jamais vu compris) | `com_rx_update()`, `src/core/system/com/com_manager.cpp` | `HAS_NODE_LINK_FAILSAFE` |

Les deux sont des channels `BOARD` (privés à la carte qui porte la source), du groupe failsafe, `value: fault`. Ils sont réarmés à `true` par la chaîne après lecture : leur valeur instantanée n'est pas l'état du lien, il faut lire `FAILSAFE_SOFT`.

`NODE_LINK_FAIL` est alimenté par les liens vers un autre nœud (portée `INTER_NODE`, link rework S2). Un lien jamais vu compte tout de suite : pas de délai de grâce, contrairement à `BOARD_LINK_FAIL`.

---

## 4. Chaîne et agrégat

- **Code** : `src/core/system/com/link_health/remote_link_fallback_chain.cpp`, `kRemoteLinkFallbackChain[]`, lancée par `remote_link_fallback_update()` dans `sys_manager_update()`, après la chaîne dure et avant la machine à états.
- **Forme** : comme la chaîne dure. Un reset (agrégat à 0), puis un `cb_or_fn` par contributeur, chacun sous son flag.
- **Agrégat** : `FAILSAFE_SOFT` (`failsafe_soft.cb`), `SYSTEM`, `both_or`, partagé par toutes les cartes du nœud.
- **Activation** : `HAS_FAILSAFE`, comme tout le module. Sans contributeur, l'agrégat vaut `false` à chaque cycle.

---

## 5. Réaction

Dans la chaîne runlevel (`kRunlevelProcs[]`, `src/machines/config/machines/volvo_A60H_bruder/combus/processors/input/runlevel_config.h`), après la chaîne dure :

```cpp
static constexpr CbRunlevelCfg kRemoteLinkRunlevelCfg {
    .high  = RunLevel::IDLE,        // FAILSAFE_SOFT 0→1 : IDLE
    .low   = std::nullopt,          // FAILSAFE_SOFT 1→0 : rien, la reprise passe par la clé
    .claim = true,                  // bloque la clé pendant ce cycle
};
```

Le processor est `cb_runlevel_once_fn` : il n'agit que sur le **front montant**. La faute douce ne « tient » pas le runlevel, contrairement à la chaîne dure. Comme les commandes de clé viennent de la télécommande, elles ne peuvent de toute façon pas arriver tant que la télécommande est perdue.

---

## 6. Lecteurs

| Lecteur | Fichier | Usage |
|---|---|---|
| chaîne runlevel | `runlevel_config.h` | `IDLE` sur front montant |
| tableau de bord machine | `src/machines/system/debug/dashboard_machine.cpp` | indicateur de télécommande connectée |
| tableau de bord des entrées | `src/machines/system/debug/dashboard_input.cpp` | indicateur de bus piloté |

---

## 7. Référence rapide

| Concept | Où |
|---|---|
| Agrégat | `src/core/system/com/link_health/failsafe_soft.cb` |
| Chaîne | `src/core/system/com/link_health/remote_link_fallback_chain.cpp` |
| Contributeurs | `src/core/config/inputs/local_remote_fail.cb`, `src/core/system/com/link_health/node_link_fail.cb` |
| Réaction | `.../processors/input/runlevel_config.h` |
| Ordre d'exécution | `src/machines/system/sys_manager.cpp` |
