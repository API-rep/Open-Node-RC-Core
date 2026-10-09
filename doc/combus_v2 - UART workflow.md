# ComBus v2 — Workflow UART (relevé du code)

> **Statut** : Document de référence — relevé factuel du workflow UART tel
> qu'implémenté dans le code aujourd'hui. Du build-flag jusqu'à `main.cpp`.
>
> **But** : Cartographier toute la chaîne UART avec les fichiers et les
> extraits de code concrets. Aucune spéculation sur des évolutions futures.

---

## 1. Vue d'ensemble — 4 couches

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ENV (machine / sound)                                                   │
│    com_init()  ──►  combus_uart_init()  ──►  combus_protocol_init()      │
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

**Fichiers qui les détectent** : `src/core/system/hw/transport/uart_com.cpp`,
`src/core/system/combus/protocol/combus_protocol.cpp`,
`src/machines/init/com/combus_uart_init.cpp`,
`src/sound_module/init/com/combus_uart_init.cpp`.

| Flag                  | Effet                                              |
|-----------------------|----------------------------------------------------|
| `-D COMBUS_UART=N`    | Full-duplex (TX + RX) sur UARTn                    |
| `-D COMBUS_UART_TX=N` | TX uniquement sur UARTn                           |
| `-D COMBUS_UART_RX=N` | RX uniquement sur UARTn                           |
| (aucun)               | Tout compile en no-op inline                       |

**N** = index du canal UART (0 = Serial/USB, 1 = Serial1, 2 = Serial2 sur ESP32).

### Extrait — `src/core/system/hw/transport/uart_com.cpp` (lignes 159-181)

```cpp
void uart_init(uint32_t baud, uint8_t maxChannels, PinReg* reg)
{
    s_maxChannels = (maxChannels <= UART_HW_CHANNEL_MAX) ? maxChannels : UART_HW_CHANNEL_MAX;

    // --- Resolve UART channel and GPIO pins from build flag ---
#if defined(COMBUS_UART)
    constexpr int uartCh    = COMBUS_UART;
    const     int uartTxPin = uartPins[uartCh].tx;
    const     int uartRxPin = uartPins[uartCh].rx;
#elif defined(COMBUS_UART_TX)
    constexpr int uartCh    = COMBUS_UART_TX;
    const     int uartTxPin = uartPins[uartCh].tx;
    constexpr int uartRxPin = -1;
#else  // COMBUS_UART_RX
    constexpr int uartCh    = COMBUS_UART_RX;
    constexpr int uartTxPin = -1;
    const     int uartRxPin = uartPins[uartCh].rx;
#endif

    // --- Open UART port once ---
    s_com[uartCh] = uart_com_init(uart_serial_for(uartCh), baud,
                                  uartTxPin, uartRxPin, "combus", reg);
}
```

---

## 3. Séquence d'initialisation — du build jusqu'à `main()`

### 3.1. `main.cpp` — point d'entrée

**Fichier** : `main.cpp` (racine du projet).

```cpp
#include <init.h>   // transitivement : const.h, struct.h, defs.h, config.h,
                    //                sys_init.h, hw_init.h, input_init.h, output_init.h

void setup() {
    machine_init();   // src/machines/init/init.cpp
}

void loop() {
    // ... boucle applicative
}
```

### 3.2. `machine_init()` — orchestrateur global

**Fichier** : `src/machines/init/init.cpp` (lignes 39-101).

