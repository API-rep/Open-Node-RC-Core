# Machine Configuration System

The machine configuration system defines how vehicle-specific configurations are organized within Open RC Node.

Its purpose is to provide a consistent and scalable structure that allows new vehicles, boards and machine types to be added without modifying the core architecture.

---

## Configuration Hierarchy

A machine configuration is organized into three levels:

```text
machines.h
  └─ <vehicle>/
       ├─ mainboard/
       │    └─ <board>/
       │         └─ envCfg.h/.cpp
       └─ ext_board/
```

### Level 0 — Vehicle Definition

The vehicle root directory defines the machine identity and its global characteristics.

Typical responsibilities:

- Vehicle name
- Machine type
- ComBus layout selection

This level acts as the entry point for the complete configuration tree.

---

### Level 1 — Environment Selection

The environment layer selects which hardware environment is being built.

Examples:

- Main board
- Extension board
- Dedicated module node

Each environment can expose its own board-specific configuration.

---

### Level 2 — Board Configuration

The board layer contains hardware-specific settings and initialization parameters.

Typical responsibilities:

- Pin assignments
- Hardware capabilities
- Peripheral configuration
- Environment-specific defaults

---

## Design Rules

### Minimal Vehicle Definition

Vehicle-level configuration should only define machine identity and high-level characteristics.

Examples:

- Vehicle name
- Machine type
- ComBus layout

Hardware-specific details belong to board configurations.

---

### Configuration Ownership

Each configuration level should only define information relevant to its responsibility.

Avoid duplicating configuration values across multiple levels.

---

### Vehicle Isolation

Adding a new vehicle should only require the creation of a dedicated vehicle directory following the standard hierarchy.

Core framework code should not require modification.

---

## Creating a New Vehicle

Creating a new machine typically involves:

1. Creating a new vehicle directory.
2. Defining the vehicle identity.
3. Creating the required board environments.
4. Providing board-specific configuration.
5. Selecting the appropriate machine class configuration.

This structure allows Open RC Node to scale from simple vehicles to more advanced multi-node systems while keeping configuration predictable and maintainable.