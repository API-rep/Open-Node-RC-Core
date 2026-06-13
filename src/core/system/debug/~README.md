# Debug Architecture

Open RC Node provides two complementary debugging systems designed for different purposes.

Although they share some infrastructure and coexist within the same terminal environment, they target different audiences and answer different questions.

---

## Two Complementary Perspectives

### Initialization Logging

Serial logs explain what the software is doing internally.

They are primarily intended for developers during:

* hardware bring-up;
* startup diagnostics;
* configuration validation;
* troubleshooting.

They provide a chronological view of the system's execution.

Typical questions answered by serial logs include:

> Did initialization succeed?

> Which configuration was loaded?

> Why did this subsystem fail?

---

### Runtime Dashboard

The dashboard exposes what the system is doing right now.

It is intended for runtime monitoring and operational diagnostics.

Rather than describing execution history, it presents the current state of the system through interactive views.

Typical questions answered by the dashboard include:

> What is the machine currently doing?

> Which values are changing?

> Which modules are active?

> What events recently occurred?

---

## Complementary Tools

The two systems are not competitors.

They provide two different perspectives on the same system:

| Initialization Logging | Runtime Dashboard  |
| ---------------------- | ------------------ |
| Developer-oriented     | Operator-oriented  |
| Chronological          | State-oriented     |
| Startup diagnostics    | Runtime monitoring |
| Textual traces         | Interactive views  |
| Explains behavior      | Observes behavior  |

Together, they provide complete visibility throughout development, validation and operation.

---

## Directory Structure

```text
src/core/system/debug/
├─ README.md
├─ dashboard/dashboard.h/.cpp
├─ loggin/debug.h/.cpp
```

* `dashboard/` contains the runtime monitoring subsystem.
* `debug/` contains the serial logging subsystem.
* Shared infrastructure remains in the current directory.

---

## Design Philosophy

Both systems follow the same principles:

* remain decoupled from application logic;
* impose minimal overhead;
* provide useful information without excessive noise;
* evolve alongside the project as new modules emerge.

Open RC Node aims to make understanding the system as important as building it.
