# ComBus Ramp Runtime — Calculation Guide

## Context

We are tuning inertia simulation parameters for RC model vehicle actuators (steering, dump body, traction). Each actuator uses a **progressive ramp** processor that smoothly transitions from one position to another.

## ComBus Domain

All positions are expressed in the **ComBus domain**:
- **Type:** `combus_t` (typedef for `uint16_t`)
- **Range:** 0 to 65535 (16-bit unsigned)
- **Neutral:** 32767 (midpoint, `CbusNeutral = CbusMaxVal >> 1`)
- **Bipolar actuators** (steering, traction): neutral = 32767, forward = 65535, reverse = 0
- **Unipolar actuators** (dump body): min = 0, max = 65535

## Ramp Configuration Structure

```cpp
struct CbRampCfg {
    uint16_t rampTimeMs;      // Update cycle period (milliseconds)
    uint16_t accelSteps;      // Position increment per cycle (acceleration)
    uint16_t accelDownSteps;  // Position increment per cycle (deceleration, optional)
    uint16_t brakeSteps;      // Position increment per cycle (braking)
    uint16_t neutralBand;     // Deadband around neutral (no movement)
};
```

**Key concept:** The ramp processor runs every `rampTimeMs` milliseconds and increments/decrements the current position by `accelSteps` (or `brakeSteps`) until it reaches the target.

## Runtime Calculation

**Runtime** = Total time to travel full range at maximum acceleration.

### Formula

For a **unipolar actuator** (0 → 65535):
```
cycles_needed = ceil(CbusMaxVal / accelSteps)
runtime_ms    = cycles_needed × rampTimeMs
runtime_sec   = runtime_ms / 1000
```

For a **bipolar actuator** (neutral → full forward/reverse):
```
cycles_needed = ceil(CbusNeutral / accelSteps)
runtime_ms    = cycles_needed × rampTimeMs
runtime_sec   = runtime_ms / 1000
```

### Helper Function

`pctToCbus(pct)` converts a percentage of half-range to ComBus units:
```cpp
constexpr uint16_t pctToCbus(uint8_t pct) {
    return static_cast<uint16_t>((CbusNeutral * pct) / 100u);
}
```

**Example:** `pctToCbus(5)` = (32767 × 5) / 100 = 1638 ComBus units (~5% of half-range)

## Current Configuration Examples

### 1. Steering — Asymmetric (fast start, instant stop)

```cpp
static constexpr CbRampCfg kSteerAsymRamp {
    .rampTimeMs  = 20u,              // 20 ms cycle
    .accelSteps  = pctToCbus(5),     // 1638 units/cycle (~5% per 20ms)
    .brakeSteps  = CbusNeutral,      // 32767 units/cycle (instant stop)
    .neutralBand = 0u,
};
```

**Runtime calculation (neutral → full lock):**
- `accelSteps = 1638`
- `cycles = ceil(32767 / 1638) = 20 cycles`
- `runtime = 20 × 20 ms = 400 ms = 0.4 sec`

**Physical meaning:** Steering servo takes ~0.4 seconds to go from center to full lock.

### 2. Dump Body — Asymmetric (slow raise, fast lower, instant stop)

```cpp
static constexpr CbRampCfg kDumpAsymRamp {
    .rampTimeMs     = 20u,              // 20 ms cycle
    .accelSteps     = pctToCbus(2),     // 655 units/cycle (~2% per 20ms)
    .accelDownSteps = pctToCbus(4),     // 1310 units/cycle (~4% per 20ms)
    .brakeSteps     = CbusNeutral,      // 32767 units/cycle (instant stop)
    .neutralBand    = 0u,
};
```

**Runtime calculation (0 → full up):**
- `accelSteps = 655`
- `cycles = ceil(65535 / 655) = 101 cycles`
- `runtime = 101 × 20 ms = 2020 ms ≈ 2.0 sec`

