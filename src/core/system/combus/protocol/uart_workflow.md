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
│    com_init()  ──►  combus_uart_init()  ──►  combus_protocol_init_all()  │
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
│    GPIO TX / RX pins (uartPins[])                                        │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Build flags — point d'entrée

**Fichiers qui les détectent** : `src/machines/init/com/combus_uart_init.cpp`
(résolution du canal et des pins), `src/machines/init/hw/hw_init_com.cpp`
(pool de ports) et `src/machines/init/output/` (appel TX).

| Flag                  | Effet                                              |
|-----------------------|----------------------------------------------------|
| `-D COMBUS_UART=N`    | Full-duplex (TX + RX) sur UARTn                    |
| `-D COMBUS_UART_TX=N` | TX uniquement sur UARTn                           |
| `-D COMBUS_UART_RX=N` | Réserve le pool de ports (`hw_init_com.cpp`) mais `combus_uart_init()` n'est pas compilé : pas de RX seul dans l'env machine |
| (aucun)               | Aucun ComBus UART                                  |

`combus_uart_init.cpp` est entièrement conditionné par
`#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)` (ligne 6).

**N** = index du canal UART (0 = Serial/USB, 1 = Serial1, 2 = Serial2 sur ESP32).

### Extrait — `src/machines/init/com/combus_uart_init.cpp` (lignes 52-65)

```cpp
    // --- Resolve UART channel and GPIO pins from build flag (was in uart_init()) ---
    #if defined(COMBUS_UART)
        constexpr int uartCh    = COMBUS_UART;
        const     int uartTxPin = uartPins[uartCh].tx;
        const     int uartRxPin = uartPins[uartCh].rx;
    #elif defined(COMBUS_UART_TX)
        constexpr int uartCh    = COMBUS_UART_TX;
        const     int uartTxPin = uartPins[uartCh].tx;
        constexpr int uartRxPin = -1;
    #endif

    // --- Open the ComBus UART port directly (no single-link helper in uart_com.h) ---
    NodeCom* com = uart_com_init(uart_serial_for(uartCh), ComBusUartBaud,
                                 uartTxPin, uartRxPin, "combus", &pinReg);
```

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
  sys_init();        // 1. pinReg + Serial0 claim + combus_init()
  hw_init();         // 2. hw_init_com() en premier, puis drivers/servos/sig/battery
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

### 3.3. `sys_init()` — registre de pins + Serial0 + combus_init

**Fichier** : `src/machines/init/sys/sys_init.cpp` (lignes 30-59).

```cpp
void sys_init() {
    // --- 1. Pin registry (no deps: pure RAM init, first in sequence) ---
    pin_reg_init(pinReg, PinRegMaxEntry);

    // --- 2. Claim Serial0 in the UART pool if any debug or dashboard flag is set.
#if defined(DEBUG_ALL)  || defined(DEBUG_SYSTEM) || defined(DEBUG_INPUT) || \
    defined(DEBUG_HW)   || defined(DEBUG_COMBUS) || defined(DEBUG_DASHBOARD)
    (void) uart_com_init(&Serial, DEBUG_MONITOR_BAUD, -1, -1, "debug", &pinReg);
#endif

    // --- 3. Internal environment data parsing ---
    combus_init(static_cast<uint8_t>(AnalogComBusID::CH_COUNT),
                static_cast<uint8_t>(DigitalComBusID::CH_COUNT));
}
```

### 3.4. `hw_init()` — drivers + transport

**Fichier** : `src/machines/init/hw/hw_init.cpp` (lignes 44-78).

```cpp
void hw_init() {
    // --- 0. Communication transport init (pin claim — must be first) ---
    hw_init_com();    // uart_com_register_pool() seulement (le port ComBus est ouvert plus tard par combus_uart_init())

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
#if defined(COMBUS_UART_TX) || defined(COMBUS_UART) || defined(COMBUS_UART_RX)
      // Static storage for the shared UART port pool — owned by the machine,
      // sized to this board's real capacity. Must outlive the program.
      // combus_uart_init() (called later from com_init()) will claim a slot
      // from this pool via uart_com_init().
    static UartCtx s_uartPool[UartComMaxPorts];
    uart_com_register_pool(s_uartPool, UartComMaxPorts);
#endif
}
```

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

**Fichier** : `src/machines/init/com/combus_uart_init.cpp` (lignes 30-108, commentaires abrégés).

```cpp
static constexpr uint8_t kBaseAnalogCount =
    static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT);
static constexpr uint8_t kBaseDigitalCount =
    static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT);

void combus_uart_init()
{
    static_assert(ComBusUartBaud <= UartMaxBaud,
                  "ComBusUartBaud exceeds board hardware ceiling UartMaxBaud");

    // P3 — caller-owned per-link handshake context.
    static CombusHandshakeContext s_linkHandshakeCtx = {};

    constexpr ComBusFrameCfg txCfg = { kBaseAnalogCount, kBaseDigitalCount };

    // ... résolution du canal UART et ouverture du port (voir §2) -> NodeCom* com

    // --- Build the ComBusLink descriptor (single source of truth) ---
    ComBusLink link{};
    link.com          = com;
    link.layer        = ChanLayer::BASE;
    link.handshakeCtx = &s_linkHandshakeCtx;
    link.name         = "combus";

    // --- TX side: COMBUS_UART_TX ou COMBUS_UART ---
    link.txCfg = txCfg;
    link.txHz  = ComBusUartTxHz;

    // --- RX side: seulement en full-duplex COMBUS_UART ---
    #if defined(COMBUS_UART)
      static uint16_t s_analog[kBaseAnalogCount];
      static bool     s_digital[kBaseDigitalCount];
      constexpr ComBusFrameCfg rxCfg = { kBaseAnalogCount, kBaseDigitalCount };
      link.rxCfg      = rxCfg;
      link.analogBuf  = s_analog;
      link.digitalBuf = s_digital;
    #endif

    ComBusLink links[] = { link };

    static CombusTxState txStates[sizeof(links) / sizeof(links[0])];
    combus_tx_register_pool(txStates, sizeof(links) / sizeof(links[0]));

    static CombusRxState rxStates[sizeof(links) / sizeof(links[0])];
    combus_rx_register_pool(rxStates, sizeof(links) / sizeof(links[0]));

    combus_protocol_init_all(links, sizeof(links) / sizeof(links[0]));
}
```

