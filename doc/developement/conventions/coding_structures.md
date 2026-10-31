# Basic Structure Conventions

This document defines the coding conventions used when implementing structures in Open RC Node.

It focuses on the structure definitions themselves: field organization, lightweight helpers, and readability considerations.

Subsystem decomposition patterns (`cfg`, `state`, `dynCfg`) are described in `include/struct/README.md`. The patterns remain valid; the directory itself is deprecated as a location (see §5).

---

## 1. Structures Describe Data

Structures primarily exist to organize related pieces of information.

They should focus on describing data rather than implementing behaviour.

```cpp
struct Position {
    int16_t x;
    int16_t y;
};
```

For subsystem structures, keep each responsibility explicit.

Prefer:

```cpp
struct MotorCfg {
    uint16_t minSpeed;
    uint16_t maxSpeed;
};

struct MotorState {
    uint16_t currentSpeed;
};

struct Motor {
    const char*       name;
    uint8_t           id;

    const MotorCfg*   cfg;
    MotorState*       state;
};
```

Avoid turning structures into miniature modules containing substantial processing logic.

Execution behaviour belongs to Runtime Engines.

Refer to `include/struct/README.md` for subsystem-level design patterns.

---

## 2. Lightweight Helpers Are Acceptable

Small helper functions deriving information directly from stored fields are acceptable.

Helpers should:

* remain focused on exposing information derived from the structure;
* avoid modifying the structure state;
* remain reasonably short and self-contained;
* be declared at the end of the structure.

### Single-expression helpers

When the implementation is trivial, it may remain directly inside the structure.

Example:

```cpp
struct Range {
    uint16_t min;
    uint16_t max;

    constexpr bool contains(uint16_t value) const noexcept {
        return value >= min && value <= max;
    }
};
```

### Multi-line helpers

When a helper spans more than a single expression, keep the structure declaration concise and move the implementation immediately below it.

Prefer:

```cpp
struct Range {
    uint16_t min;
    uint16_t max;

    constexpr bool intersects(const Range& other) const noexcept;
};

// Helper implementation
constexpr bool Range::intersects(const Range& other) const noexcept {
    return (min <= other.max) && (other.min <= max);
}
```

This preserves the readability of the structure while keeping type-specific helpers close to the type they belong to.

Helpers involving substantial processing, external interactions, ownership management, execution sequencing, or subsystem behaviour should instead be implemented outside of the structure entirely.

Those responsibilities belong to Runtime Engines or standalone functions.

---

## 3. Dynamic Configuration Overrides

Some configurations may support runtime overrides through an optional `dynCfg`.

When used, `dynCfg` should:

* reuse the same structure type as `cfg`;
* reside in RAM;
* preserve the original Flash-resident configuration;
* only be introduced when runtime adaptation is genuinely required.

Small configurations may be duplicated entirely:

```cpp
// Flash
const RampCfg rampCfg = {
    .rate   = 100,
    .target = 2000,
};

// RAM
RampCfg rampDynCfg = rampCfg;
bool rampDynActive = false;
```

Effective configuration is then resolved as:

```cpp
const RampCfg* eff = rampDynActive
                   ? &rampDynCfg
                   : &rampCfg;
```

For larger configurations, prefer separating immutable and mutable parameters into distinct structures so that only the truly dynamic subset consumes RAM.

Refer to `include/struct/README.md` for the rationale and memory considerations behind this pattern.

---

## 4. Group Repeated Fields Into Sub-Structures

When a structure grows, look for the axes that repeat: the same set of fields per kind, per direction or per role, or a block of fields that serves a single concern. Name the axis once with a sub-structure, then use it once per instance.

Signals:

* the same prefix or suffix on several fields (`nAnalog`, `analog`, `analogRx` and `nDigital`, `digital`, `digitalRx`; `md5`, `md5Stable`, `md5Peer`, `md5PeerStable`);
* a block of fields that the same functions read and write together (the watch of a link, its coupling);
* fields that only one feature uses: grouped, they become a single member, easy to find and to guard by a flag.

Avoid:

```cpp
struct ComPeerLink {
    const char*    coupler;
    const uint8_t* md5;
    const uint8_t* md5Stable;
    const uint8_t* md5Peer;
    const uint8_t* md5PeerStable;
    uint8_t        nAnalog;
    uint8_t        nDigital;
    uint8_t        nStableAnalog;
    uint8_t        nStableDigital;
    const uint8_t* analog;
    const uint8_t* digital;
    const bool*    analogRx;
    const bool*    digitalRx;
};
```

Prefer:

```cpp
struct ComLinkMd5 {
    const uint8_t* stable;
    const uint8_t* next;
};

struct ComLinkTable {
    uint8_t        count;
    uint8_t        stableCount;
    const uint8_t* ids;
    const bool*    rx;
};

struct ComPeerLink {
    const char*  coupler;
    ComLinkMd5   announced;
    ComLinkMd5   expected;
    ComLinkTable analog;
    ComLinkTable digital;
};
```

An access then reads as a sentence (`peer->analog.ids[i]`, `peer->expected.stable`), a function that handles one half takes one `ComLinkTable`, and a whole sub-structure is copied in one assignment (`ctx->expected = peer->expected;`).

Keep in mind:

* name a sub-structure after its concern (`ComLinkWatch`, `ComLinkCoupling`), and its members after their role, without repeating the prefix the member name already gives (`analog.count`, not `analog.nAnalog`);
* the regrouping costs nothing at run time: the same fields, the same bytes (padding aside); when the structure is generated or sent, check that the generated files and the MD5 do not move;
* do not group two or three unrelated fields for the sake of symmetry: a short, homogeneous structure stays flat.

---

## 5. Where a Structure Lives

`include/struct/` is deprecated. A new structure goes at the top of the **master header of its theme**, the one that already defines the neighbouring type (for example `ComDev` in `core/system/combus/protocol/combus_protocol.h`).

When several related types form a theme of their own (pure definitions, used by several modules, without code), they move to a `<theme>_defs.h` file next to the master header, which includes it. Such a file holds type definitions only: no feature flag, no function. Examples: `combus_frame_defs.h` (the frame), `combus_peer_link_defs.h` (a link between two nodes: `ComLinkMd5`, `ComLinkTable`, `ComPeerLink`).
