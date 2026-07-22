# Script pour corriger les 4 dernières occurrences de ChanOwner

Write-Host "=== CORRECTION DES DERNIÈRES OCCURRENCES ==="

# 1. cb_gear.h
$gearFile = "src/core/system/combus/processors/modules/gear/cb_gear.h"
if (Test-Path $gearFile) {
    $content = Get-Content $gearFile -Raw
    $newContent = $content -replace '#include <struct/combus_proc_struct\.h>\s*//\s*CbProc,\s*ChanOwner', '#include <struct/combus_proc_struct.h>  // CbProc, CbProcFn'
    if ($newContent -ne $content) {
        Set-Content $gearFile $newContent -NoNewline
        Write-Host "✓ cb_gear.h corrigé"
    }
}

# 2. cb_cruise.h  
$cruiseFile = "src/core/system/combus/processors/motion/cb_cruise.h"
if (Test-Path $cruiseFile) {
    $content = Get-Content $cruiseFile -Raw
    $newContent = $content -replace '#include <struct/combus_proc_struct\.h>\s*//\s*CbProc,\s*ChanOwner', '#include <struct/combus_proc_struct.h>  // CbProc, CbProcFn'
    if ($newContent -ne $content) {
        Set-Content $cruiseFile $newContent -NoNewline
        Write-Host "✓ cb_cruise.h corrigé"
    }
}

# 3. combus_sound_interpreter.cpp
$soundFile = "src/sound_module/system/combus_sound_interpreter.cpp"
if (Test-Path $soundFile) {
    $content = Get-Content $soundFile -Raw
    # Corriger le commentaire
    $newContent = $content -replace 'Uses `ChanOwner::SYSTEM_EXT` — the caller identity', 'Uses `ChanLayer::REMOTE` — the caller identity'
    # Corriger l'include
    $newContent = $newContent -replace '#include <struct/combus_struct\.h>\s*//\s*ChanOwner', '#include <struct/combus_struct.h>  // ChanLayer'
    if ($newContent -ne $content) {
        Set-Content $soundFile $newContent -NoNewline
        Write-Host "✓ combus_sound_interpreter.cpp corrigé"
    }
}

# Vérifier le résultat
Write-Host "`n=== VÉRIFICATION FINALE ==="
$remaining = (findstr /s /i "ChanOwner" src\*.h src\*.cpp 2>$null | measure-object -line).Lines
Write-Host "Occurrences ChanOwner restantes: $remaining"

if ($remaining -eq 0) {
    Write-Host "🎉 TOUTES LES OCCURRENCES ONT ÉTÉ CORRIGÉES !"
} else {
    Write-Host "⚠️  Il reste $remaining occurrence(s) à corriger:"
    findstr /s /i "ChanOwner" src\*.h src\*.cpp 2>$null
}

Write-Host "`n=== Fin du script ==="