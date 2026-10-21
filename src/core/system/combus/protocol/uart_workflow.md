# Workflow UART du ComBus (relevé du code)

> **Statut** : relevé factuel de la chaîne UART telle qu'implémentée dans le
> code, du flag de build jusqu'à `loop()`. Relu contre le code le 2026-10-14
> (version d'origine : 2026-05-09).
>
> **À refaire lors du rework du module de communication** (voir
> `doc/roadmap.md`, « Rework du module de communication ComBus ») : la chaîne
> d'initialisation décrite ici est celle de l'UART statique actuel.
>
> Les sections trame, TX et RX (§5, §7, §8) sont résumées avec des renvois ;
> le détail de la trame et du handshake est dans
> [`frame/combus_frame_protocol.md`](frame/combus_frame_protocol.md).

---

## 1. Vue d'ensemble — 4 couches

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ENV (machine)                                                           │
│    com_init()  ──►  combus_uart_init()  ──►  combus_links_init()         │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  COUCHE PROTOCOLE                                                        │
│    combus_tx_init() / combus_rx_init()                                   │
│    combus_tx_update() / combus_rx_update()                               │
│    combus_frame_encode() / combus_frame_decode() / combus_frame_apply()  │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  COUCHE TRANSPORT (NodeCom)                                              │
│    uart_com_init()  ──►  NodeCom* (write/readByte/available)             │
│    uart_com_register_pool()  ──►  pool statique de UartCtx               │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  COUCHE PHYSIQUE                                                         │
│    HardwareSerial (Serial / Serial1 / Serial2)                           │
│    GPIO TX / RX pins (ComPort)                                           │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Build flags — point d'entrée

**Fichiers qui les détectent** : `src/machines/init/com/combus_uart_init.cpp`
(choix du `ComDev` et du port de la carte d'après le flag), `src/machines/init/hw/hw_init_com.cpp`
(pool de ports) et `src/machines/init/output/` (appel TX).

| Flag                  | Effet                                              |
|-----------------------|----------------------------------------------------|
| `-D COMBUS_UART=N`    | Full-duplex (TX + RX) sur UARTn                    |
| `-D COMBUS_UART_TX=N` | TX uniquement sur UARTn                           |
| `-D COMBUS_UART_RX=N` | `combus_uart_init()` n'est pas compilé : pas de RX seul dans l'env machine |
| (aucun)               | Aucun ComBus UART                                  |

`combus_uart_init.cpp` est entièrement conditionné par
`#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)`.

**N** = index du canal UART (0 = Serial/USB, 1 = Serial1, 2 = Serial2 sur ESP32).

### Extrait — `src/machines/init/com/combus_uart_init.cpp` (choix du lien)

```cpp
    // --- Resolve the ComDev, the UART number and the role of this end from the build flag ---
    #if defined(COMBUS_UART)
        constexpr uint8_t uartCh = COMBUS_UART;
        const ComDev*     dev    = &kComDevExtBi;         // full-duplex
        constexpr LinkEnd end    = LinkEnd::TX_RX;
    #else
        constexpr uint8_t uartCh = COMBUS_UART_TX;
        const ComDev*     dev    = &kComDevExtUni;        // TX only
        constexpr LinkEnd end    = LinkEnd::TX;
    #endif

    const ComPort* port = findUartPort(uartCh);            // the board port that is UART N
    const ComDevMap maps[] = { { dev, port, end } };
```

Le port est ensuite ouvert par `combus_links_init()` (`combus_links.cpp`,
`openPort`) : `uart_com_init(uart_serial_for(index), débit, txPin, rxPin, nom du
ComDev, &pinReg)`. La broche d'un bout qui n'émet pas (ou ne reçoit pas) est
passée à `-1` et n'est donc pas réservée. Le plafond de débit du port est
contrôlé à la compilation (`static_assert` de `combus_uart_init()`) et à
l'exécution (`combus_link_check`).

L'ancienne fonction `uart_init()` de `uart_com.cpp` n'existe plus.

---

## 3. Séquence d'initialisation — du build jusqu'à `main()`

### 3.1. `main.cpp` — point d'entrée

**Fichier** : `src/machines/main.cpp` (lignes 22-26 et 45 pour `loop()`).

```cpp
void setup() {
  machine_init();
}

void loop() {
  sys_manager_update(comBus);
  // ... chaînes de processors, FSM de runlevel ...
  output_update(comBus);
}
```

> À VÉRIFIER : un fichier `main.cpp` existe aussi à la racine du dépôt. Les
> deux n'ont pas été comparés ligne à ligne ; ce document décrit
> `src/machines/main.cpp`.

### 3.2. `machine_init()` — orchestrateur global

**Fichier** : `src/machines/init/init.cpp` (fonction `machine_init()`, logs omis).

```cpp
void machine_init() {
  sys_init();        // 1. pinReg + pool de ports UART + Serial0 claim + combus_init()
  hw_init();         // 2. drivers/servos/sig/battery
  input_init();      // 3. sources d'entrée physiques
  output_init();     // 4. com_init() -> combus_uart_init() -> combus_protocol_init_all()

  // --- 5. Boot-safe runlevel ---
  combus_set_analog(comBus, AnalogComBusID::RUNLEVEL, (uint16_t)DEF_RUNLEVEL, ChanLayer::SYSTEM);
  stopAllDcDrivers(machine);
  sleepAllDcDrivers(machine);
  disableAllDcDrivers(machine);

  // --- 6. Dashboard setup ---
  dashboard_machine_setup(&comBus, &machine, static_cast<uint8_t>(AnalogComBusID::CH_COUNT),
                          static_cast<uint8_t>(DigitalComBusID::CH_COUNT));

  // --- 7. Démarrage de la tâche dashboard (pause optionnelle : -D PAUSE_LOG_AFTER_INIT) ---
  dashboard_start_task();
}
```

### 3.3. `sys_init()` — registre de pins + pool UART + Serial0 + combus_init

**Fichier** : `src/machines/init/sys/sys_init.cpp` (lignes 30-59).

```cpp
void sys_init() {
    // --- 1. Pin registry (no deps: pure RAM init, first in sequence) ---
    pin_reg_init(pinReg, PinRegMaxEntry);

    // --- 2. Register the shared UART port pool (before any uart_com_init() claim) ---
    hw_init_com();

    // --- 3. Claim Serial0 in the UART pool if any debug or dashboard flag is set.
#if defined(DEBUG_ALL)  || defined(DEBUG_SYSTEM) || defined(DEBUG_INPUT) || \
    defined(DEBUG_HW)   || defined(DEBUG_COMBUS) || defined(DEBUG_DASHBOARD)
    (void) uart_com_init(&Serial, DEBUG_MONITOR_BAUD, -1, -1, "debug", &pinReg);
#endif

    // --- 4. Internal environment data parsing ---
    combus_init(static_cast<uint8_t>(AnalogComBusID::CH_COUNT),
                static_cast<uint8_t>(DigitalComBusID::CH_COUNT));
}
```

### 3.4. `hw_init()` — drivers

**Fichier** : `src/machines/init/hw/hw_init.cpp` (lignes 44-78).

```cpp
void hw_init() {
    // --- 1. Driver init ---
    dcDriverInit(machine);

    // --- 2. Servo init ---
    servoInit(machine);

    // --- 3. Signal device init ---
    sigDevInit(machine);

    // --- 4. Battery init ---
    vbat_init(&vBatSense);
}
```

### 3.5. `hw_init_com()` — pool UART

**Fichier** : `src/machines/init/hw/hw_init_com.cpp` (fonction `hw_init_com()`).

```cpp
void hw_init_com()
{
      // Static storage for the shared UART port pool — owned by the machine,
      // sized to this board's real capacity. Must outlive the program.
      // Every uart_com_init() claims a slot from this pool: the debug serial
      // (sys_init()) and the ComBus port (combus_uart_init()).
    static UartCtx s_uartPool[UartComMaxPorts];
    uart_com_register_pool(s_uartPool, UartComMaxPorts);
}
```

Appelée au début de `sys_init()`, avant la première réservation de port. Le pool est
enregistré sans condition de flag : le port de debug en a besoin même sans ComBus.

### 3.6. `output_init()` → `com_init()` → `combus_uart_init()`

**Fichier** : `src/machines/init/output/output_init.cpp` (lignes 26-32).

```cpp
void output_init() {
    com_init();    // no-op if no transport flag is defined
}
```

**Fichier** : `src/machines/init/com/com_init.cpp` (lignes 23-26).

```cpp
void com_init() {
    combus_uart_init();  // no-op if no ComBus UART flag is defined
}
```

**Fichier** : `src/machines/init/com/combus_uart_init.cpp` (fin de `combus_uart_init()`).

```cpp
    // --- Wiring of the link, and the caller-owned storage for it (its size IS the link count) ---
    const ComDevMap maps[] = { { dev, port, end } };
    constexpr uint8_t kLinkCount = 1u;

    static CombusTxState  txStates[kLinkCount];
    static CombusRxState  rxStates[kLinkCount];
    static ComLinkRuntime runtime[kLinkCount];
    static ComBusLink     links[kLinkCount];
    const ComLinksStorage storage = { txStates, rxStates, runtime, links };

    combus_links_init(maps, kLinkCount, storage, &pinReg);
```

**Fichier** : `src/core/system/combus/protocol/combus_links.cpp` (`combus_links_init`).
Pour chaque `ComDevMap` : `combus_link_check` (câblage valide), ouverture du port
(`openPort`), puis `combus_link_build`, qui remplit le `ComBusLink` : couche et
taille de trame d'après la portée, TX et/ou RX d'après le rôle du bout, contexte
de handshake et tampons RX pris dans `ComLinkRuntime`. Les pools TX et RX sont
ensuite enregistrés et `combus_protocol_init_all()` démarre les liens. Un câblage
refusé est journalisé et laissé inactif.

Points à retenir (lus dans ces fichiers) :
- le lien est de portée `INTRA_NODE` (étape 0.4b), soit `ChanLayer::SYSTEM` : la
  trame transporte les canaux de la vue `combus_system`, qui contient ceux de
  `combus_base` ;
- `link.target` n'est pas renseigné : aucune application automatique des trames
  reçues dans le ComBus (`combus_frame_apply` n'est pas appelé par ce lien) ;
- les pools d'états TX et RX sont des tableaux statiques dont la taille est le
  nombre de liens ;
- les tampons RX de `ComLinkRuntime` sont dimensionnés pour la vue complète (la
  plus grande), quelle que soit la portée du lien.

### 3.7. `combus_protocol_init_all()` / `combus_protocol_init()` — wire TX/RX au transport

**Fichier** : `src/core/system/combus/protocol/combus_protocol.cpp` (lignes 31-108, extrait).

```cpp
void combus_protocol_init_all( ComBusLink* links, uint8_t count )
{
    for (uint8_t i = 0u; i < count; ++i) {
        combus_protocol_init(i, &links[i]);
    }
}

void combus_protocol_init( uint8_t linkIdx, ComBusLink* link )
{
    // 1. Garde : link et link->com non nuls ; handshakeCtx obligatoire (sinon erreur loguée, return)
    // 2. LY4 : switch (link->layer) -> ctx->expectedMd5 = kCombus{Base|System|}ComBusMd5
    //          UNDEFINED -> log d'erreur FATAL + boucle infinie (arrêt du système)

    // 3. Contexte de handshake partagé TX/RX, câblé AVANT les init
    combus_tx_set_handshake_ctx(linkIdx, link->handshakeCtx);
    combus_rx_set_handshake_ctx(linkIdx, link->handshakeCtx);

    // 4. TX seulement si txCfg est présent
    if (link->txCfg) {
        combus_tx_init(linkIdx, link->com, *link->txCfg, link->txHz, link->layer);
    }
    // 5. RX seulement si rxCfg est présent
    if (link->rxCfg) {
        combus_rx_init(linkIdx, link->com, *link->rxCfg, link->analogBuf, link->digitalBuf, link->target, link->layer);
    }
}
```

Structure `ComBusLink` : `src/core/system/combus/protocol/combus_protocol.h` (section 2)
(champs `com`, `txCfg`, `txHz`, `rxCfg`, `analogBuf`, `digitalBuf`,
`handshakeCtx`, `layer`, `name`, `target`). `txCfg` et `rxCfg` sont des
`std::optional` : leur absence désactive le sens correspondant.

---

## 4. Configuration — sources de vérité

### 4.1. Configuration board (par environnement)

**Fichier** : `src/machines/config/boards/ESP32_8M_6S.h` (lignes 100-117).

```cpp
// built-in ESP32 ESP32-DevKitC V4 built-in serial port (connected to USB port).
static constexpr int8_t Txd0Pin = 1;    // ESP32 built-in TX pin
static constexpr int8_t Rxd0Pin = 3;    // ESP32 built-in RX pin

// Extension port — UART link to the sound node (or any future ext device).
inline HardwareSerial& SerialExt = Serial2;       // SerialExt maps to the physical UART port

static constexpr int8_t  TxdExtPin = 18;  // UART ext TX pin
static constexpr int8_t  RxdExtPin = 13;  // UART ext RX pin

// General UART hardware ceiling for all ext/com ports on this board.
static constexpr uint32_t UartMaxBaud = 115200u;

// Maximum number of simultaneously UART ports on this board (including Serial0/USB).
static constexpr uint8_t  UartComMaxPorts = 3u;
```

**Fichier** : `src/machines/config/machines/volvo_A60H_bruder/board/mainboard/ESP32_8M_6S/envCfg.cpp` (table `comPorts`).

```cpp
// Values from ESP32_8M_6S.h, included via envCfg.h → boards.h.
const ComPort comPorts[] = {
    ComPort::makeUart("USB",      { 0, Txd0Pin,   Rxd0Pin,   UartMaxBaud }),  // UART0 — USB / debug serial
    ComPort::makeUart("COM/GPIO", { 2, TxdExtPin, RxdExtPin, UartMaxBaud }),  // UART2 — extension port / ComBus link
};

const uint8_t comPortsCount = static_cast<uint8_t>(sizeof(comPorts) / sizeof(comPorts[0]));
```

**Struct** : `src/core/system/hw/node_com.h` (section 2).

```cpp
struct UartPortCfg {
    uint8_t  index;     ///< UART peripheral number (0 = Serial, 1 = Serial1, 2 = Serial2)
    int8_t   txPin;     ///< GPIO TX pin (-1 = not wired)
    int8_t   rxPin;     ///< GPIO RX pin (-1 = not wired)
    uint32_t maxBaud;   ///< hardware baud-rate ceiling of this port
};
```

`ComPort` regroupe le nom de la prise (celui de la sérigraphie de la carte), son
type (`ComPortType`) et la configuration propre à ce type. Seul l'UART existe.
Un port est retrouvé par son `uart.index`, l'ordre de la table est libre.

### 4.2. Configuration ComBus (protocole)

**Fichier** : `src/core/config/outputs/combus_uart.h` (lignes 55-77).

```cpp
static constexpr uint8_t ComBusUartFrameSize =
    CombusFrameHeaderLen
  + ((static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT) + 7u) / 8u)
  +  (static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT)  * 2u)
  + 1u;

static constexpr uint32_t ComBusUartMaxTxHz      = 200u;
```

Le débit et la fréquence d'émission du lien ne sont plus ici : ce sont les paramètres
(`ComProtocolCfg`) de son `ComDev`.

**Fichier** : `src/machines/config/machines/volvo_A60H_bruder/board/com_dev.h`.

```cpp
inline constexpr ComProtocolCfg kComProtoExt = { 115200u, 50u };

inline constexpr ComDev kComDevExtUni = {          // COMBUS_UART_TX
	"combus", LinkScope::INTRA_NODE, LinkType::UNIDIRECTIONAL,
	&kComProtoExt, LinkActivation::ALWAYS, nullptr
};

inline constexpr ComDev kComDevExtBi = {           // COMBUS_UART
	"combus", LinkScope::INTRA_NODE, LinkType::BIDIRECTIONAL,
	&kComProtoExt, LinkActivation::ALWAYS, nullptr
};
```

Le lien d'extension est intra-nœud : il est défini une fois pour le nœud, à la racine
de `board/`, et chaque carte le câble sur son port. `combus_uart_init()` vérifie à la
compilation `kComProtoExt.rate <= UartMaxBaud` et `kComProtoExt.txHz <= ComBusUartMaxTxHz`.

### 4.3. Configuration frame layout

**Fichier** : `src/core/system/combus/protocol/frame/combus_frame_defs.h` (lignes 45-48).

```cpp
struct ComBusFrameCfg {
    uint8_t nAnalog;   ///< Number of analog channels in this layout.
    uint8_t nDigital;  ///< Number of digital channels in this layout.
};
```

---

## 5. Format de trame binaire

Voir [`frame/combus_frame_protocol.md`](frame/combus_frame_protocol.md), §1 : `SOF`
`0xAA`, en-tête de 4 octets (`nAnalog`, `nDigital`, `seq`, `flags`), bits
numériques puis valeurs analogiques `uint16` petit-boutistes, CRC-8/MAXIM.
Le `runLevel` n'est pas dans l'en-tête : il voyage comme canal `RUNLEVEL`.

---

## 6. Abstraction `NodeCom` — interface transport-agnostique

**Fichier** : `src/core/system/hw/node_com.h` (lignes 46-52).

```cpp
struct NodeCom {
    void*       ctx;                                          ///< adapter private context
    void      (*write)    (void* ctx, const uint8_t* data, size_t len); ///< send bytes
    int       (*readByte) (void* ctx);                        ///< read one byte (-1 = empty)
    int       (*available)(void* ctx);                        ///< RX bytes waiting
    const char* name;                                         ///< port id
};
```

**Adapter UART** : `src/core/system/hw/transport/uart_com.cpp` (lignes 41-53).

```cpp
static void uart_write(void* ctx, const uint8_t* data, size_t len) {
    static_cast<UartCtx*>(ctx)->serial->write(data, len);
}

static int uart_readByte(void* ctx) {
    return static_cast<UartCtx*>(ctx)->serial->read();
}

static int uart_available(void* ctx) {
    return static_cast<UartCtx*>(ctx)->serial->available();
}
```

**Pool de ports** : `src/core/system/hw/transport/uart_com.h` (lignes 52-58).

```cpp
struct UartCtx {
    HardwareSerial* serial  = nullptr;  ///< Pointer to the associated HardwareSerial instance
    const char*     owner   = nullptr;  ///< Name of the module owning this port
    uint32_t        baud    = 0u;       ///< Baud rate recorded at init
    bool            claimed = false;    ///< Indicates if the port is already allocated
    NodeCom         com     = {};       ///< NodeCom instance linked to this port
};
```

---

## 7. TX — résumé

Le détail de la trame (en-tête, ordre des canaux, taille selon le layer du lien)
est dans [`frame/combus_frame_protocol.md`](frame/combus_frame_protocol.md).

- `combus_tx_init(linkIdx, nodeCom, frameCfg, txHz, layer)`
  (`combus_tx.cpp:88-162`) : mémorise le transport et le layout, calcule la
  période `1000 / txHz`, résout le nombre de canaux émis (`analogWireEnd` /
  `digitalWireEnd`) selon le layer du lien, arme la rafale de handshake. Un layer
  `UNDEFINED` arrête le système.
- `combus_tx_update(bus)` (`combus_tx.cpp:186-259`) : pour chaque lien enregistré,
  pilote la rafale de handshake, applique le timer d'émission, appelle
  `combus_frame_encode(cfg, buffer, bus, seq, analogWireEnd, digitalWireEnd)`,
  écrit sur le `NodeCom`, puis avance `seq` (1 à 255, jamais 0).
- `combus_frame_encode` : `combus_frame.cpp:111-195`. CRC-8/MAXIM
  (`combus_frame_crc8`, `combus_frame.cpp:33-47`) : valeur initiale 0,
  polynôme réfléchi `0x8C`.
- Il n'y a plus de paramètre `failSafe` : l'état failsafe voyage par le canal
  `FAILSAFE`.

---

## 8. RX — résumé

- `combus_rx_init(linkIdx, nodeCom, frameCfg, analogBuf, digitalBuf, target, layer)` :
  mémorise transport et layout, branche les tampons de l'appelant, déduit du
  `layer` du lien l'identité d'écriture des trames appliquées (un lien BOARD
  reçoit l'identité SYSTEM), remet à zéro l'état du lien et invalide le contrat
  de handshake.
- `combus_rx_update(bus)` : pour chaque lien, vide le transport dans un tampon
  circulaire puis décode tant que possible. Si `HAS_FAILSAFE` est défini et que le
  canal `FAILSAFE` de `bus` est actif, invalide le contrat de handshake du lien
  (`bus` peut être `nullptr` : la réaction est alors ignorée).
- `tryDecode` : cherche le `SOF`, lit l'octet `seq`
  (décalage 3) pour distinguer le handshake (`seq == 0`, chemin séparé) d'une
  trame de contrôle, vérifie la longueur et le CRC, puis, si le lien a une
  `target`, appelle `combus_frame_apply` avec l'identité d'écriture du lien.
- `combus_frame_decode` : `combus_frame.cpp:236-302` ; `combus_frame_apply` :
  `combus_frame.cpp:338-395`.

> **Aucun appel à `combus_rx_update()` dans `src/`** (recherche
> `combus_rx_update` : seuls les tests `test/test_combus_loopback/` l'appellent).
> La boucle principale de la machine ne fait que de la TX
> (`output_update` → `combus_tx_update`). Le lien UART de la machine n'a pas de
> `target`.

---

## 9. Handshake — contexte par lien

Chaque lien ComBus indépendant possède son propre `CombusHandshakeContext` :

**Fichier** : `src/core/system/combus/protocol/combus_links.h` (`ComLinkRuntime`).

```cpp
struct ComLinkRuntime {
    CombusHandshakeContext handshake = {};
    // ... tampons de réception du lien
};
```

Un `ComLinkRuntime` est fourni par lien (tableau statique de l'appelant) ;
`combus_link_build` y prend le contexte de handshake.

Le contexte est partagé entre TX et RX du même lien. `combus_protocol_init()`
le câble avant les init (`combus_tx_set_handshake_ctx(linkIdx, ctx)`,
`combus_rx_set_handshake_ctx(linkIdx, ctx)`, voir §3.7) et y résout le
pointeur `expectedMd5` selon le layer du lien.

Format de la trame de handshake, rafale au démarrage (3 trames, 100 ms),
validation MD5 + version, interrupteurs de compilation :
`src/core/system/combus/protocol/frame/combus_frame_protocol.md`, §8.

**Fichiers handshake** :
- `src/core/system/combus/protocol/frame/combus_handshake.h`
- `src/core/system/combus/protocol/frame/combus_handshake.cpp`
- `src/core/system/combus/protocol/frame/combus_handshake_tx.h/.cpp`
- `src/core/system/combus/protocol/frame/combus_handshake_rx.h/.cpp`

---

## 10. Loop — flux d'exécution

**Fichier** : `src/core/system/output/output_manager.cpp` (lignes 20-27) ; appelé en
fin de `loop()` (`src/machines/main.cpp:233`).

```cpp
void output_update(const ComBus& bus) {

	// --- ComBus UART TX (50 Hz timer-gated, non-blocking) ---
#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
  combus_tx_update(&bus);
#endif

}
```

Ordre d'un tour de `loop()` (`src/machines/main.cpp`, appels aux lignes 51, 57-58 et 233) :

1. `sys_manager_update(comBus)` : acquisition des entrées (`input_refresh`,
   `input_update`), `vbat_update`, `failsafe_update` (chaîne FAILSAFE), puis
   `remote_link_fallback_update` si `HAS_REMOTE_LINK_LOST_FALLBACK`
   (`src/machines/system/sys_manager.cpp:19-43`) ;
2. chaînes de processors d'entrée et de simulation (`proc_chain_update`) ;
3. machine à états des runlevels ;
4. `output_update(comBus)` : émission ComBus.

Aucun appel à `combus_rx_update()` dans ce tour (voir §8.2).

Le RX doit être appelé séparément par un consommateur via `combus_rx_update()` ;
aucun consommateur de ce type n'existe dans `src/` à ce jour (le côté
récepteur historique, le nœud son, a été retiré).

---

## 11. Récapitulatif — fichiers par couche

| Couche / Rôle                | Fichier                                                              |
|------------------------------|----------------------------------------------------------------------|
| **Point d'entrée**           | `main.cpp`                                                           |
| **Orchestrateur global**     | `src/machines/init/init.cpp`                                         |
| **System init**              | `src/machines/init/sys/sys_init.cpp`                                 |
| **Hardware init**            | `src/machines/init/hw/hw_init.cpp`                                   |
| **HW transport init**        | `src/machines/init/hw/hw_init_com.cpp`                               |
| **Output init**              | `src/machines/init/output/output_init.cpp`                           |
| **Com orchestrateur**        | `src/machines/init/com/com_init.cpp`                                 |
| **Wrapper env machine**      | `src/machines/init/com/combus_uart_init.cpp`                         |
| **Liens depuis ComDev**      | `src/core/system/combus/protocol/combus_links.cpp`                   |
| **Protocole orchestrateur**  | `src/core/system/combus/protocol/combus_protocol.cpp` (`combus_protocol_init_all`) |
| **TX protocole**             | `src/core/system/combus/protocol/combus_tx.cpp`                      |
| **RX protocole**             | `src/core/system/combus/protocol/combus_rx.cpp`                      |
| **Codec frame**              | `src/core/system/combus/protocol/frame/combus_frame.cpp`             |
| **Defs frame**               | `src/core/system/combus/protocol/frame/combus_frame_defs.h`          |
| **Handshake**                | `src/core/system/combus/protocol/frame/combus_handshake.cpp`         |
| **Interface transport**      | `src/core/system/hw/node_com.h`                                      |
| **Adapter UART**             | `src/core/system/hw/transport/uart_com.cpp`                          |
| **Port struct**              | `src/core/system/hw/node_com.h` (`ComPort`)                          |
| **Config ComBus**            | `src/core/config/outputs/combus_uart.h`                             |
| **Board pins (header)**      | `src/machines/config/boards/ESP32_8M_6S.h`                           |
| **Board ports (data)**       | `src/machines/config/machines/.../envCfg.cpp`                        |
| **Dispatch output**          | `src/core/system/output/output_manager.cpp`                          |

---

**Auteur** : Workflow documenté depuis le code existant.
**Date** : 2026-05-09
**Version** : 2.0 (relevé factuel avec extraits de code)
