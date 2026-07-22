@echo off
echo === Script de correction ChanOwner ===
echo.

echo 1. Compter les occurrences avant correction...
for /f %%a in ('findstr /s /i "ChanOwner" src\*.h src\*.cpp 2^>nul ^| find /c "ChanOwner"') do set beforeCount=%%a
echo Occurrences avant correction: %beforeCount%
echo.

echo 2. Correction des signatures dans les fichiers .h...
echo.

REM Trouver tous les fichiers .h avec ChanOwner
for /r src %%f in (*.h) do (
    findstr /i "ChanOwner" "%%f" >nul 2>&1
    if not errorlevel 1 (
        echo Traitement: %%f
        
        REM Créer un fichier temporaire
        set "tempFile=%%f.tmp"
        
        REM Utiliser PowerShell pour faire les remplacements
        powershell -Command "(Get-Content '%%f' -Raw) -replace 'void\s+(\w+)_fn\s*\(\s*CbProc\s*\*\s*proc\s*,\s*uint16_t\s*&\s*value\s*,\s*bool\s*&\s*claimed\s*,\s*ChanOwner\s+\w+\s*\)', 'void $1_fn(CbProc* proc, uint16_t& value, bool& claimed)' | Set-Content '%%f' -NoNewline"
        
        powershell -Command "(Get-Content '%%f' -Raw) -replace '//\s*ChanOwner', '// CbProcFn' | Set-Content '%%f' -NoNewline"
        powershell -Command "(Get-Content '%%f' -Raw) -replace '#include\s+<struct/combus_proc_struct\.h>\s*//\s*CbProc,\s*CbProcFn,\s*ChanOwner', '#include <struct/combus_proc_struct.h>  // CbProc, CbProcFn' | Set-Content '%%f' -NoNewline"
        powershell -Command "(Get-Content '%%f' -Raw) -replace '#include\s+<struct/combus_struct\.h>\s*//\s*ChanOwner', '#include <struct/combus_struct.h>  // ChanLayer' | Set-Content '%%f' -NoNewline"
        
        echo    ✓ Modifié
    )
)

echo.
echo 3. Correction des implémentations dans les fichiers .cpp...
echo.

REM Trouver tous les fichiers .cpp avec ChanOwner
for /r src %%f in (*.cpp) do (
    findstr /i "ChanOwner" "%%f" >nul 2>&1
    if not errorlevel 1 (
        echo Traitement: %%f
        
        REM Utiliser PowerShell pour faire les remplacements
        powershell -Command "(Get-Content '%%f' -Raw) -replace 'void\s+(\w+)_fn\s*\(\s*CbProc\s*\*\s*proc\s*,\s*uint16_t\s*&\s*value\s*,\s*bool\s*&\s*claimed\s*,\s*ChanOwner\s+\w+\s*\)', 'void $1_fn(CbProc* proc, uint16_t& value, bool& claimed)' | Set-Content '%%f' -NoNewline"
        
        REM Remplacer les appels à combus_set_* avec chainOwner
        powershell -Command "(Get-Content '%%f' -Raw) -replace 'combus_set_analog\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([^,]+)\s*,\s*\w+\s*\)', 'combus_set_analog($1, $2, $3)' | Set-Content '%%f' -NoNewline"
        powershell -Command "(Get-Content '%%f' -Raw) -replace 'combus_set_digital\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([^,]+)\s*,\s*\w+\s*\)', 'combus_set_digital($1, $2, $3)' | Set-Content '%%f' -NoNewline"
        powershell -Command "(Get-Content '%%f' -Raw) -replace 'combus_set_runlevel\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*\w+\s*\)', 'combus_set_runlevel($1, $2)' | Set-Content '%%f' -NoNewline"
        powershell -Command "(Get-Content '%%f' -Raw) -replace 'combus_set_battlow\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*\w+\s*\)', 'combus_set_battlow($1, $2)' | Set-Content '%%f' -NoNewline"
        
        echo    ✓ Modifié
    )
)

echo.
echo 4. Compter les occurrences après correction...
for /f %%a in ('findstr /s /i "ChanOwner" src\*.h src\*.cpp 2^>nul ^| find /c "ChanOwner"') do set afterCount=%%a
echo Occurrences après correction: %afterCount%
echo.

echo === RÉSUMÉ ===
echo Occurrences avant: %beforeCount%
echo Occurrences après: %afterCount%
set /a reduction=beforeCount - afterCount
echo Réduction: %reduction% occurrences
echo.

if %afterCount% equ 0 (
    echo 🎉 Toutes les occurrences de ChanOwner ont été corrigées !
) else (
    echo ⚠️ Il reste %afterCount% occurrences à corriger manuellement.
    echo Fichiers restants:
    findstr /s /i "ChanOwner" src\*.h src\*.cpp 2>nul
)

echo.
echo === Fin du script ===
pause