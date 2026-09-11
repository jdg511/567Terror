$ErrorActionPreference = 'Continue'
$log = 'C:\Users\Jason\source\repos\Glitchwave\hardware\review\tools\make_fab_run.log'
"START $(Get-Date -Format o)" | Out-File -FilePath $log -Encoding utf8
try {
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\Users\Jason\source\repos\Glitchwave\hardware\review\tools\make_fab.ps1' *>&1 | Out-File -FilePath $log -Append -Encoding utf8
  "EXITCODE $LASTEXITCODE" | Out-File -FilePath $log -Append -Encoding utf8
} catch {
  "EXCEPTION $_" | Out-File -FilePath $log -Append -Encoding utf8
}
"DONE $(Get-Date -Format o)" | Out-File -FilePath $log -Append -Encoding utf8
