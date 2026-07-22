# Script PowerShell pour terminer le rework ComBus
# Corrige toutes les occurrences restantes de ChanOwner et makeChanOwner

Write-Host "=== FINALISATION DU REWORK COMBUS ===" -ForegroundColor Cyan
Write-Host "Début de l'exécution..." -ForegroundColor Yellow

# 1. Compter les occurrences avant correction
Write-Host "`n1. Compter les occurrences avant correction..." -ForegroundColor Green
$chanOwnerBefore = (Select-String -Path "src\*.h", "src\*.cpp" -Pattern "ChanOwner" -Recurse | Measure-Object).Count
$makeChanOwnerBefore = (Select-String -Path "src\*.h", "src\*.cpp" -Pattern "makeChanOwner" -Recurse | Measure-Object).Count
Write-Host "ChanOwner avant: $chanOwnerBefore" -ForegroundColor Yellow
Write-Host "makeChanOwner avant: $makeChanOwnerBefore" -ForegroundColor Yellow

# 2. Corriger les signatures de fonctions dans les fichiers .cpp
Write-Host "`n2. Correction des signatures de fonctions..." -ForegroundColor Green
$cppFiles = Get-ChildItem -Path "src" -Include "*.cpp" -Recurse | Where-Object { $_ | Select-String -Pattern "void.*ChanOwner" }

foreach ($file in $cppFiles) {
    Write-Host "  Traitement: $($file.Name)" -ForegroundColor Gray
    
    $content = Get-Content $file.FullName -Raw
    
    # Remplacer les signatures avec ChanOwner
    $newContent = $content -replace 'void\s+(\w+)_fn\s*\(\s*CbProc\s*\*\s*proc\s*,\s*uint16_t\s*&\s*value\s*,\s*bool\s*&\s*claimed\s*,\s*ChanOwner\s+\w+\s*\)', 'void $1_fn(CbProc* proc, uint16_t& value, bool& claimed)'
    
    # Remplacer les signatures avec commentaires
    $newContent = $newContent -replace 'void\s+(\w+)_fn\s*\(\s*CbProc\s*\*\s*proc\s*,\s*uint16_t\s*&\s*value\s*,\s*bool\s*&\s*/\*claimed\*/,\s*ChanOwner\s+/\*\w+\*/\)', 'void $1_fn(CbProc* proc, uint16_t& value, bool& /*claimed*/)'
    
    if ($newContent -ne $content) {
        Set-Content -Path $file.FullName -Value $newContent -NoNewline
        Write-Host "    ✓ Signatures corrigées" -ForegroundColor Green
    }
}

# 3. Remplacer makeChanOwner dans tous les fichiers
Write-Host "`n3. Remplacement de makeChanOwner..." -ForegroundColor Green
$allFiles = Get-ChildItem -Path "src" -Include "*.cpp", "*.h" -Recurse | Where-Object { $_ | Select-String -Pattern "makeChanOwner" }

foreach ($file in $allFiles) {
    Write-Host "  Traitement: $($file.Name)" -ForegroundColor Gray
    
    $content = Get-Content $file.FullName -Raw
    
    # Remplacer makeChanOwner par les valeurs ChanLayer appropriées
    $newContent = $content -replace 'makeChanOwner\(EnvNodeGroup,\s*ComBusOwner::PROC_SYSTEM\)', 'ChanLayer::LOCAL'
    $newContent = $newContent -replace 'makeChanOwner\(EnvNodeGroup,\s*ComBusOwner::PROC_VBAT\)', 'ChanLayer::LOCAL'
    $newContent = $newContent -replace 'makeChanOwner\(EnvNodeGroup,\s*ComBusOwner::PROC_BRIDGE\)', 'ChanLayer::REMOTE'
    
    # Cas spéciaux pour main.cpp - utiliser les surcharges sans paramètre
    if ($file.Name -eq "main.cpp") {
        $newContent = $newContent -replace 'combus_set_runlevel\(comBus,\s*RunLevel::(\w+),\s*ChanLayer::LOCAL\)', 'combus_set_runlevel(comBus, RunLevel::$1)'
        $newContent = $newContent -replace 'combus_set_battlow\(comBus,\s*true,\s*ChanLayer::LOCAL\)', 'combus_set_battlow(comBus, true)'
        $newContent = $newContent -replace 'combus_set_digital\(comBus,\s*DigitalComBusID::BATTERY_LOW,\s*true,\s*ChanLayer::LOCAL\)', 'combus_set_digital(comBus, DigitalComBusID::BATTERY_LOW, true)'
    }
    
    if ($newContent -ne $content) {
        Set-Content -Path $file.FullName -Value $newContent -NoNewline
        Write-Host "    ✓ makeChanOwner corrigé" -ForegroundColor Green
    }
}

