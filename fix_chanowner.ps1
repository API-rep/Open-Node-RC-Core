# Script PowerShell pour corriger automatiquement les occurrences de ChanOwner
# dans le cadre du rework ComBus

Write-Host "=== Script de correction ChanOwner ===" -ForegroundColor Cyan
Write-Host "Début de l'exécution..." -ForegroundColor Yellow

# 1. Compter les occurrences avant correction
Write-Host "`n1. Compter les occurrences avant correction..." -ForegroundColor Green
$beforeCount = (Select-String -Path "src\*.h", "src\*.cpp" -Pattern "ChanOwner" -Recurse | Measure-Object).Count
Write-Host "Occurrences avant correction: $beforeCount" -ForegroundColor Yellow

# 2. Corriger les signatures de fonctions dans les fichiers .h
Write-Host "`n2. Correction des signatures dans les fichiers .h..." -ForegroundColor Green
$hFiles = Get-ChildItem -Path "src" -Include "*.h" -Recurse | Where-Object { $_ | Select-String -Pattern "ChanOwner" }

foreach ($file in $hFiles) {
    Write-Host "  Traitement: $($file.FullName)" -ForegroundColor Gray
    
    # Lire le contenu
    $content = Get-Content $file.FullName -Raw
    
    # Remplacer les signatures de fonctions
    # Pattern: void nom_fn(CbProc* proc, uint16_t& value, bool& claimed, ChanOwner nom)
    $newContent = $content -replace 'void\s+(\w+)_fn\s*\(\s*CbProc\s*\*\s*proc\s*,\s*uint16_t\s*&\s*value\s*,\s*bool\s*&\s*claimed\s*,\s*ChanOwner\s+\w+\s*\)', 'void $1_fn(CbProc* proc, uint16_t& value, bool& claimed)'
    
    # Remplacer les includes obsolètes
    $newContent = $newContent -replace '//\s*ChanOwner', '// CbProcFn'
    $newContent = $newContent -replace '#include\s+<struct/combus_proc_struct\.h>\s*//\s*CbProc,\s*CbProcFn,\s*ChanOwner', '#include <struct/combus_proc_struct.h>  // CbProc, CbProcFn'
    $newContent = $newContent -replace '#include\s+<struct/combus_struct\.h>\s*//\s*ChanOwner', '#include <struct/combus_struct.h>  // ChanLayer'
    
    # Écrire le fichier si modifié
    if ($newContent -ne $content) {
        Set-Content -Path $file.FullName -Value $newContent -NoNewline
        Write-Host "    ✓ Modifié" -ForegroundColor Green
    } else {
        Write-Host "    ✗ Non modifié" -ForegroundColor DarkGray
    }
}

# 3. Corriger les implémentations dans les fichiers .cpp
Write-Host "`n3. Correction des implémentations dans les fichiers .cpp..." -ForegroundColor Green
$cppFiles = Get-ChildItem -Path "src" -Include "*.cpp" -Recurse | Where-Object { $_ | Select-String -Pattern "ChanOwner" }

foreach ($file in $cppFiles) {
    Write-Host "  Traitement: $($file.FullName)" -ForegroundColor Gray
    
    # Lire le contenu
    $content = Get-Content $file.FullName -Raw
    
    # Remplacer les signatures de fonctions
    $newContent = $content -replace 'void\s+(\w+)_fn\s*\(\s*CbProc\s*\*\s*proc\s*,\s*uint16_t\s*&\s*value\s*,\s*bool\s*&\s*claimed\s*,\s*ChanOwner\s+(\w+)\s*\)', 'void $1_fn(CbProc* proc, uint16_t& value, bool& claimed)'
    
    # Remplacer les appels à combus_set_* avec chainOwner
    $newContent = $newContent -replace 'combus_set_analog\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([^,]+)\s*,\s*\w+\s*\)', 'combus_set_analog($1, $2, $3)'
    $newContent = $newContent -replace 'combus_set_digital\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([^,]+)\s*,\s*\w+\s*\)', 'combus_set_digital($1, $2, $3)'
    $newContent = $newContent -replace 'combus_set_runlevel\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*\w+\s*\)', 'combus_set_runlevel($1, $2)'
    $newContent = $newContent -replace 'combus_set_battlow\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*\w+\s*\)', 'combus_set_battlow($1, $2)'
    
    # Écrire le fichier si modifié
    if ($newContent -ne $content) {
        Set-Content -Path $file.FullName -Value $newContent -NoNewline
        Write-Host "    ✓ Modifié" -ForegroundColor Green
    } else {
        Write-Host "    ✗ Non modifié" -ForegroundColor DarkGray
    }
}

# 4. Compter les occurrences après correction
Write-Host "`n4. Compter les occurrences après correction..." -ForegroundColor Green
$afterCount = (Select-String -Path "src\*.h", "src\*.cpp" -Pattern "ChanOwner" -Recurse | Measure-Object).Count
Write-Host "Occurrences après correction: $afterCount" -ForegroundColor Yellow

# 5. Résumé
Write-Host "`n=== RÉSUMÉ ===" -ForegroundColor Cyan
Write-Host "Occurrences avant: $beforeCount" -ForegroundColor White
Write-Host "Occurrences après: $afterCount" -ForegroundColor White
$reduction = $beforeCount - $afterCount
Write-Host "Réduction: $reduction occurrences" -ForegroundColor $(if ($reduction -gt 0) { "Green" } else { "Red" })

if ($afterCount -eq 0) {
    Write-Host "`n🎉 Toutes les occurrences de ChanOwner ont été corrigées !" -ForegroundColor Green
} else {
    Write-Host "`n⚠️ Il reste $afterCount occurrences à corriger manuellement." -ForegroundColor Yellow
    Write-Host "Fichiers restants:" -ForegroundColor Yellow
    Select-String -Path "src\*.h", "src\*.cpp" -Pattern "ChanOwner" -Recurse | ForEach-Object {
        Write-Host "  - $($_.FileName): ligne $($_.LineNumber)" -ForegroundColor Gray
    }
}

Write-Host "`n=== Fin du script ===" -ForegroundColor Cyan