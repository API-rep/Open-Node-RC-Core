# Runtime Engine Coding Conventions

This document defines the coding conventions used when implementing Runtime Engines in Open RC Node.

It focuses on the engine definition itself: structure, methods, and interaction with configured subsystem data.

Subsystem data patterns (`cfg`, `state`, `dynCfg`) are described in `include/struct/README.md`.

The conceptual foundation of Runtime Engines is described in the Runtime Engines concept document.

---

## 1. Engines Are Execution Modules

Runtime Engines are lightweight execution modules that operate on externally configured subsystem data.

They do not own the data they manipulate.

They receive pointers to structures defined and instantiated in configuration files, then apply behaviour to them.

Engine behaviour is implemented as methods attached to the engine definition itself.

```cpp
struct RampEngine {
    const Ramp* ramp;   // Externally configured subsystem data

    bool init(const Ramp* r);
    void update();
};
```

This keeps execution flow explicit and traceable without introducing ownership ambiguities, virtual dispatch, or object hierarchies.

The call site shows exactly which engine operates on which data:

```cpp
RampEngine engine;

engine.init(&ramp);
engine.update();
```

The engine does not own subsystem data. It only stores references to externally configured structures and its own execution bookkeeping.

---

## 2. Persistent Execution

Persistent engines maintain private execution bookkeeping between calls.

They are instantiated once, initialized with subsystem data, then reused throughout the subsystem lifetime.

```cpp
struct RampEngine {
    const Ramp* ramp;

    // Internal bookkeeping
    uint32_t lastUpdateUs;
    int16_t accumulator;
    bool initialized;

    bool init(const Ramp* r);
    void update();
};
```

```cpp
bool RampEngine::init(const Ramp* r)
{
    if (!r || !r->cfg || !r->state) {
        return false;
    }

    ramp = r;

    lastUpdateUs = 0;
    accumulator = 0;
    initialized = true;

    return true;
}

void RampEngine::update()
{
    if (!ramp || !ramp->state || !initialized) {
        return;
    }

    const RampCfg* eff =
        ramp->dynCfg && ramp->dynActive
            ? ramp->dynCfg
            : ramp->cfg;

    uint32_t nowUs = micros();
    uint32_t dtUs = nowUs - lastUpdateUs;
    lastUpdateUs = nowUs;

    // Apply behaviour using eff and ramp->state
    // Update accumulator and ramp->state as needed
}
```

The engine stores references to the subsystem and maintains its own execution context.

Execution methods resolve dynamic configuration and update shared state through these stored references.

The engine must not store local copies of `cfg` or `dynCfg`.

The effective configuration should be resolved whenever execution occurs.

---

## 3. Transient Execution

Transient engines do not maintain persistent execution data.

They behave as specialized execution routines operating directly on configured subsystem data passed as parameters.

```cpp
struct RampEngine {
    static void update(const Ramp* ramp);
};
```

```cpp
void RampEngine::update(const Ramp* ramp)
{
    if (!ramp || !ramp->cfg || !ramp->state) {
        return;
    }

    const RampCfg* eff =
        ramp->dynCfg && ramp->dynActive
            ? ramp->dynCfg
            : ramp->cfg;

    // Apply behaviour using eff and ramp->state
}
```

Transient engines are suitable when no private runtime context is required.

They are well suited to localized behaviours that do not require persistent execution context.

---

## 4. Internal Data Belongs to the Engine

Timers, counters, flags, cached values, and other execution bookkeeping belong to the engine itself.

They are not part of the configured subsystem data.

This data is invisible outside the engine implementation.

If external systems require access to a value, that value should instead become part of the subsystem's shared structures.

Transient engines naturally do not carry internal execution data.

---

## 5. Engines Remain Local

An engine belongs to the subsystem it serves.

Prefer:

```text
core/system/ramp/
    ramp_engine.h
    ramp_engine.cpp
```

This co-location reduces fragmentation, keeps the scope obvious for both human readers and AI agents, and makes ownership immediately visible.

Avoid exposing engine structures as shared project definitions unless the engine itself represents a genuinely reusable concept.

```text
include/struct/ramp_engine.h    // Discouraged unless broadly shared
```

The engine's local contract — what it expects from its environment and what it produces — is defined alongside the subsystem implementation.

---

## 6. Using a Runtime Engine

A Runtime Engine is instantiated and driven by a coordinating layer.

This may be a subsystem umbrella, a scheduler, or the main loop.

### Persistent execution

```cpp
// --- Configuration (defined in config files) ---
extern const Ramp ramp;

// --- Engine instance ---
RampEngine rampEngine;

// --- Initialization ---
void subsystem_init()
{
    rampEngine.init(&ramp);
}

// --- Execution ---
void subsystem_update()
{
    rampEngine.update();
}
```

### Transient execution

```cpp
// --- Configuration (defined in config files) ---
extern const Ramp ramp;

// --- Execution ---
void subsystem_update()
{
    RampEngine::update(&ramp);
}
```

The coordinator does not access the engine's internal bookkeeping.

It treats the Runtime Engine as an opaque execution unit.
