# Local Runtime Engines

Local runtime engines are small execution mechanisms used internally by a subsystem when simple data structures are no longer sufficient to express its behaviour.

They are not shared concepts of the Open RC Node ecosystem. They are implementation details designed to solve localized execution problems.

---

## Why does this concept exist?

Most of Open RC Node relies on simple structures describing data:

* definitions (`cfg`);
* runtime state (`state`);
* aggregation structures.

In some situations, however, a subsystem starts needing behaviour in addition to data:

* execution logic;
* ordered processing;
* local decision making;
* reusable execution patterns.

The natural object-oriented response would be to introduce a small class.

For example:

```cpp
class RampProcessor {
public:
    void update();

private:
    RampCfg cfg;
    RampState state;
};
```

As the system evolves, this often becomes:

```cpp
class Processor {
public:
    virtual void update() = 0;
};

class RampProcessor   : public Processor { ... };
class MixerProcessor  : public Processor { ... };
class FilterProcessor : public Processor { ... };
```

Conceptually, local runtime engines solve the same problem.

However, they do so using explicit structures and functions rather than dynamic object hierarchies.

---

## The Open RC Node Approach

Instead of hiding behaviour behind objects, dependencies remain visible.

```cpp
struct RampCfg {
    ...
};

struct RampState {
    ...
};

struct RampEngine {
    const RampCfg* cfg;
    RampState* state;
};

void ramp_update(RampEngine* engine);
```

A local runtime engine can therefore be viewed as:

> **a small class without the object-oriented machinery.**

The objective is not to reject classes entirely. The objective is to preserve the qualities that matter most in an embedded environment.

---

## Why prefer this approach?

Keeping execution mechanisms explicit provides several advantages.

Flash and RAM ownership remain immediately visible:

```cpp
const RampCfg* cfg;   // Flash
RampState* state;     // RAM
```

Execution flow remains easy to follow:

```cpp
ramp_update(&engine);
```

is often simpler to trace and debug than a chain of virtual calls.

Avoiding dynamic polymorphism also gives the compiler more freedom to optimise generated code through techniques such as inlining and dead-code elimination.

Finally, the mechanism introduces only the complexity required by the problem being solved. No hidden framework, runtime registry or object hierarchy is needed.

The result is execution that remains predictable, efficient and understandable.

---

## When should a local runtime engine be used?

A local runtime engine becomes relevant when you find yourself thinking:

> "This structure really needs a method."

or:

> "I could solve this by introducing a small dedicated class."

It allows local behaviour to emerge without pushing the entire subsystem toward an object-oriented architecture.

Typical examples include:

* processor runners;
* dispatch tables;
* mixers;
* sequencers;
* lightweight schedulers;
* finite-state-machine runners.

---

## When should it NOT be used?

A local runtime engine must remain local to the subsystem it serves.

It should not become:

* a framework;
* a middleware layer;
* a plugin system;
* a generic interpreter;
* a hidden operating system.

Warning signs include:

* multiple unrelated modules depending on it;
* increasing levels of indirection;
* abstractions becoming more complex than the original problem.

In those situations, explicit procedural code is often the better choice.

---

## Placement Within the Project

Local runtime engines belong to the subsystem that executes them.

Prefer:

```text
core/system/combus/
    combus_engine.cpp
    combus_engine.h
```

rather than exposing them as shared structures:

```text
include/struct/
    processor_struct.h    ✗
```

unless the descriptor itself genuinely represents a reusable concept shared across the project.

---

## Relationship with Shared Structures

A common evolution pattern looks like this:

```text
Simple structure
        ↓
Structure + helper
        ↓
Structure + state + dedicated function
        ↓
Local runtime engine
```

This progression is natural.

The important distinction is recognising when a structure stops describing data and starts describing execution.

At that point, it should move into the runtime layer of the subsystem that owns it.

---

## Rule of Thumb

```text
Simple data structure
    ✓ Preferred

Structure with small helpers
    ✓ Acceptable

Local runtime engine
    ✓ Appropriate when justified

Shared execution framework
    ⚠ Reconsider

Entire firmware built around it
    ✗ Avoid
```

Local runtime engines exist to solve local execution problems.

They should never become the architecture of the entire system.
