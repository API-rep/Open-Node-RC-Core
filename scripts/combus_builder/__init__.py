# combus_builder/__init__.py
#
# ComBus v2 builder package.
#
# Pipeline: parser -> canon -> (diffusion) -> generator -> md5.
# See scripts/combus_builder/README.md for the module map.
#
# Modules:
#   scripts/combus_builder/
#       parser.py        — discovery + parsing of .cb / .cbch
#       flags.py         — BuildContext (defines)
#       canon/           — validation, merge, canonical sort
#       machine_type.py  — machine type filter
#       node_context.py  — node / board detection
#       diffusion.py     — SYSTEM diffusion across boards
#       generator.py     — views, IDs, C++ emission
#       md5.py           — contract hashes
#       coherence.py     — consistency checks with platformio.ini
