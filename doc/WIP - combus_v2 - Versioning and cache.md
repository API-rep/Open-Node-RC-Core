# ComBus — Versioning & Friend-Cache Design

Résumé de la discussion sur le versioning ComBus, tenue avant la
suppression de l'ancien mécanisme `CombusLayout` — cassé lors de la
migration du code de dispatch et retiré depuis en tant que régression
propre (voir commit associé). Le remplacement éventuel de ce mécanisme
est hors scope de ce document et devra être conçu séparément.

---

## 1. État actuel

- (Historique) `ComBusFrameCfg` transmettait, dans chaque frame, un champ
  documenté comme `envId` mais qui contenait en réalité une valeur
  `CombusLayout::MACHINE_TYPE` castée en `uint8_t`. Ce champ et l'enum
  `CombusLayout` ont été retirés (régression ciblée) ; toute future
  métadonnée de versioning devra être définie indépendamment, sans
  réutiliser cette ancienne convention de nommage.
- Le protocole ComBus est **stateless, streaming, best-effort** : chaque

  frame est indépendante, avec un compteur roulant `seq` (`uint8_t`, 0–255),
  tolérant nativement à la perte de frames.
- Distinction déjà posée dans `board_architecture.md` :
  - **Board** — sa propre MCU, son propre firmware compilé et flashé
    indépendamment (mainboard, extension board).
  - **Peripheral** — fixed-function, pas de MCU, version hardware figée à
    la fabrication, jamais reflashé indépendamment.
- Une même remote peut piloter plusieurs types de machines différents —
  c'est le cœur du projet, pas un cas marginal.
- ComBus est déjà segmenté en couches (layering de transmission), avec un
  flag uplink/downlink en préparation, et une QoS à deux canaux
  (basse/haute priorité, chacun avec son propre compteur et framing)
  envisagée pour plus tard.
- Les liaisons ComBus concernées sont génériques à tout niveau du projet :
  série point-à-point (mainboard ↔ extension board) et RF potentiellement
  multi-émetteurs (remote ↔ machine).

---

## 2. Besoin

- Détecter les mismatchs accidentels de version — combus layout **et**
  version logicielle — entre deux participants ComBus dont les firmwares
  sont flashés indépendamment. Protection "anti-étourderie" pour garder un
  parc hardware/logiciel cohérent, pas une protection contre un émetteur
  malveillant.
- S'applique à tout lien entre deux **Board** (node RF machine↔remote,
  *et* board série mainboard↔extension board) — jamais aux **Peripheral**,
  dont la version hardware figée ne peut pas driver.
- Rester léger : le surcoût par frame de contrôle normale doit rester
  minime (1–2 octets acceptables face à 16 canaux 16 bits), et le
  mécanisme ne doit pas introduire d'état de session classique
  (handshake avec accusé de réception, timeout de session, etc.) —
  incompatible avec le modèle stateless/best-effort existant.
- Concevoir un mécanisme de versioning robuste, sans sur-ingénierie
  (X-macro jugée peu utile pour ce besoin).


---

## 3. Solution retenue

- **Trame magique** : la valeur `seq == 0` est réservée exclusivement à
  une frame de handshake/validation (payload MD5 combus+version, adresse
  émetteur). Le compteur de frames de contrôle normal démarre à `1`
  après un reset et tourne en 1..255 (wrap 255 → 1, jamais 0).

  Côté TX, le compteur est tenu par `CombusTxState::seq` dans
  `src/core/system/combus/protocol/combus_tx.cpp`.  Côté RX, le décodeur
  `combus_rx.cpp::tryDecode()` peek le byte `seq` dès le scan SOF et
  route structurellement vers `combus_handshake_tryDecode()` (module
  dédié `combus_handshake.{h,cpp}`) — pas un simple `if` imbriqué dans
  le chemin de contrôle.  Cela permet au futur code de handshake de
  grossir sans refactor du chemin de contrôle.

  Ce mécanisme est générique à **tout** participant ComBus (node RF
  machine↔remote, ou board série mainboard↔extension board) — pas
  limité au lien RF.

- **Déclencheur de validation côté récepteur** : pas seulement
  `seq == 0`, mais la condition structurelle *"adresse absente du cache
  amies"* — couvre à la fois le cas où c'est l'émetteur qui reboote
  (repasse par `seq 0`) et celui où c'est le récepteur qui reboote (perd
  son cache alors que l'émetteur est déjà à un `seq` élevé).
