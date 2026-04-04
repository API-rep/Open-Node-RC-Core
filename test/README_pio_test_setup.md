# PlatformIO 6 — Comment faire fonctionner les tests

## Règles obligatoires (validées 15/03/2026 sur PIO 6.1.19)

### 1. Le dossier de test DOIT commencer par `test_`

PlatformIO 6 ignore silencieusement tout dossier sous `test/` dont le nom ne
commence pas par `test_`. Pas d'erreur, juste "Nothing to build".

```
test/test_combus_loopback/   ✅
test/combus_loopback/        ❌ ignoré
```

### 2. `test_filter` = le nom du dossier exactement, sans glob

```ini
test_filter = test_combus_loopback     ✅
test_filter = combus_loopback/*        ❌
```

### 3. `test_build_src = yes` est obligatoire

Par défaut cette option vaut `False` en PIO 6. Sans elle, les fichiers sources du
projet (`src/`) ne sont **pas** compilés — résultat : des dizaines de
`undefined reference` au link, même si `build_src_filter` est correct.

### 4. `build_src_filter` sur UNE SEULE ligne

```ini
build_src_filter = -<*> +<core/system/combus/**> +<core/system/hw/**>   ✅

; Syntaxe multiligne cassée pour ce paramètre :
build_src_filter =    ❌
    -<*>
    +<core/system/combus/**>
```

### 5. Stub `config/config.h` dans le dossier de la suite

`uart_com.cpp` inclut `<config/config.h>` pour lire `UartComMaxPorts`.
Le test runner ajoute automatiquement le dossier de la suite au chemin d'include
(`CPPPATH`), donc un stub minimal suffit :

```
test/test_combus_loopback/config/config.h   ← stub, déclaré ici
```

Si une autre suite a besoin du même mécanisme, copier ce fichier dans son dossier.

---

## Template `platformio.ini` pour une nouvelle suite

```ini
[env:test_<nom>]
test_framework   = unity
test_filter      = test_<nom>
test_build_src   = yes                       ; NE PAS OUBLIER
build_src_filter = -<*> +<les/sources/**>
build_flags      = ${env.build_flags} -D LES_FLAGS -I src -I include
```

## Commandes

```bash
# Compile + upload + run
pio test -e test_<nom>

# Compile seulement (sans ESP32 branché)
pio test -e test_<nom> --without-uploading --without-testing

# Lister les suites découvertes
pio test -e test_<nom> --list-tests
```

## Erreur fréquente : `TypeError: unsupported operand type(s) for -: 'float' and 'NoneType'`

Erreur SCons interne lors d'une toute première installation de lib (cache corrompu).
Solution : supprimer le cache et relancer.

```powershell
Remove-Item "$env:LOCALAPPDATA\Temp\PlatformIO\build\Open_Node_RC_Core\test_<nom>" -Recurse -Force
Remove-Item ".pio\libdeps\test_<nom>" -Recurse -Force
```
