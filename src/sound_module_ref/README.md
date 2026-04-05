# Sound Module — Open Node RC Core

Standalone sub-project for a **dedicated Sound ESP32** node.  
The sound engine (RC Engine Sound ESP32) runs on its own microcontroller and
receives channel data from the Machine ESP32 over a simple UART binary link.
The machine codebase is completely unaware of audio — it just publishes its
ComBus snapshot on a serial port.

---

## 1. Architecture overview

```
┌─────────────────────────────┐      UART 115200         ┌─────────────────────────────┐
│      Machine ESP32          │  ─────────────────────►  │       Sound ESP32           │
│  (volvo_A60H_bruder env)    │   13 bytes @ 50 Hz       │  (sound_node_volvo env)     │
│                             │                          │                             │
│  ComBus (analog + digital)  │                          │  sound_uart_rx_update()     │
│  sound_uart_tx_update()     │                          │  → ComBusFrame              │
│  └─ combus_frame_encode │                          │  sound_hal_update()         │
│                             │                          │  └─ pulseWidth[] globals    │
│                             │                          │  rc_engine_sound engine     │
└─────────────────────────────┘                          └─────────────────────────────┘
```

Key design decisions:
- **Machine code is not modified** — the UART TX is a non-breaking optional hook
  guarded by `#ifdef SOUND_NODE_UART` in `machines/main.cpp`.
- **Sound engine is not modified** — the HAL adapter bridges ComBus snapshots to
  the existing `pulseWidth[]` globals that rc_engine_sound already reads.
- **No RC hardware on the sound node** — no SBUS, no PWM input decoder; the UART
  link replaces all RC hardware abstraction.
- **Generic transport** — `src/core/combus/combus_frame.h` is
  platform-independent. The same framing can be reused for an ESP-Now RF link.

---

## 2. Wiring

| Signal      | Machine ESP32 pin | Sound ESP32 pin |
|-------------|:-----------------:|:---------------:|
| UART2 TX    | GPIO 17           | GPIO 16 (RX)    |
| GND         | GND               | GND             |

One unidirectional wire + shared GND.  
No RX line is required on the machine side in the base configuration.

Constants defined in `platformio.ini` (overridable):
```
SOUND_TX_PIN   17    (machine side, defined in build_flags)
SOUND_RX_PIN   -1    (machine side, unused)
SOUND_RX_PIN   16    (sound side,  defined in main.cpp)
```

---

## 3. Binary frame format

Produced by `combus_frame_encode()`, consumed by `combus_frame_decode()`.

```
Offset  Size   Field
──────  ─────  ──────────────────────────────────────────────────────────
  0      1     SOF = 0xA5
  1      1     n_analog   (uint8_t — number of analog channels)
  2      1     n_digital  (uint8_t — number of digital channels)
  3      1     digital[0..7] packed as 8 bits (1 byte per 8 channels)
  4+     2×n   analog[i] as uint16_t little-endian (0 = min, 65535 = max,
               32767 = center / neutral)
  last   1     CRC-8/MAXIM (poly 0x31, init 0x00, no final XOR)
```

For `DUMPER_TRUCK` (3 analog + 3 digital): **13 bytes** per frame.  
At 50 Hz → ~520 bytes/s on a 115200 baud link (< 1 % utilisation).

---

## 4. ComBus → rc_engine_sound channel mapping

| ComBus channel              | rc_engine_sound index | Role          |
|-----------------------------|-----------------------|---------------|
| `ENGINE_RPM_BUS` (analog)   | `pulseWidth[3]`       | Throttle      |
| `STEERING_BUS`    (analog)  | `pulseWidth[1]`       | Steering      |
| `DUMP_BUS`        (analog)  | `pulseWidth[6]`       | Gearbox / 2nd |
| `HORN`    (digital, bit 0)  | `pulseWidth[5]`       | Horn toggle   |
| `LIGHTS`  (digital, bit 1)  | `pulseWidth[2]`       | Function / R  |

All pulse widths are mapped linearly: ComBus 0→1000 µs, 32767→1500 µs, 65535→2000 µs.  
Digital channels are mapped to 1000 µs (OFF) or 2000 µs (ON).

Mapping constants are centralised in `config/sound_config.h`.

---

## 5. Directory structure

```
src/sound_module/
├── README.md             ← this file
├── main.cpp              ← Sound ESP32 entry point (setup + loop scaffold)
├── config/
│   └── sound_config.h    ← ComBus ↔ rc_engine_sound channel mapping + constants
├── transport/
│   ├── sound_uart_rx.h   ← Public API: init, update, latest snapshot, link alive
│   └── sound_uart_rx.cpp ← Ring-buffer UART receiver, SOF sync, CRC validation
├── hal/
│   ├── sound_hal.h       ← Public API: init, update
│   └── sound_hal.cpp     ← Snapshot → pulseWidth[] mapping + failsafe
└── engine/
    └── .gitkeep          ← Placeholder — rc_engine_sound sources go here
```

Companion file in core (machine-independent, reusable):
```
src/core/transport/
├── combus_frame.h    ← encode / decode / apply / CRC-8 — no Arduino deps
└── combus_frame.cpp
```

Machine-side hook (non-breaking, conditional):
```
src/machines/system/
├── sound_uart_tx.h       ← Public API: init, update (50 Hz timer-gated)
└── sound_uart_tx.cpp
```

---

## 6. Build environments