- **Résilience à la perte de frame** : pas d'accusé de réception — la
  trame magique est émise en rafale courte au boot (3–5 répétitions),
  traitée de façon idempotente côté récepteur.
- **Portée** : capacité générique de tout lien ComBus entre deux
  participants Board — node (RF) ou board (série) indifféremment, jamais
  avec un peripheral.
- **Cache "amies"** : multi-entrées adressées par défaut partout, y
  compris sur liaison série point-à-point où une seule entrée sera
  réellement utilisée — pour éviter un cas spécial "board-à-board only"
  qui complexifierait le code sans bénéfice net. Le nombre d'entrées reste
  une constante/variable configurable par instance de lien.
- **Pas de revalidation périodique** pour l'instant : une seule validation
  par cycle de vie de la connexion physique est jugée suffisante.
- **Cas particulier retenu — cache à une seule entrée (slot unique)** :
  pour un lien où un seul contrôleur actif à la fois a du sens (ex. remote
  ↔ machine), le slot n'est **jamais écrasé silencieusement** par une
  nouvelle trame magique tant qu'un contrôleur est actif. Il n'est vidé
  que lorsque le véhicule bascule en runlevel `IDLE`/`SLEEPING` — bascule
  aujourd'hui déclenchée par le failsafe existant (perte de connexion,
  volontaire ou non). Une fois le slot vidé, toute remote (y compris
  l'ancienne, sans traitement spécial) doit repasser par une trame magique
  pour reprendre la main. Ce comportement réutilise directement la règle
  "adresse absente du cache = handshake-only" déjà posée plus haut, sans
  mécanisme supplémentaire.
  - Reprise de contrôle sans retour (uplink only) : gérée par retry manuel
    côté remote (pas de moyen de détecter l'échec autrement).
  - Avec retour (downlink) : la remote détecte l'échec via timeout sur
    l'accusé de réception attendu.
- MD5 choisi comme mécanisme de check ; à garder explicitement en tête que
  c'est un check de cohérence, pas une mesure de sécurité contre un tiers
  malveillant sur un lien broadcast.

---

## 4. Graines posées pour le futur

- **QoS à deux canaux (basse/haute priorité)** : la trame magique est une
  candidate naturelle pour vivre sur le futur canal haute priorité — elle
  doit arriver coûte que coûte, contrairement à une frame de contrôle
  normale que la suivante remplace de toute façon. Le code de validation
  actuel doit éviter de coder en dur l'hypothèse "canal unique" pour ne
  pas devoir être redéplacé lors de l'introduction de la QoS.

---

## 5. État d'avancement (roadmap)

> Snapshot à `b7614ff` (HEAD de `combus-frame-handshake` post-refonte
> `scripts/combus_md5.py` — scan récursif, agnostic au build). Cette section
> croise chaque point de la solution retenue avec son statut d'implémentation
> réel (commits, fichiers, scripts). Pas une wishlist — un constat.


### 5.1 Points traités ✅

| # | Point du WIP | Commit(s) | Fichier(s) / Livrable(s) |
|---|---|---|---|
| T1 | Régression `CombusLayout` (suppression enum + champ frame) | `0c5adfe` | `combus_frame_defs.h`, `combus_frame.{h,cpp}` |
| T2 | Réservation `seq == 0` pour handshake | `71a38e4` | `combus_rx.cpp::tryDecode()` — peek byte seq + route structurelle |
| T3 | Décodeur dédié handshake (pas de `if` imbriqué) | `71a38e4`, `e37f5aa`, `13b4c5f` | `combus_handshake.{h,cpp}` (umbrella) + `combus_handshake_rx.{h,cpp}` + `combus_handshake_tx.{h,cpp}` |
| T4 | Compteur `seq` côté TX (1..255, wrap 255→1, jamais 0) | `2d1e1b0`, `8dd57e6`, `71a38e4`, `41fc08f` | `CombusTxState::seq` dans `combus_tx.cpp` |
| T5 | Split RX/TX handshake | `13b4c5f` | `combus_handshake_rx.{h,cpp}`, `combus_handshake_tx.{h,cpp}` |
| T6 | Génération MD5 des `.inc` REMOTE (legacy — 1 .h par build, gate par MACHINE_*) | `13b4c5f` → supplanté en `b7614ff` | `scripts/combus_md5.py` → `<build_dir>/<pioenv>/combus_handshake_md5.h` — gardé à titre historique, **plus utilisé** depuis T6b |
| T6b | Génération MD5 par scan récursif (1 .h par paire `.inc`, agnostic au build) | `b7614ff` | `scripts/combus_md5.py` (réécrit) → `combus_ids_remote_md5.h` à côté de chaque paire `combus_ids_remote_{analog,digital}.inc` découverte sous `src/core/`. Découverte par présence de fichier, indépendante du build flag. |

| T7 | Dispatcher `machine_type.h` (TYPE → `<type>_config.h`) | `1e7612d` | `src/core/config/machines/machine_type.h` |
| T8 | Runtime umbrella TYPE (`combus_remote.{h,cpp}`) | `1e7612d` (créé) → `8409439` (supprimé) | `core/config/machines/<type>/combus/combus_remote.{h,cpp}` — **régression** : umbrella sans valeur ajoutée (re-export pur de `combus_ids_remote.h`), supprimé en `8409439`. Chaque environnement instancie désormais son runtime combus directement via son `combus.cpp` instance-specific. |

| T9 | Fix CRC re-sync + TX seq log | `41fc08f` | `combus_handshake_rx.cpp`, `combus_tx.cpp` |
| T10 | Renommage `combus_ids_remote_*.inc` → `combus_remote_*.inc` | `e37f5aa` | `core/config/machines/dumper_truck/combus/combus_remote_*.inc` |
| T11 | Déplacement codec trame vers `src/core/system/combus/frame/` | `e37f5aa` | `frame/combus_frame.{h,cpp}`, `frame/combus_frame_defs.h`, `frame/combus_handshake*.{h,cpp}` |
| T12 | Suppression dispatcher racine `combus_ids_remote.h` | `1e7612d` | `src/core/config/machines/combus_ids_remote.h` (supprimé) |
| T13 | Doc `tree_structure.md` synchronisée | `a90a48d` | `tree_structure.md` (racine) |
| T14 | Câblage payload MD5 dans handshake TX | `13b4c5f` | `combus_handshake_tx.cpp::combus_handshake_sendOnce()` — `frame[payloadStart + i] = combus::wire::kCombusWireMd5[i]` (16 octets MD5) puis `kProjectVersionMajor/Minor` (2 octets version) ; longueur totale 18u garantie par `static_assert` dans `combus_handshake.cpp`. **Clos le point 1b** du §5.5. |
| T15 | Flag `s_contractValidated` + lifecycle P2 | `bfc108f`, `89db20a`, `c3457bf`, `64ab934`, `7819194` | `combus_handshake.{h,cpp}` : flag statique + accesseur public `combus_handshake_is_contract_validated()` + bridge interne `markContractValidated()` / `clearContractValidated()`. `combus_handshake_rx.cpp` : `compareAndLog` retourne `bool` (match) ; `tryDecode` appelle `markContractValidated()` après match ; **`tryDecode` skip le MD5+version compare quand le flag est `true`** (optimisation runtime promise par le Doxygen du flag). `combus_rx.cpp::combus_rx_init()` : appelle `clearContractValidated()` (couplage cycle de vie transport). 4 tests Group C dans `test_combus_loopback.cpp` (clear-on-init, set-on-match, no-mark-on-mismatch, full-cycle init→match→re-init). Bypass `COMBUS_MD5_CHECK_DISABLE` retourne `false` et ne flippe jamais le flag → compare tourne à chaque frame en mode bypass (par design). **Clos le point P2** du §5.2. |



### 5.2 Points partiels 🟡 (infra posée, logique métier à finaliser)

| # | Point du WIP | Statut | Reste à faire |
|---|---|---|---|
| _P1_ | ~~Payload MD5 handshake~~ | ✅ Clos — voir T14 en §5.1 | — |
| _P2_ | ~~Déclencheur "adresse absente du cache amies"~~ | ✅ Clos — voir T15 en §5.1. Recadrage : pas de cache multi-entrées, juste un `bool valid` par lien, false au boot et après `combus_rx_init()`, true au premier match MD5+version réel. | — |

| P3 | Rafale courte au boot (3–5 répétitions) | Pas de code | Ajouter compteur de rafale dans `combus_handshake_tx.cpp`, démarrer au boot |

| P4 | Slot unique vidé au runlevel IDLE/SLEEPING | Concept documenté (délègue au failsafe) ; pas de hook | Ajouter callback `onRunlevelChanged()` dans le cache, abonné au failsafe existant |
| P5 | Failsafe ↔ handshake bridge | Failsafe séparé (`doc/WIP - Failsafe module design.md` ouvert) | Synchroniser les deux WIP — voir section 5.4 |

### 5.3 Points non démarrés ❌

| # | Point du WIP | Effort estimé | Dépendances |
|---|---|---|---|
| N1 | Cache multi-entrées (série point-à-point + RF multi-émetteurs) | ~1 jour-homme | P2 |
| N2 | Distinction uplink-only (retry manuel) vs downlink (timeout ACK) | ~2 jours-homme | P2, infra TX/RX handshake |
| N3 | Validation hardware bout en bout (tous les envs) | ~1 journée | T1–T10 stables, recompile flash |
| N4 | Mesure du surcoût par frame de contrôle normale | ~2 h | Bench UART |
| N5 | Documentation Doxygen du handshake (wire layout, offsets) | ~3 h | T11 figé |

### 5.4 Points bloqués / décisions ouvertes 🔴

| # | Sujet | Question ouverte | Impact |
|---|---|---|---|
| B1 | ~~Sémantique payload MD5~~ | **DÉCISION PRISE** : payload = `version (major, minor) + MD5(REMOTE .inc)`. Version mergée dans le hash via header `v<major>.<minor>\n`. Sémantique = identité complète du contrat combus (layout + version). | ✅ Résolu |
| B2 | Validation hardware | Aucun test live sur bench RF ni liaison série depuis `e37f5aa` | Bloque N3 |
| B3 | Graine QoS 2 canaux (section 4 du WIP) | Hors scope handshake actuel ; à planifier séparément | Aucun impact court terme |
| B4 | Failsafe ↔ handshake | Failsafe en cours de design dans son propre WIP | Doit converger avant P4 |
| B5 | ~~Résolution Remote .inc via macros~~ | ✅ Clos en `b7614ff` (T6b). Le parsing des macros `COMBUS_IDS_REMOTE_*_INC` a été remplacé par scan récursif `os.walk` de `src/core/` à la recherche de la paire `.inc`. Le rework de layering combus n'est plus nécessaire. | — |



### 5.5 Prochaines étapes (par ordre de priorité)

| Étape | Action | Pré-requis | Effort |
|---|---|---|---|
| 1 | ~~Décision B1~~ : ✅ tranchée — version + combus.remote | — | — |
| 1b | ~~Implémenter P1~~ : ✅ clos — voir T14 en §5.1. Le câblage MD5 dans `combus_handshake_tx.cpp::combus_handshake_sendOnce()` est en place (`kCombusWireMd5[16]` + `kProjectVersionMajor/Minor`), `static_assert` cohérence OK, longueur payload = 18u comme spécifié | — | — |
| 2 | **Implémenter P2** : cache multi-entrées + lookup | T3, T5 | ~4 h |
| 3 | **Implémenter P3** : rafale au boot | T3 | ~1 h |

| 4 | **Synchroniser failsafe ↔ handshake** (B4) | WIP failsafe avancé | ~2 h |
| 5 | **Implémenter P4** : vidage cache sur IDLE/SLEEPING | Étape 4 | ~1 h |
| 6 | **Validation hardware N3** | T1–T11 stables sur bench | 1 journée |
| 7 | ~~TODO post-rework layering combus~~ : ✅ clos en `b7614ff` (T6b). Le parsing des macros `COMBUS_IDS_REMOTE_*_INC` a été remplacé par scan récursif `os.walk` de `src/core/` à la recherche de la paire `.inc`. Le rework de layering combus (multi-root -I overlay) n'est plus un pré-requis. | — | — |



### 5.6 Statut du successeur de `CombusLayout`

**DÉCLARATION** : le présent WIP (section 3 "Solution retenue") constitue
**de facto le successeur** du mécanisme `CombusLayout` supprimé en
`0c5adfe`. Il est désormais explicitement piloté par le tableau ci-dessus
(§5.1 traité / §5.2 partiel / §5.3 non démarré / §5.4 bloqué) — plus
aucun successeur implicite à révéler.

- **QoS 2 canaux** : section 4 du WIP, graines uniquement, pas d'implémentation.


