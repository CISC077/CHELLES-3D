# Cree la tache Windows "CHELLES 3D - Verification erreurs" (tous les jours)
param([string]$Heure = "23:00")
$ErrorActionPreference = "Stop"
$nom = "CHELLES 3D - Verification erreurs"
$bat = Join-Path $PSScriptRoot "Verifier_maintenant.bat"
try {
  $action   = New-ScheduledTaskAction -Execute $bat -Argument "auto" -WorkingDirectory $PSScriptRoot
  $trigger  = New-ScheduledTaskTrigger -Daily -At $Heure
  $settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Hours 2)
  $principal = New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -LogonType Interactive -RunLevel Limited
  Register-ScheduledTask -TaskName $nom -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Description "Verifie chaque soir les erreurs de chargement du projet CHELLES 3D et corrige ce qui est sur (rien n'est supprime)." -Force | Out-Null
  Write-Host ""
  Write-Host "  OK : la verification se lancera tous les jours a $Heure."
  Write-Host "  Si le PC est eteint a cette heure-la, elle se lancera au prochain demarrage."
} catch {
  Write-Host "  Methode 1 impossible ($($_.Exception.Message)), j'essaie l'autre..."
  schtasks /Create /F /SC DAILY /ST $Heure /TN "$nom" /TR "`"$bat`" auto" | Out-Host
  if ($LASTEXITCODE -eq 0) { Write-Host "  OK : tous les jours a $Heure." } else { Write-Host "  ECHEC : dis-le a Claude." ; exit 1 }
}
