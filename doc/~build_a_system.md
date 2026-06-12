# Building an Open RC Node System

This document covers the practical aspects of building, configuring and deploying an Open RC Node system.

Topics include:

- Hardware selection
- Development environment setup
- PlatformIO configuration
- Firmware build process
- Configuration files
- Upload and testing procedures

---

## Build Flags

PlatformIO environments use a small set of build flags to select the target machine and hardware configuration.

### Vehicle Selection

```ini
-D MACHINE=<vehicle>
```

Selects the machine configuration to build.

Example:

```ini
-D MACHINE=VOLVO_A60_H_BRUDER
```

---

### Environment Type

```ini
-D IS_MACHINE
```

Identifies a machine firmware build.

---

### Main Board Selection

```ini
-D IS_MAINBOARD
```

Selects the main board configuration tree.

---

### Extension Board Selection

```ini
-D IS_EXT_BOARD
```

Selects the extension board configuration tree.

---

### Board Override

```ini
-D BOARD=<board>
```

Overrides the default board selected by the machine configuration.

Example:

```ini
-D BOARD=ESP32_8M_6S
```

---

Additional build flags and examples will be documented here as the project evolves.