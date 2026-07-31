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
  émetteur). Le compteur de frames de contrôle normal redémarre à `1`
  après un reset, jamais `0`.
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
- Si un vrai besoin de sécurité (anti-usurpation) apparaît un jour, le

  mécanisme actuel (MD5 sans clé secrète) ne le couvre pas — à revisiter
  hors scope actuel le cas échéant.
- **Failsafe actuel identifié comme fragile** : `IDLE`/`SLEEPING` est
  aujourd'hui assignable librement par n'importe quel process système, et
  la détection de perte de connexion repose sur un flag de mise à jour
  ComBus activé/reset à chaque loop. Fonctionnel pour l'instant, mais à
  migrer côté input pour plus de robustesse — migration qui devra aussi
  couvrir le cas d'absence d'input (véhicule autonome, `INPUT_MODULE_NONE`)
  où ce flag n'a par définition personne pour le mettre à jour.