# 4. Mettre à jour les includes obsolètes
Write-Host "`n4. Mise à jour des includes..." -ForegroundColor Green
$hFiles = Get-ChildItem -Path "src" -Include "*.h" -Recurse | Where-Object { $_ | Select-String -Pattern "ChanOwner" }

foreach ($file in $hFiles) {
    Write-Host "  Traitement: $($file.Name)" -ForegroundColor Gray
    
    $content = Get-Content $file.FullName -Raw
    
    # Remplacer les commentaires d'include
    $newContent = $content -replace '#include\s+<struct/combus_proc_struct\.h>\s*//\s*CbProc,\s*ChanOwner', '#include <struct/combus_proc_struct.h>  // CbProc, CbProcFn'
    $newContent = $newContent -replace '#include\s+<struct/combus_struct\.h>\s*//\s*ChanOwner', '#include <struct/combus_struct.h>  // ChanLayer'
    $newContent = $newContent -replace '//\s*ChanOwner', '// CbProcFn'
    
    if ($newContent -ne $content) {
        Set-Content -Path $file.FullName -Value $newContent -NoNewline
        Write-Host "    ✓ Includes mis à jour" -ForegroundColor Green
    }
}

# 5. Compter les occurrences après correction
Write-Host "`n5. Compter les occurrences après correction..." -ForegroundColor Green
$chanOwnerAfter = (Select-String -Path "src\*.h", "src\*.cpp" -Pattern "ChanOwner" -Recurse | Measure-Object).Count
$makeChanOwnerAfter = (Select-String -Path "src\*.h", "src\*.cpp" -Pattern "makeChanOwner" -Recurse | Measure-Object).Count

Write-Host "ChanOwner après: $chanOwnerAfter" -ForegroundColor Yellow
Write-Host "makeChanOwner après: $makeChanOwnerAfter" -ForegroundColor Yellow

# 6. Résumé
Write-Host "`n=== RÉSUMÉ FINAL ===" -ForegroundColor Cyan
Write-Host "ChanOwner: $chanOwnerBefore → $chanOwnerAfter (réduction: $($chanOwnerBefore - $chanOwnerAfter))" -ForegroundColor White
Write-Host "makeChanOwner: $makeChanOwnerBefore → $makeChanOwnerAfter (réduction: $($makeChanOwnerBefore - $makeChanOwnerAfter))" -ForegroundColor White

if ($chanOwnerAfter -eq 0 -and $makeChanOwnerAfter -eq 0) {
    Write-Host "`n🎉 REWORK COMBUS TERMINÉ AVEC SUCCÈS !" -ForegroundColor Green
    Write-Host "Toutes les occurrences ont été corrigées." -ForegroundColor Green
} else {
    Write-Host "`n⚠️ Il reste des occurrences à corriger manuellement:" -ForegroundColor Yellow
    
    if ($chanOwnerAfter -gt 0) {
        Write-Host "`nFichiers avec ChanOwner restant:" -ForegroundColor Yellow
        Select-String -Path "src\*.h", "src\*.cpp" -Pattern "ChanOwner" -Recurse | ForEach-Object {
            Write-Host "  - $($_.FileName): ligne $($_.LineNumber)" -ForegroundColor Gray
        }
    }
    
    if ($makeChanOwnerAfter -gt 0) {
        Write-Host "`nFichiers avec makeChanOwner restant:" -ForegroundColor Yellow
        Select-String -Path "src\*.h", "src\*.cpp" -Pattern "makeChanOwner" -Recurse | ForEach-Object {
            Write-Host "  - $($_.FileName): ligne $($_.LineNumber)" -ForegroundColor Gray
        }
    }
}

Write-Host "`n=== Fin du script ===" -ForegroundColor Cyan
Write-Host "`nRecommandation: Compiler pour vérifier les erreurs:" -ForegroundColor White
Write-Host "  pio run" -ForegroundColor Gray