Points à retenir (lus dans ce fichier) :
- le lien est en `ChanLayer::BASE` : la trame transporte les canaux de la vue
  `combus_base` (`kBase*Count`) ;
- `link.target` n'est pas renseigné : aucune application automatique des trames
  reçues dans le ComBus (`combus_frame_apply` n'est pas appelé par ce lien) ;
- les pools d'états TX et RX sont des tableaux statiques dont la taille est le
  nombre de liens.

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
        combus_rx_init(linkIdx, link->com, *link->rxCfg, link->analogBuf, link->digitalBuf, link->target);
    }
}
```

Structure `ComBusLink` : `src/core/system/combus/protocol/combus_protocol.h:108-119`
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

**Fichier** : `src/machines/config/machines/volvo_A60H_bruder/mainboard/ESP32_8M_6S/envCfg.cpp` (lignes 151-157).

```cpp
// Values from ESP32_8M_6S.h, included via envCfg.h → boards.h.
const UartPinCfg uartPins[] = {
    { Txd0Pin,   Rxd0Pin   },  // [0] UART0 — USB / debug serial
    { -1,        -1        },  // [1] UART1 — unassigned on this board
    { TxdExtPin, RxdExtPin },  // [2] UART2 — extension port / ComBus TX link
};

const uint8_t uartPinsCount = static_cast<uint8_t>(sizeof(uartPins) / sizeof(uartPins[0]));
```

**Struct** : `include/struct/uart_struct.h` (lignes 22-25).

```cpp
struct UartPinCfg {
    int8_t tx;  ///< TX GPIO pin number (-1 = Arduino default / not used).
    int8_t rx;  ///< RX GPIO pin number (-1 = Arduino default / not used).
};
```

### 4.2. Configuration ComBus (protocole)

**Fichier** : `src/core/config/outputs/combus_uart.h` (lignes 55-77).

```cpp
static constexpr uint8_t ComBusUartFrameSize =
    CombusFrameHeaderLen
  + ((static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT) + 7u) / 8u)
  +  (static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT)  * 2u)
  + 1u;

static constexpr uint32_t ComBusUartBaud         = 115200u;
static constexpr uint32_t ComBusUartTxHz         = 50u;
static constexpr uint32_t ComBusUartMaxTxHz      = 200u;
```

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

- `combus_rx_init(linkIdx, nodeCom, frameCfg, analogBuf, digitalBuf, target)`
  (`combus_rx.cpp:253-291`) : mémorise transport et layout, branche les tampons
  de l'appelant, remet à zéro l'état du lien et invalide le contrat de handshake.
- `combus_rx_update()` (`combus_rx.cpp:306-344`) : pour chaque lien, vide le
  transport dans un tampon circulaire puis décode tant que possible. Si
  `HAS_FAILSAFE` est défini et que le canal `FAILSAFE` est actif, invalide le
  contrat de handshake du lien.
- `tryDecode` (`combus_rx.cpp:143-234`) : cherche le `SOF`, lit l'octet `seq`
  (décalage 3) pour distinguer le handshake (`seq == 0`, chemin séparé) d'une
  trame de contrôle, vérifie la longueur et le CRC, puis, si le lien a une
  `target`, appelle `combus_frame_apply` avec le layer `BASE`.
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

**Fichier** : `src/machines/init/com/combus_uart_init.cpp` (ligne 44).

```cpp
static CombusHandshakeContext s_linkHandshakeCtx = {};
```

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
| **Protocole orchestrateur**  | `src/core/system/combus/protocol/combus_protocol.cpp` (`combus_protocol_init_all`) |
| **TX protocole**             | `src/core/system/combus/protocol/combus_tx.cpp`                      |
| **RX protocole**             | `src/core/system/combus/protocol/combus_rx.cpp`                      |
| **Codec frame**              | `src/core/system/combus/protocol/frame/combus_frame.cpp`             |
| **Defs frame**               | `src/core/system/combus/protocol/frame/combus_frame_defs.h`          |
| **Handshake**                | `src/core/system/combus/protocol/frame/combus_handshake.cpp`         |
| **Interface transport**      | `src/core/system/hw/node_com.h`                                      |
| **Adapter UART**             | `src/core/system/hw/transport/uart_com.cpp`                          |
| **Pin struct**               | `include/struct/uart_struct.h`                                       |
| **Config ComBus**            | `src/core/config/outputs/combus_uart.h`                             |
| **Board pins (header)**      | `src/machines/config/boards/ESP32_8M_6S.h`                           |
| **Board pins (data)**        | `src/machines/config/machines/.../envCfg.cpp`                        |
| **Dispatch output**          | `src/core/system/output/output_manager.cpp`                          |

---

**Auteur** : Workflow documenté depuis le code existant.
**Date** : 2026-05-09
**Version** : 2.0 (relevé factuel avec extraits de code)
