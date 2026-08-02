# Regenerate the complete PCBWay fab package for both Glitchwave boards.
#
# WHY THIS EXISTS: the fab/ folder was hand-made on 2026-07-25 and there was no
# script, so it silently went stale while the boards kept changing. Ordering from
# a stale package builds the wrong board. Run this after ANY board edit, and check
# the timestamps in fab/ against the .kicad_pcb timestamps before ordering.
#
# Layer set, filenames and options are matched to the original July package:
# 4 copper layers, Protel extensions (.gtl/.gbl/.g1/.g2/...), separate PTH/NPTH
# Excellon in mm, CSV position files.
#
# Usage:  powershell -File make_fab.ps1 [-NoZip]

param([switch]$NoZip)

$ErrorActionPreference = 'Stop'
$cli  = "C:\Users\Jason\AppData\Local\Programs\KiCad\10.0\bin\kicad-cli.exe"
$hw   = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # ...\hardware
$fab  = Join-Path $hw 'fab'

$layers = 'F.Cu,In1.Cu,In2.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,F.Paste,B.Paste,Edge.Cuts'

$boards = @(
  @{ name='main'; pcb=(Join-Path $hw 'kicad\glitchwave567\glitchwave567.kicad_pcb');
     gdir=(Join-Path $fab 'gerbers_main'); pos=(Join-Path $fab 'pos_main.csv');
     zip=(Join-Path $fab 'glitchwave567_main_gerbers.zip') },
  @{ name='ctrl'; pcb=(Join-Path $hw 'kicad\glitchwave567_ctrl\glitchwave567_ctrl.kicad_pcb');
     gdir=(Join-Path $fab 'gerbers_ctrl'); pos=(Join-Path $fab 'pos_ctrl.csv');
     zip=(Join-Path $fab 'glitchwave567_ctrl_gerbers.zip') }
)

foreach ($b in $boards) {
  Write-Host ("=" * 60)
  Write-Host ("BOARD: {0}   src mtime {1}" -f $b.name, (Get-Item $b.pcb).LastWriteTime)

  if (Test-Path $b.gdir) { Remove-Item (Join-Path $b.gdir '*') -Force -ErrorAction SilentlyContinue }
  else { New-Item -ItemType Directory -Force -Path $b.gdir | Out-Null }

  # --check-zones makes kicad-cli refill before plotting, so the gerbers can never
  # be plotted from a stale zone fill.
  & $cli pcb export gerbers --output $b.gdir --layers $layers --check-zones --subtract-soldermask $b.pcb
  if ($LASTEXITCODE -ne 0) { throw "gerber export failed for $($b.name)" }

  & $cli pcb export drill --output $b.gdir --format excellon --excellon-separate-th `
        --excellon-units mm --drill-origin absolute --generate-map --map-format pdf $b.pcb
  if ($LASTEXITCODE -ne 0) { throw "drill export failed for $($b.name)" }

  & $cli pcb export pos --output $b.pos --format csv --units mm --side both $b.pcb
  if ($LASTEXITCODE -ne 0) { throw "pos export failed for $($b.name)" }

  if (-not $NoZip) {
    if (Test-Path $b.zip) { Remove-Item $b.zip -Force }
    Compress-Archive -Path (Join-Path $b.gdir '*') -DestinationPath $b.zip
  }

  $n = (Get-ChildItem $b.gdir -File).Count
  Write-Host ("  -> {0} files in {1}" -f $n, (Split-Path -Leaf $b.gdir))
}

Write-Host ("=" * 60)
Write-Host "fab/ contents:"
Get-ChildItem $fab -Recurse -File |
  Select-Object @{n='file';e={$_.FullName.Replace($fab + '\','')}},
                @{n='KB';e={[int]($_.Length/1024)}}, LastWriteTime |
  Sort-Object file | Format-Table -AutoSize | Out-String -Width 160 | Write-Host
