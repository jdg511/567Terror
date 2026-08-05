# Apply the J1/L100 collision fix to the main board, end to end.
#
#   1. back up the board (hash-verified)
#   2. move J1 +3.0mm x and R1 +3.1mm x, ripping the traces that detach
#   3. reroute IN_TIP and /input_dirt/IN_A with microroute.py
#   4. refill zones + run DRC
#
# Run with -DryRun first to see the moves and the traces that would be ripped
# without writing anything.
#
#   powershell -ExecutionPolicy Bypass -File run_fix_j1_l100.ps1
#   powershell -ExecutionPolicy Bypass -File run_fix_j1_l100.ps1 -DryRun
#
# Rollback at any point:
#   Copy-Item <pcb>.bak_j1fix <pcb> -Force

param([switch]$DryRun)

$ErrorActionPreference = 'Stop'

$PY   = 'C:\Users\Jason\AppData\Local\Programs\KiCad\10.0\bin\python.exe'
$TOOL = 'C:\Users\Jason\source\repos\Glitchwave\hardware\kicad\tools'
$PCB  = 'C:\Users\Jason\source\repos\Glitchwave\hardware\kicad\glitchwave567\glitchwave567.kicad_pcb'
$BAK  = $PCB + '.bak_j1fix'

if (-not (Test-Path $PY))  { throw "KiCad python not found: $PY" }
if (-not (Test-Path $PCB)) { throw "board not found: $PCB" }

Set-Location $TOOL

if ($DryRun) {
    Write-Host "=== DRY RUN ===" -ForegroundColor Cyan
    & $PY (Join-Path $TOOL 'fix_j1_l100.py') $PCB --dry-run
    exit $LASTEXITCODE
}

# --- 1. backup -------------------------------------------------------------
Copy-Item $PCB $BAK -Force
$hb = (Get-FileHash $BAK -Algorithm SHA256).Hash
$hp = (Get-FileHash $PCB -Algorithm SHA256).Hash
if ($hb -ne $hp) { throw "backup hash mismatch -- aborting" }
Write-Host "[1/4] backup OK -> $BAK" -ForegroundColor Green

# --- 2. move ---------------------------------------------------------------
Write-Host "[2/4] moving J1 and R1 ..." -ForegroundColor Cyan
$moveOut = & $PY (Join-Path $TOOL 'fix_j1_l100.py') $PCB 2>&1
$moveOut | ForEach-Object { Write-Host "    $_" }
if ($LASTEXITCODE -ne 0) { throw "move failed -- board restored"; Copy-Item $BAK $PCB -Force }

$netsLine = $moveOut | Where-Object { $_ -match '^NETS=' } | Select-Object -Last 1
$nets = @()
if ($netsLine) { $nets = ($netsLine -replace '^NETS=','').Split(',') | Where-Object { $_ } }

# --- 3. reroute ------------------------------------------------------------
Write-Host "[3/4] rerouting $($nets.Count) net(s) ..." -ForegroundColor Cyan
foreach ($n in $nets) {
    Write-Host "    microroute: $n"
    & $PY (Join-Path $TOOL 'microroute.py') $PCB $n 2>&1 | ForEach-Object { Write-Host "        $_" }
}

# --- 4. zones + DRC --------------------------------------------------------
Write-Host "[4/4] refilling zones + DRC ..." -ForegroundColor Cyan
& $PY (Join-Path $TOOL 'refill_and_drc.py') $PCB 2>&1 | ForEach-Object { Write-Host "    $_" }

Write-Host ""
Write-Host "DONE. Open the board in KiCAD and eyeball the IN_TIP route -- it is" -ForegroundColor Yellow
Write-Host "the guitar input, the most noise-sensitive net on the board." -ForegroundColor Yellow
Write-Host "Rollback: Copy-Item '$BAK' '$PCB' -Force"
