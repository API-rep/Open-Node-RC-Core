# Fusion `both_or` — l'anti ping-pong

Comment un channel `both_or` (par exemple `FAILSAFE`) est partagé entre les
cartes : la règle, la mécanique, un exemple, et ce qui se passerait sans elle.
Mis en place le 2026-10-22 (étape 1.1b du WIP *ComBus link rework*).

## 1. La règle

Un channel `both_or` est une valeur que toutes les cartes partagent. **Toute
valeur positive, locale à la carte ou venant d'une carte distante, rend le
channel vrai** sur la carte. Pour `FAILSAFE` : il suffit qu'une carte soit en
défaut pour que tout le nœud soit en failsafe, et le failsafe retombe quand plus
aucune carte n'est en défaut.

Deux pièges sont à éviter :

- **écraser** : si la trame reçue écrasait la valeur de la carte, le calcul local
  (la chaîne `FAILSAFE`) l'effacerait au cycle suivant, ou inversement ;
- **le ping-pong** : si une carte renvoyait à une autre la valeur qu'elle vient
  d'en recevoir, chacune entretiendrait celle de l'autre et un `true` ne
  retomberait jamais.

## 2. La mécanique

Pour chaque channel `both_or`, chaque carte garde **trois choses séparées** :

| Terme | Ce que c'est | Où |
|---|---|---|
| la valeur **hôte** | ce que le code de la carte écrit lui-même (la chaîne `FAILSAFE` la recalcule à chaque cycle, à partir de `false` : le *clear*) | `DigitalComBus::localValue` |
| le **cache** des entrées | la dernière valeur reçue de **chaque lien**, une par lien | l'instantané de réception du lien (`combus_rx_snapshot`) |
| la valeur **fusionnée** | ce que lisent tous les consommateurs (runlevel, sorties, tableau de bord) | `DigitalComBus::value` |

Quatre règles :

1. **Fusion** : `valeur = hôte OU (cache des liens vivants)`, réévaluée à chaque
   cycle (`com_rx_update`) et à chaque écriture locale (`combus_set_digital`).
   Une trame reçue n'écrase jamais la valeur : elle met seulement à jour le cache
   de son lien.
2. **Émission sans écho** : sur le lien L, la carte envoie
   `hôte OU (cache des liens AUTRES que L)`. Elle ne renvoie jamais à L ce que L
   lui a envoyé. Avec un seul lien, elle envoie simplement sa valeur hôte.
3. **Une carte qui n'a jamais écrit le channel ne contribue pas** : son terme hôte
   vaut `false` (`localWritten`). Une carte qui reçoit `FAILSAFE` par diffusion
   sans le calculer garde sa valeur par défaut (`high`) dans le slot ; sans cette
   règle, elle l'enverrait à tout le monde, et la machine resterait en failsafe
   dès le démarrage. Elle ne fait que relayer.
4. **Un lien perdu ne compte plus** : sa dernière valeur reçue est périmée. La
   sécurité est alors assurée par la politique de perte du lien (`onLoss`).

En formules, pour une carte B reliée aux liens L1…Ln :

```text
hôte(B)        = localValue si localWritten, sinon false
valeur(B)      = hôte(B) OU cache(Li)              pour tous les Li vivants
envoyé(B → Lk) = hôte(B) OU cache(Li)              pour tous les Li vivants, i ≠ k
```

## 3. Exemple

Trois cartes d'un nœud, reliées à la carte principale M. E (extension moteurs)
calcule son propre `FAILSAFE` (par exemple une surchauffe de driver). S (carte
son) ne le calcule pas : elle le reçoit par diffusion.

```text
      E ──── L1 ──── M ──── L2 ──── S
```

| Instant | Événement | E (hôte → valeur) | M (hôte → valeur) | S (valeur) | M envoie sur L1 | M envoie sur L2 |
|---|---|---|---|---|---|---|
| t0 | tout va bien | 0 → **0** | 0 → **0** | **0** | 0 | 0 |
| t1 | E surchauffe | 1 → **1** | 0, reçoit 1 de L1 → **1** | reçoit 1 de M → **1** | 0 OU (L2 : 0) = **0** | 0 OU (L1 : 1) = **1** |
| t2 | E refroidit | 0 → **0** | 0, reçoit 0 de L1 → **0** | reçoit 0 → **0** | 0 | 0 |

À t1, M passe en `FAILSAFE` ; sa chaîne de runlevel écrit `RUNLEVEL = FAILSAFE`,
qui est diffusé à E et S. À t2, tout retombe de soi-même (la sortie du runlevel
`FAILSAFE` reste manuelle, à la clé).

**Sans l'émission sans écho**, à t1 M renverrait `1` à E ; à t2, E n'a plus de
cause mais reçoit `1` de M, qui reçoit `1` de E : le ping-pong entretient le
défaut pour toujours. **Si S contribuait sa valeur par défaut**, elle enverrait
`1` en permanence et M ne sortirait jamais du failsafe.

## 4. Dans le code

| Quoi | Où |
|---|---|
| champs `localValue`, `localWritten`, `remoteValue` | `src/core/system/combus/combus_defs.h` (`DigitalComBus`) |
| écriture locale : garde l'hôte à part, recalcule la valeur | `combus_set_digital()`, `combus_access.cpp` |
| terme hôte, mise à jour du cache fusionné | `combus_both_or_local()`, `combus_both_or_set_remote()`, `combus_access.h` |
| une trame n'écrase pas un `both_or` | `combus_frame_apply()`, `frame/combus_frame.cpp` |
| OU des liens vivants, avec un lien exclu | `combus_rx_peer_or()`, `combus_rx.cpp` |
| fusion à chaque cycle | `com_rx_update()`, `src/core/system/com/com_manager.cpp` |
| émission sans écho, lien par lien | `combus_tx_update()`, `combus_tx.cpp` (valeurs passées à `combus_frame_encode()`) |
| délai de perte des liens qui reçoivent | `combus_links_watch()` → `combus_rx_set_link_timeout()` |

Tests : groupe L de `test/test_combus_loopback` (fusion, retour à `false`, pair
silencieux, pas d'écho, carte qui n'écrit jamais).

## 5. Limites

- Seuls les channels numériques `both_or` sont fusionnés. `uplink_or` et
  `downlink_or` restent déclaratifs : ce sont des OU entre plusieurs producteurs,
  que la diffusion refuse aujourd'hui (deux `uplink` = erreur de build).
- Un relais (`passthrough`, étape 3.2) devra suivre la même règle d'émission
  sans écho.
- La perte d'un lien est traitée par sa politique `onLoss`, pas par la fusion.