```cpp
void machine_init() {
    // --- 1. System init ---
    sys_init();        // pinReg + Serial0 claim + combus_init()

    // --- 2. Hardware init ---
    hw_init();         // hw_init_com() en premier (claim pins) puis drivers/servos/sig/battery

    // --- 3. Input init ---
    input_init();      // sources d'entrée physiques (PS4 BT, etc.)

    // --- 4. Output init ---
    output_init();     // com_init() → combus_uart_init() → combus_protocol_init()

    // --- 5. Boot-safe runlevel ---
    combus_set_runlevel(comBus, DEF_RUNLEVEL, ChanLayer::LOCAL);
    stopAllDcDrivers(machine);
    sleepAllDcDrivers(machine);
    disableAllDcDrivers(machine);

    // --- 6. Dashboard setup ---
    dashboard_machine_setup(&comBus, &machine, ...);

    // --- 7. Post-init pause (optionnel, -D PAUSE_LOG_AFTER_INIT) ---
    // --- 8. Start dashboard FreeRTOS task ---
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
    hw_init_com();    // uart_com_register_pool() + uart_init()

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

### 3.5. `hw_init_com()` — pool UART + ouverture du port ComBus

**Fichier** : `src/machines/init/hw/hw_init_com.cpp` (lignes 14-24).

```cpp
void hw_init_com() {
#if defined(COMBUS_UART_TX) || defined(COMBUS_UART) || defined(COMBUS_UART_RX)
    // Static storage for the shared UART port pool — owned by the machine,
    // sized to this board's real capacity. Must outlive the program.
    static UartCtx s_uartPool[UartComMaxPorts];
    uart_com_register_pool(s_uartPool, UartComMaxPorts);

    uart_init(ComBusUartBaud, UartComMaxPorts, &pinReg);
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

**Fichier** : `src/machines/init/com/combus_uart_init.cpp` (lignes 32-62).

```cpp
void combus_uart_init() {
    static_assert(ComBusUartBaud <= UartMaxBaud,
                  "ComBusUartBaud exceeds board hardware ceiling UartMaxBaud");

    // P3 — caller-owned per-link handshake context.
    static CombusHandshakeContext s_linkHandshakeCtx = {};

    constexpr ComBusFrameCfg txCfg = {
        kRemoteAnalogCount,    // Analog wire channels only
        kRemoteDigitalCount,   // Digital wire channels only
    };

    // --- Full-duplex: also initialise RX side ---
#if defined(COMBUS_UART)
    static uint16_t s_analog[kRemoteAnalogCount];
    static bool     s_digital[kRemoteDigitalCount];
    constexpr ComBusFrameCfg rxCfg = { kRemoteAnalogCount, kRemoteDigitalCount };

    combus_protocol_init(uart_get_combus_com(), txCfg, ComBusUartTxHz, rxCfg,
                         s_analog, s_digital, &s_linkHandshakeCtx);
#else
    combus_protocol_init(uart_get_combus_com(), txCfg, ComBusUartTxHz,
                         {}, nullptr, nullptr, &s_linkHandshakeCtx);
#endif
}
```

### 3.7. `combus_protocol_init()` — wire TX/RX au transport

**Fichier** : `src/core/system/combus/protocol/combus_protocol.cpp` (lignes 22-46).

```cpp
void combus_protocol_init( NodeCom*               com,
                           ComBusFrameCfg         txCfg,
                           uint32_t               txHz,
                           ComBusFrameCfg         rxCfg,
                           uint16_t*              analogBuf,
                           bool*                  digitalBuf,
                           CombusHandshakeContext* handshakeCtx )
{
    // --- Wire the per-link handshake context BEFORE init ---
    combus_tx_set_handshake_ctx(handshakeCtx);
    combus_rx_set_handshake_ctx(handshakeCtx);

    // --- TX protocol layer ---
#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
    combus_tx_init(com, txCfg, txHz);
#endif

    // --- RX protocol layer ---
#if defined(COMBUS_UART_RX) || defined(COMBUS_UART)
    combus_rx_init(com, rxCfg, analogBuf, digitalBuf);
#endif
}
```

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
  + ((static_cast<uint8_t>(DigitalComBusRemoteID::CH_COUNT) + 7u) / 8u)
  +  (static_cast<uint8_t>(AnalogComBusRemoteID::CH_COUNT)  * 2u)
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

**Fichier** : `src/core/system/combus/protocol/frame/combus_frame.h` (lignes 8-29).

```
Offset  Taille  Champ
------  ------  -----
0       1       SOF          CombusFrameSof (0xAA)
1-5     5       header       CombusFrameHeader fields
6       var     digital[]    bits packed LSB-first, ceil(n_digital/8) bytes
6+d     var     analog[]     uint16_t LE, n_analog entries
last    1       crc8         CRC-8/MAXIM over bytes [0 ... last-1]
```

**Sémantique de `seq`** (lignes 17-25) :
- `1..255` : trames de contrôle (rolling counter, wrap 255 → 1)
- `0` : **RÉSERVÉ** pour trames handshake/versioning (chemin décodage séparé)

**Constantes** (lignes 44-54) :

```cpp
static constexpr uint8_t CombusFrameSof = 0xAAu;
static constexpr uint8_t CombusFrameHeaderLen = sizeof(CombusFrameSof) + sizeof(CombusFrameHeader);
static constexpr uint8_t CombusFrameMinLen = CombusFrameHeaderLen + sizeof(uint8_t);
```

**Header struct** (lignes 72-78) :

```cpp
struct CombusFrameHeader {
    ComBusFrameCfg cfg;   ///< Static layout snapshot: nAnalog, nDigital (wire offsets 0–1).
    uint8_t seq;          ///< Rolling frame counter (1..255 for control frames; 0 is RESERVED).
    uint8_t runLevel;     ///< Combus RunLevel cast to uint8_t.
    uint8_t flags;        ///< COMBUS_FLAG_* bits (transport status only).
};
```

**Flags** (ligne 64) :

```cpp
#define COMBUS_FLAG_FAILSAFE     (1u << 0)  ///< upstream failsafe active
```

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

## 7. TX — encode + send

### 7.1. `combus_tx_init()`

**Fichier** : `src/core/system/combus/protocol/combus_tx.cpp` (lignes 80-108).

```cpp
void combus_tx_init(NodeCom* nodeCom, ComBusFrameCfg frameCfg, uint32_t txHz) {
    if (!nodeCom || txHz == 0u) { return; }

    comBusTx.nodeCom  = nodeCom;
    comBusTx.frameCfg = frameCfg;
    comBusTx.periodMs = 1000u / txHz;

    // P3 — Reset handshake burst state on the linked context.
    if (comBusTx.handshakeCtx) {
        combus_handshake_internal::startBurst(comBusTx.handshakeCtx);
    }
}
```

### 7.2. `combus_tx_update()` — timer-gated, non-blocking

**Fichier** : `src/core/system/combus/protocol/combus_tx.cpp` (lignes 127-184).

```cpp
void combus_tx_update(const ComBus* bus, bool failSafe) {
    if (!comBusTx.nodeCom || comBusTx.periodMs == 0u || !bus) { return; }

    // P3 — drive the boot-time handshake burst on the linked context.
    if (comBusTx.handshakeCtx) {
        combus_handshake_tx_update(comBusTx.handshakeCtx, comBusTx.nodeCom);
    }

    // --- Timer gate ---
    uint32_t now = millis();
    if ((now - comBusTx.lastTxMs) < comBusTx.periodMs) { return; }
    comBusTx.lastTxMs = now;

    // --- Encode ---
    static uint8_t frame[255u];
    uint8_t frameLen = combus_frame_encode(comBusTx.frameCfg, frame, bus,
                                           comBusTx.seq, failSafe);
    if (frameLen == 0u) { return; }

    // --- Send via transport ---
    comBusTx.nodeCom->write(comBusTx.nodeCom->ctx, frame, frameLen);

    // Advance seq counter (1..255, wrap 255 → 1, never 0).
    comBusTx.seq++;
    if (comBusTx.seq == 0u) { comBusTx.seq = 1u; }
}
```

### 7.3. `combus_frame_encode()` — sérialisation

**Fichier** : `src/core/system/combus/protocol/frame/combus_frame.cpp` (lignes 81-154).

```cpp
uint8_t combus_frame_encode(const ComBusFrameCfg& cfg, uint8_t* outputBuffer,
                            const ComBus* combus, uint8_t seq, bool failSafe) {
    const uint8_t nAnalog  = cfg.nAnalog;
    const uint8_t nDigital = cfg.nDigital;

    // 1. Guard conditions
    if (!outputBuffer || !combus) { return 0; }
    if (seq == 0u) { return 0; }   // seq==0 RESERVED for handshake

    uint8_t nDigBytes = (nDigital + 7u) / 8u;

    // 2. Build flags byte
    uint8_t flags = 0;
    if (failSafe) { flags |= COMBUS_FLAG_FAILSAFE; }

    // 3. Write fixed header
    uint8_t pos = 0;
    outputBuffer[pos++] = CombusFrameSof;
    outputBuffer[pos++] = nAnalog;
    outputBuffer[pos++] = nDigital;
    outputBuffer[pos++] = seq;
    outputBuffer[pos++] = (uint8_t)combus->runLevel;
    outputBuffer[pos++] = flags;

    // 4. Pack digital bits (LSB-first)
    for (uint8_t b = 0; b < nDigBytes; ++b) {
        uint8_t packed = 0;
        for (uint8_t bit = 0; bit < 8u; ++bit) {
            uint8_t ch = (uint8_t)(b * 8u + bit);
            if (ch < nDigital && combus->digitalBus && combus->digitalBus[ch].value) {
                packed |= (uint8_t)(1u << bit);
            }
        }
        outputBuffer[pos++] = packed;
    }

    // 5. Write analog values (uint16_t LE)
    for (uint8_t a = 0; a < nAnalog; ++a) {
        uint16_t val = combus->analogBus ? combus->analogBus[a].value : 0;
        outputBuffer[pos++] = (uint8_t)(val & 0xFFu);
        outputBuffer[pos++] = (uint8_t)((val >> 8u) & 0xFFu);
    }

    // 6. Append CRC8
    outputBuffer[pos] = combus_frame_crc8(outputBuffer, pos);
    pos++;

    return pos;
}
```

### 7.4. CRC-8/MAXIM

**Fichier** : `src/core/system/combus/protocol/frame/combus_frame.cpp` (lignes 33-47).

```cpp
uint8_t combus_frame_crc8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; ++i) {
        uint8_t byte = data[i];
        for (uint8_t b = 0; b < 8u; ++b) {
            uint8_t mix = (crc ^ byte) & 0x01u;
            crc >>= 1u;
            if (mix) { crc ^= 0x8Cu; }
            byte >>= 1u;
        }
    }
    return crc;
}
```

---

## 8. RX — drain + decode + apply

### 8.1. `combus_rx_init()`

**Fichier** : `src/core/system/combus/protocol/combus_rx.cpp` (lignes 240-275).

```cpp
void combus_rx_init(NodeCom* nodeCom, ComBusFrameCfg frameCfg,
                    uint16_t* analogBuf, bool* digitalBuf) {
    if (!nodeCom || !analogBuf || !digitalBuf) { return; }

    comBusRx.nodeCom  = nodeCom;
    comBusRx.frameCfg = frameCfg;

    // Wire caller-provided buffers into the snapshot struct
    comBusRx.snap.analog  = analogBuf;
    comBusRx.snap.digital = digitalBuf;

    // Reset ring buffer and link state
    comBusRx.rxHead       = 0u;
    comBusRx.rxCount      = 0u;
    comBusRx.snapValid    = false;
    comBusRx.everReceived = false;

    combus_handshake_internal::clearContractValidated(comBusRx.handshakeCtx);
}
```

### 8.2. `combus_rx_update()` — drain + decode

**Fichier** : `src/core/system/combus/protocol/combus_rx.cpp` (lignes 289-305).

```cpp
void combus_rx_update() {
    if (!comBusRx.nodeCom) { return; }

    // --- Drain available bytes into ring buffer ---
    while (comBusRx.nodeCom->available(comBusRx.nodeCom->ctx) > 0) {
        int b = comBusRx.nodeCom->readByte(comBusRx.nodeCom->ctx);
        if (b >= 0) { rxBufPush((uint8_t)b); }
    }

    // --- Try to decode (may process multiple back-to-back frames) ---
    while (comBusRx.rxCount >= CombusFrameMinLen) {
        if (tryDecode() == 0u) { break; }
    }
}
```

### 8.3. `tryDecode()` — discrimination handshake / control

**Fichier** : `src/core/system/combus/protocol/combus_rx.cpp` (lignes 150-221).

```cpp
static uint8_t tryDecode() {
    // --- 1. Scan for SOF ---
    while (comBusRx.rxCount > 0u && rxBufAt(0u) != CombusFrameSof) {
        rxBufConsume(1u);
    }
    if (comBusRx.rxCount < CombusFrameHeaderLen) { return 0u; }

    // --- 2. Peek `seq` byte (wire offset 3) to discriminate frame kind ---
    uint8_t seqByte = rxBufAt(1u + offsetof(CombusFrameHeader, cfg) +
                              offsetof(ComBusFrameCfg, nDigital) + 1u);

    if (seqByte == 0u) {
        // --- 2a. HANDSHAKE PATH — structurally separate from control ---
        return combus_handshake_tryDecode(comBusRx.handshakeCtx,
                                          rxBuf, rxBufSize,
                                          comBusRx.rxHead, comBusRx.rxCount);
    }

    // --- 3. CONTROL-FRAME PATH ---
    if (comBusRx.rxCount < CombusFrameMinLen) { return 0u; }

    uint8_t nAnalog   = rxBufAt(1u + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nAnalog));
    uint8_t nDigital  = rxBufAt(1u + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nDigital));
    uint8_t nDigBytes = (nDigital + 7u) / 8u;

    uint16_t expectedLenW = CombusFrameHeaderLen + (uint16_t)nDigBytes
                          + (uint16_t)nAnalog * 2u + 1u;
    if (expectedLenW > frameMaxLen) { rxBufConsume(1u); return 0u; }
    uint8_t expectedLen = (uint8_t)expectedLenW;
    if (comBusRx.rxCount < expectedLen) { return 0u; }

    // --- 4. Copy frame into a linear buffer and decode ---
    uint8_t linear[frameMaxLen];
    for (uint8_t i = 0u; i < expectedLen; ++i) { linear[i] = rxBufAt(i); }

    if (combus_frame_decode(comBusRx.frameCfg, &comBusRx.snap, linear, expectedLen)) {
        comBusRx.snapValid    = true;
        comBusRx.lastRxMs     = millis();
        comBusRx.everReceived = true;
        rxBufConsume(expectedLen);
        return expectedLen;
    } else {
        rxBufConsume(1u);  // CRC mismatch — discard SOF and re-sync
        return 0u;
    }
}
```

### 8.4. `combus_frame_decode()` — validation + unpack

**Fichier** : `src/core/system/combus/protocol/frame/combus_frame.cpp` (lignes 191-257).

```cpp
bool combus_frame_decode(const ComBusFrameCfg& cfg, ComBusFrame* outputFrame,
                         const uint8_t* inputBuffer, uint8_t len) {
    const uint8_t analogBufSize  = cfg.nAnalog;
    const uint8_t digitalBufSize = cfg.nDigital;

    // 1. Minimum length and SOF guard check
    if (!outputFrame || !inputBuffer || len < CombusFrameMinLen) { return false; }
    if (inputBuffer[0] != CombusFrameSof) { return false; }

    // 2. Parse header fields
    CombusFrameHeader header;
    memcpy(&header, inputBuffer + 1u, sizeof(header));

    uint8_t nDigBytes = (header.cfg.nDigital + 7u) / 8u;

    // 3. Sanity-check declared sizes
    if (!outputFrame->analog || !outputFrame->digital) { return false; }
    uint16_t expectedLenW = CombusFrameHeaderLen + (uint16_t)nDigBytes
                          + (uint16_t)header.cfg.nAnalog * 2u + 1u;
    if (expectedLenW > 255u) { return false; }
    if (header.cfg.nAnalog > analogBufSize) { return false; }

    uint8_t expectedLen = (uint8_t)expectedLenW;
    if (len < expectedLen) { return false; }

    // 4. Validate CRC
    uint8_t crcExpected = inputBuffer[expectedLen - 1u];
    uint8_t crcActual   = combus_frame_crc8(inputBuffer, (uint8_t)(expectedLen - 1u));
    if (crcActual != crcExpected) { return false; }

    // 5. Unpack digital bits
    uint8_t nDigital = header.cfg.nDigital;
    if (nDigital > digitalBufSize) { nDigital = digitalBufSize; }

    uint8_t pos = CombusFrameHeaderLen;
    for (uint8_t b = 0; b < nDigBytes; ++b) {
        uint8_t packed = inputBuffer[pos++];
        for (uint8_t bit = 0; bit < 8u; ++bit) {
            uint8_t ch = (uint8_t)(b * 8u + bit);
            if (ch < digitalBufSize) {
                outputFrame->digital[ch] = (packed >> bit) & 0x01u;
            }
        }
    }

    // 6. Unpack analog values (uint16_t LE)
    for (uint8_t a = 0; a < header.cfg.nAnalog; ++a) {
        uint16_t lo  = inputBuffer[pos++];
        uint16_t hi  = inputBuffer[pos++];
        outputFrame->analog[a] = (uint16_t)(lo | (hi << 8u));
    }

    // 7. Populate frame header
    outputFrame->header              = header;
    outputFrame->header.cfg.nDigital = nDigital;

    return true;
}
```

### 8.5. `combus_frame_apply()` — écrit dans le ComBus live

**Fichier** : `src/core/system/combus/protocol/frame/combus_frame.cpp` (lignes 288-329).

```cpp
void combus_frame_apply(const ComBusFrameCfg& cfg, ComBus* combus,
                        const ComBusFrame* inputFrame, ChanLayer caller) {
    const uint8_t nAnalog  = cfg.nAnalog;
    const uint8_t nDigital = cfg.nDigital;

    if (!combus || !inputFrame) { return; }

    // 2. RunLevel + watchdog timestamp
    combus_set_runlevel(*combus, (RunLevel)inputFrame->header.runLevel, caller);
    combus->lastFrameMs = millis();

    // 4. Analog channels
    uint8_t nAnalogEff = (inputFrame->header.cfg.nAnalog < nAnalog)
                       ? inputFrame->header.cfg.nAnalog : nAnalog;
    if (combus->analogBus) {
        for (uint8_t i = 0; i < nAnalogEff; ++i) {
            combus_set_analog(*combus, (AnalogComBusID)i, inputFrame->analog[i], caller);
        }
    }

    // 5. Digital channels
    uint8_t nDigitalEff = (inputFrame->header.cfg.nDigital < nDigital)
                        ? inputFrame->header.cfg.nDigital : nDigital;
    if (combus->digitalBus) {
        for (uint8_t i = 0; i < nDigitalEff; ++i) {
            combus_set_digital(*combus, (DigitalComBusID)i, inputFrame->digital[i], caller);
        }
    }

    // 6. Mark bus as actively driven by this frame
    combus->isDrived = true;
}
```

---

## 9. Handshake P3 — contexte par lien

Chaque lien ComBus indépendant possède son propre `CombusHandshakeContext` :

**Fichier** : `src/machines/init/com/combus_uart_init.cpp` (lignes 41-42).

```cpp
// P3 — caller-owned per-link handshake context.  Zero-initialised
//      at boot, shared between TX and RX of the same ComBus link
//      by combus_protocol_init().
static CombusHandshakeContext s_linkHandshakeCtx = {};
```

Le contexte est partagé entre TX et RX du même lien via :
- `combus_tx_set_handshake_ctx(ctx)` — appelé avant `combus_tx_init()`
- `combus_rx_set_handshake_ctx(ctx)` — appelé avant `combus_rx_init()`

**Fichiers handshake** :
- `src/core/system/combus/protocol/frame/combus_handshake.h`
- `src/core/system/combus/protocol/frame/combus_handshake.cpp`
- `src/core/system/combus/protocol/frame/combus_handshake_tx.h/.cpp`
- `src/core/system/combus/protocol/frame/combus_handshake_rx.h/.cpp`

---

## 10. Loop — flux d'exécution

**Fichier** : `src/core/system/output/output_manager.cpp` (lignes 20-27).

```cpp
void output_update(const ComBus& bus, bool failsafeActive) {
    // --- ComBus UART TX (50 Hz timer-gated, non-blocking) ---
#if defined(COMBUS_UART_TX) || defined(COMBUS_UART)
    combus_tx_update(&bus, failsafeActive);
#endif
}
```

Le RX est appelé séparément (typiquement depuis le module sound ou un autre
consumer) via `combus_rx_update()`.

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
| **Wrapper env sound**        | `src/sound_module/init/com/combus_uart_init.cpp`                     |
| **Protocole orchestrateur**  | `src/core/system/combus/protocol/combus_protocol.cpp`                |
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