Defined in `platformio.ini`:

| Environment         | Purpose                                   | Key build flags                       |
|---------------------|-------------------------------------------|---------------------------------------|
| `sound_node_base`   | Base sound env (no profile-specific libs) | `SOUND_NODE` · `COMBUS_UARTx_RX`     |
| `sound_node_volvo`  | Volvo A60H profile + full audio libs      | `+ MACHINE_TYPE=DUMPER_TRUCK`         |

Source filter (both envs):
```ini
build_src_filter = -<*> +<core/transport/**> +<sound_module/**>
```
`sound_module/` is a direct subfolder of `src/`, so no path prefix is needed.

To enable machine-side TX in the machine build, add `SOUND_NODE_UART` and define
`SOUND_TX_PIN` / `SOUND_RX_PIN` in the machine env's `build_flags`.

---

## 7. Failsafe behaviour

| Condition                                          | Result in sound hal          |
|----------------------------------------------------|------------------------------|
| UART link silent > 500 ms (`COMBUS_LINK_TIMEOUT`)  | All channels → neutral 1500µs, `failSafe = true` in engine |
| `COMBUS_FLAG_FAILSAFE` bit set in ComBus flags     | Same as above                |
| `RunLevel != RUNNING` on machine side              | keyOn = false → engine off   |

---

## 8. Step-by-step engine integration

The scaffold compiles and links as-is (`SOUND_ENGINE_READY` not defined).  
To activate the real rc_engine_sound engine:

### Step 1 — Copy engine sources

Copy these files from `Rc_Engine_Sound_ESP32/src/` into `src/sound_module/engine/`:

```
src.ino  →  engine/sound_engine.cpp  (rename, do NOT compile as .ino)
src/lib/sbus.h + sbus.cpp
src/lib/SUMD.h + SUMD.cpp
0_generalSettings.h  1_Vehicle.h  2_Remote.h  3_ESC.h  4_Transmission.h
5_Shaker.h  6_Lights.h  7_Sound.h  8_Dashboard.h  9_Dashboard.h  10_Trailer.h
vehicles/00_Master.h  vehicles/1000HpScaniaV8.h  (or the target vehicle)
```

### Step 2 — Strip RC input sections from sound_engine.cpp

In the original `src.ino`, all RC input reading blocks are in clearly delimited
`#ifdef` sections.  Comment out or remove:

- `readSignals()` body — the hardware-specific SBUS/SUMD/PWM read block
- Any call to `sbus.read()`, `sumd.read()`, or `pulseIn()` for PWM
- The SBUS/SUMD initialization in `setup()`

The `pulseWidth[]` array itself must **survive** — it is read by the engine.  
The HAL (`sound_hal.cpp`) is the new sole writer of `pulseWidth[]`.

### Step 3 — Expose globals

Add to the top of `sound_engine.cpp` (or a new `engine_globals.h`):

```cpp
extern volatile uint16_t pulseWidth[];   // HAL writes, engine reads
extern bool              failSafe;
extern bool              keyOn;
```

Or declare them with `volatile` linkage and include `engine_globals.h` from
both `sound_hal.cpp` and `sound_engine.cpp`.

### Step 4 — Activate the flag

In `platformio.ini`, under `[env:sound_node_volvo]`, add:

```ini
-D SOUND_ENGINE_READY
```

This unblocks the `#ifdef SOUND_ENGINE_READY` guards in:
- `src/sound_module/hal/sound_hal.cpp` (real extern declarations)
- `src/sound_module/main.cpp` (engine `setup()` and `loop()` calls)

### Step 5 — Select vehicle profile

Edit `engine/sound_engine.cpp` (or the vehicle header include) to include the
correct vehicle: `vehicles/Volvo_A60H.h` (create if not present) derived from
`vehicles/00_Master.h`.

### Step 6 — Build and validate

```bash
pio run -e sound_node_volvo
```

Expected: clean link. The engine will start with `pulseWidth[]` fed from the
UART link instead of hardware RC inputs.

---

## 9. Design conclusions

### What works well
- **Zero coupling**: machine code and sound engine never include each other.
  The binary frame on UART is the only interface.
- **Incremental build**: the scaffold compiles without the engine sources,
  allowing CI and code review before the audio library is integrated.
- **Generic framing**: `combus_frame` encodes any ComBus snapshot regardless
  of the machine type. Adding a new channel only changes `sound_config.h` mapping.
- **Failsafe is clean**: a single 500 ms link-alive check drives all safety
  channels; no partial state is possible.

### Limitations and next steps
- **One-way link only**: the sound node cannot send acknowledgement or status
  back to the machine. For diagnostics, use the dashboard on the machine side
  and USB serial on the sound node.
- **No re-sync on cold boot**: if the sound node boots after the machine, it
  will sit in failsafe until the first valid frame arrives (< 20 ms at 50 Hz —
  negligible in practice).
- **Engine sources not included**: `src/sound_module/engine/` is empty and under
  `.gitkeep`. The Rc_Engine_Sound_ESP32 codebase carries its own licence; it is
  not bundled here. Follow §8 above to integrate locally.
- **IDF 4.4 lock**: the project is currently pinned to `espressif32@6.7.0`
  (IDF 4.4) because the PS4 BT library is incompatible with IDF 5.x. This
  constraint applies equally to the sound node env as it inherits `[env]` base.
  Re-evaluate when an IDF-5-compatible PS4 lib is available.
