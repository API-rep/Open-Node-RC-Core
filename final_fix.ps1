# Script PowerShell simple pour terminer le rework ComBus

Write-Host "=== FINALISATION DU REWORK COMBUS ==="
Write-Host ""

# 1. Compter avant
Write-Host "1. Compter les occurrences avant correction..."
$chanBefore = (findstr /s /i "ChanOwner" src\*.h src\*.cpp 2>$null | measure-object -line).Lines
$makeBefore = (findstr /s /i "makeChanOwner" src\*.h src\*.cpp 2>$null | measure-object -line).Lines
Write-Host "   ChanOwner: $chanBefore"
Write-Host "   makeChanOwner: $makeBefore"
Write-Host ""

# 2. Corriger les signatures de fonctions
Write-Host "2. Correction des signatures de fonctions..."
Get-ChildItem -Path "src" -Include "*.cpp" -Recurse | ForEach-Object {
    $file = $_.FullName
    $content = Get-Content $file -Raw
    
    # Remplacer les signatures
    $newContent = $content -replace 'void\s+(\w+)_fn\s*\(\s*CbProc\s*\*\s*proc\s*,\s*uint16_t\s*&\s*value\s*,\s*bool\s*&\s*claimed\s*,\s*ChanOwner\s+\w+\s*\)', 'void $1_fn(CbProc* proc, uint16_t& value, bool& claimed)'
    $newContent = $newContent -replace 'void\s+(\w+)_fn\s*\(\s*CbProc\s*\*\s*proc\s*,\s*uint16_t\s*&\s*value\s*,\s*bool\s*&\s*/\*claimed\*/,\s*ChanOwner\s+/\*\w+\*/\)', 'void $1_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/)'
    
    if ($newContent -ne $content) {
        Set-Content $file $newContent -NoNewline
        Write-Host "   Corrigé: $($_.Name)"
    }
}
Write-Host ""

# 3. Remplacer makeChanOwner
Write-Host "3. Remplacement de makeChanOwner..."
Get-ChildItem -Path "src" -Include "*.cpp","*.h" -Recurse | ForEach-Object {
    $file = $_.FullName
    $content = Get-Content $file -Raw
    
    $newContent = $content -replace 'makeChanOwner\(EnvNodeGroup,\s*ComBusOwner::PROC_SYSTEM\)', 'ChanLayer::LOCAL'
    $newContent = $newContent -replace 'makeChanOwner\(EnvNodeGroup,\s*ComBusOwner::PROC_VBAT\)', 'ChanLayer::LOCAL'
    $newContent = $newContent -replace 'makeChanOwner\(EnvNodeGroup,\s*ComBusOwner::PROC_BRIDGE\)', 'ChanLayer::REMOTE'
    
    if ($newContent -ne $content) {
        Set-Content $file $newContent -NoNewline
        Write-Host "   Corrigé: $($_.Name)"
    }
}
Write-Host ""

# 4. Pour main.cpp - utiliser les surcharges
Write-Host "4. Optimisation de main.cpp..."
$mainFile = "src/machines/main.cpp"
if (Test-Path $mainFile) {
    $content = Get-Content $mainFile -Raw
    $newContent = $content -replace 'combus_set_runlevel\(comBus,\s*RunLevel::(\w+),\s*ChanLayer::LOCAL\)', 'combus_set_runlevel(comBus, RunLevel::$1)'
    $newContent = $newContent -replace 'combus_set_battlow\(comBus,\s*true,\s*ChanLayer::LOCAL\)', 'combus_set_battlow(comBus, true)'
    $newContent = $newContent -replace 'combus_set_digital\(comBus,\s*DigitalComBusID::BATTERY_LOW,\s*true,\s*ChanLayer::LOCAL\)', 'combus_set_digital(comBus, DigitalComBusID::BATTERY_LOW, true)'
    
    if ($newContent -ne $content) {
        Set-Content $mainFile $newContent -NoNewline
        Write-Host "   Optimisé: main.cpp"
    }
}
Write-Host ""

# 5. Compter après
Write-Host "5. Compter les occurrences après correction..."
$chanAfter = (findstr /s /i "ChanOwner" src\*.h src\*.cpp 2>$null | measure-object -line).Lines
$makeAfter = (findstr /s /i "makeChanOwner" src\*.h src\*.cpp 2>$null | measure-object -line).Lines
Write-Host "   ChanOwner: $chanAfter"
Write-Host "   makeChanOwner: $makeAfter"
Write-Host ""

# 6. Résumé
Write-Host "=== RÉSUMÉ ==="
Write-Host "ChanOwner: $chanBefore -> $chanAfter (réduction: $($chanBefore - $chanAfter))"
Write-Host "makeChanOwner: $makeBefore -> $makeAfter (réduction: $($makeBefore - $makeAfter))"
Write-Host ""

if ($chanAfter -eq 0 -and $makeAfter -eq 0) {
    Write-Host "🎉 SUCCÈS: Toutes les occurrences ont été corrigées !"
} else {
    Write-Host "⚠️  ATTENTION: Il reste des occurrences à corriger manuellement"
    if ($chanAfter -gt 0) {
        Write-Host "   ChanOwner restant dans:"
        findstr /s /i "ChanOwner" src\*.h src\*.cpp 2>$null
    }
    if ($makeAfter -gt 0) {
        Write-Host "   makeChanOwner restant dans:"
        findstr /s /i "makeChanOwner" src\*.h src\*.cpp 2>$null
    }
}

Write-Host ""
Write-Host "=== Fin du script ==="
Write-Host "Recommandation: Compiler pour vérifier: pio run"