**Runtime calculation (full up → 0, lowering):**
- `accelDownSteps = 1310`
- `cycles = ceil(65535 / 1310) = 51 cycles`
- `runtime = 51 × 20 ms = 1020 ms ≈ 1.0 sec`

**Physical meaning:** Dump body takes ~2 seconds to raise fully, ~1 second to lower.

### 3. Traction — Heavy truck inertia

```cpp
static constexpr CbRampCfg kTractionRamp {
    .rampTimeMs  = 50u,              // 50 ms cycle
    .accelSteps  = pctToCbus(3),     // 983 units/cycle (~3% per 50ms)
    .brakeSteps  = pctToCbus(6),     // 1966 units/cycle (~6% per 50ms)
    .neutralBand = 0u,
};
```

**Runtime calculation (neutral → full speed forward):**
- `accelSteps = 983`
- `cycles = ceil(32767 / 983) = 34 cycles`
- `runtime = 34 × 50 ms = 1700 ms = 1.7 sec`

**Runtime calculation (full speed → stop, braking):**
- `brakeSteps = 1966`
- `cycles = ceil(32767 / 1966) = 17 cycles`
- `runtime = 17 × 50 ms = 850 ms = 0.85 sec`

**Physical meaning:** ESC takes ~1.7 seconds to reach full speed, ~0.85 seconds to stop.

## What We Need

We are tuning a **1:14 scale Volvo A60H articulated hauler** (dumper truck). The model has:
- **Traction:** Heavy inertia (large vehicle, loaded)
- **Steering:** Articulated joint (not car-like steering rack)
- **Dump body:** Hydraulic actuator (slow raise, gravity-assist lower)

### Question for the AI

Given the following constraints:
- **Physical realism:** Runtimes should match 1:14 scale heavy equipment behavior
- **Servo safety:** No instant movements (except intentional emergency stop)
- **User experience:** Responsive but not twitchy

**Please propose:**
1. **Traction ramp** (`rampTimeMs`, `accelSteps`, `brakeSteps`) — loaded hauler acceleration/braking
2. **Steering ramp** (`rampTimeMs`, `accelSteps`) — articulated steering response
3. **Dump ramp** (`rampTimeMs`, `accelSteps`, `accelDownSteps`) — hydraulic dump body

**Output format:**
```cpp
// Traction
.rampTimeMs  = <value>u,
.accelSteps  = pctToCbus(<pct>),  // runtime: <X.X> sec (0 → full)
.brakeSteps  = pctToCbus(<pct>),  // runtime: <X.X> sec (full → stop)

// Steering
.rampTimeMs  = <value>u,
.accelSteps  = pctToCbus(<pct>),  // runtime: <X.X> sec (center → lock)

// Dump
.rampTimeMs     = <value>u,
.accelSteps     = pctToCbus(<pct>),      // runtime: <X.X> sec (lowered → raised)
.accelDownSteps = pctToCbus(<pct>),      // runtime: <X.X> sec (raised → lowered)
```

**Reference values (current config, may need adjustment):**
- Traction: 1.7 sec accel, 0.85 sec brake
- Steering: 0.4 sec center → lock
- Dump: 2.0 sec raise, 1.0 sec lower

**Please suggest improved values based on realistic 1:14 scale heavy equipment dynamics.**

---

## Notes for Implementation

- All `pctToCbus()` calls are compile-time `constexpr` — zero runtime cost.
- `brakeSteps = CbusNeutral` (32767) means "instant stop" — one cycle to zero.
- `neutralBand` creates a deadzone (e.g., ±1% around neutral = no movement) — currently unused (0).
- Asymmetric ramps (`accelSteps` ≠ `brakeSteps`) allow different rise/fall rates.
- Per-gear dynamic ramp times are handled separately (see `gear_dyn_ramp_fn` in code).

## Constants Reference

```cpp
CbusMaxVal   = 65535  // Full range (uint16_t)
CbusNeutral  = 32767  // Midpoint (bipolar neutral)
CbusMinVal   = 0      // Zero
```
