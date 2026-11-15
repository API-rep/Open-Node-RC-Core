# Configuration Files

This section describes the file formats that configure an Open RC Node system. Each format has its syntax reference page.

| Format | Extension | Role | Reference |
|---|---|---|---|
| ComBus channel declaration | `.cb` (YAML) | Declares the ComBus channels (identifier, type, scope, direction, activation, safe value). Read by the ComBus Builder at build time. | [cb_syntax.md](cb_syntax.md) |

## Common Rules

- The files are YAML. A file must be a mapping at its root; an empty file or a list at the root is a build error.
- The validation is strict: an unknown key in a channel is refused with a message that names the file and the position of the channel.
- A `.cb` file can be placed anywhere under `src/`, next to the module it describes. The details of the search are in [cb_syntax.md](cb_syntax.md).

How the ComBus Builder works inside (to modify the builder itself): [ComBus Builder](../../scripts/combus_builder/README.md).

## Related Documentation

- [Machine configuration system](../../src/machines/config/~README.md)
- [Machine family configuration](../../src/core/config/machines/README.md)
- [Shared data definitions (`include/`)](../../include/README.md)
