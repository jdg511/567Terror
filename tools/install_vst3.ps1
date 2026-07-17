# ============================================================
#  install_vst3.ps1 — copies the built VST3 into the system
#  VST3 folder so DAWs pick it up.
#
#  First run needs one admin approval (Windows UAC prompt).
#  It also grants your user account write permission on the
#  VST3 folder, so every run after that is silent/automatic.
# ============================================================

$src    = Join-Path (Split-Path $PSScriptRoot -Parent) 'build\Glitchwave567_artefacts\Release\VST3\Glitchwave 567.vst3'
$dstDir = Join-Path $env:CommonProgramFiles 'VST3'
$dst    = Join-Path $dstDir 'Glitchwave 567.vst3'

if (-not (Test-Path $src)) {
    Write-Host "VST3 not built yet - run build.bat first. (Looked for: $src)"
    exit 1
}

# --- try the plain copy first (works once permissions are granted) -----------
try {
    if (-not (Test-Path $dstDir)) { throw 'need admin to create folder' }
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force -ErrorAction Stop }
    Copy-Item $src $dst -Recurse -Force -ErrorAction Stop
    Write-Host "VST3 installed: $dst"
    exit 0
} catch {
    Write-Host 'Plain copy failed (likely needs admin) - requesting elevation...'
}

# --- one-time elevated run: grant ACL so future copies are silent ------------
$grant = "$($env:USERNAME):(OI)(CI)M"
$cmd = "New-Item -ItemType Directory -Force -Path '$dstDir' | Out-Null; " +
       "icacls '$dstDir' /grant '$grant' | Out-Null; " +
       "Remove-Item '$dst' -Recurse -Force -ErrorAction SilentlyContinue; " +
       "Copy-Item '$src' '$dst' -Recurse -Force"
try {
    $p = Start-Process -FilePath 'powershell' -Verb RunAs -Wait -PassThru `
         -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-Command',$cmd)
} catch {
    Write-Host 'Admin prompt was declined. Copy manually:'
    Write-Host "  from: $src"
    Write-Host "  to:   $dstDir"
    exit 1
}

if (Test-Path $dst) {
    Write-Host "VST3 installed: $dst"
    Write-Host '(Future installs will not need the admin prompt.)'
    exit 0
}

Write-Host 'Install did not complete. If your DAW is open and using the plugin, close it and try again.'
exit 1
