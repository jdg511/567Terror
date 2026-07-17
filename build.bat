@echo off
rem ============================================================
rem  Glitchwave 567 - one-click Windows build
rem  Needs: Visual Studio 2022 with "Desktop development with C++"
rem  (CMake is included with that workload.)
rem ============================================================
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    rem try the copy of CMake that ships inside Visual Studio
    set "CMAKE_VS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
    if exist "%CMAKE_VS%\cmake.exe" (
        set "PATH=%CMAKE_VS%;%PATH%"
    ) else (
        echo.
        echo ERROR: CMake was not found.
        echo Install Visual Studio 2022 Community with the
        echo "Desktop development with C++" workload, then run this again.
        echo.
        pause
        exit /b 1
    )
)

echo.
echo [1/2] Configuring (first run downloads JUCE, ~2 min)...
cmake -B build
if errorlevel 1 goto fail

echo.
echo [2/2] Building Release (this takes a few minutes the first time)...
cmake --build build --config Release
if errorlevel 1 goto fail

echo.
echo Installing VST3 into the system VST3 folder...
set "VST3_SRC=%~dp0build\Glitchwave567_artefacts\Release\VST3\Glitchwave 567.vst3"
set "VST3_DST=%CommonProgramFiles%\VST3\Glitchwave 567.vst3"
"%SystemRoot%\System32\Robocopy.exe" "%VST3_SRC%" "%VST3_DST%" /MIR /NFL /NDL /NJH /NJS >nul 2>nul
if exist "%VST3_DST%\Contents" (
    echo   Installed: "%VST3_DST%"
) else (
    echo   Plain copy failed - trying with admin rights, a Windows prompt may appear...
    "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\install_vst3.ps1"
)

echo.
echo ============================================================
echo  BUILD SUCCEEDED
echo.
echo  Standalone app:
echo    build\Glitchwave567_artefacts\Release\Standalone\Glitchwave 567.exe
echo.
echo  VST3 plugin (also auto-installed for your DAW):
echo    C:\Program Files\Common Files\VST3\Glitchwave 567.vst3
echo ============================================================
echo.
pause
exit /b 0

:fail
echo.
echo BUILD FAILED - scroll up for the first line that says "error".
echo.
pause
exit /b 1
