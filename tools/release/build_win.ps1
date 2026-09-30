$ErrorActionPreference = 'Stop'
$repo = 'C:\Users\Jason\source\repos\Glitchwave'
Set-Location $repo
$cmake = 'C:\Program Files\Python314\Lib\site-packages\cmake\data\bin\cmake.exe'
$dist = Join-Path $repo 'dist_v0.60'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$log = Join-Path $dist 'build_windows.log'
Get-Process | Where-Object { $_.ProcessName -like 'Where The Fuzz*' } | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1
& $cmake -B build *>&1 | Tee-Object -FilePath $log
if ($LASTEXITCODE -ne 0) { throw 'configure failed' }
& $cmake --build build --config Release --parallel *>&1 | Tee-Object -FilePath $log -Append
if ($LASTEXITCODE -ne 0) { throw 'build failed' }
$rel = Join-Path $repo 'build\Wtf567_artefacts\Release'
# install VST3 and LV2 for local testing
robocopy (Join-Path $rel 'VST3\Where The Fuzz Meets The Funk.vst3') 'C:\Program Files\Common Files\VST3\Where The Fuzz Meets The Funk.vst3' /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS | Out-Null
$lv2 = Get-ChildItem (Join-Path $rel 'LV2') -Directory -ErrorAction SilentlyContinue
foreach ($d in $lv2) { robocopy $d.FullName (Join-Path $env:APPDATA ('LV2\' + $d.Name)) /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS | Out-Null }
# package
$stage = Join-Path $env:TEMP 'wtf_pkg_win'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item (Join-Path $rel '*') $stage -Recurse
Get-ChildItem $stage -Recurse -Include *.lib,*.exp,*.a | Remove-Item -Force
$zip = Join-Path $dist 'wtf-windows-x64.zip'
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
'WINDOWS_BUILD_DONE' | Tee-Object -FilePath $log -Append
Get-Item $zip | Select-Object Name, Length | Out-String | Tee-Object -FilePath $log -Append
Get-ChildItem $stage | Select-Object Name | Out-String | Tee-Object -FilePath $log -Append
$exe = Get-ChildItem (Join-Path $rel 'Standalone') -Filter *.exe | Select-Object -First 1
Start-Process $exe.FullName
