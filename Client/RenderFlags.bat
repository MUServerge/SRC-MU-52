@echo off
REM ============================================================
REM  MU 5.2 - renderer marker files
REM
REM  The client is re-spawned by its protection wrapper, which drops
REM  environment variables AND the command line. Marker files are the only
REM  switches that survive, so every renderer gate is a file in this folder.
REM
REM  Toggle one, then launch the game normally.
REM ============================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

:menu
cls
echo.
echo   MU 5.2 renderer flags        (folder: %CD%)
echo   ---------------------------------------------------------------
call :show 1 "vbo.disable"           "VBO model path OFF (legacy reference)"
call :show 2 "vbotranslate.on"       "Translated char/equip meshes on the VBO path  (BIG FPS)"
call :show 3 "gl33char.disable"      "Core character path OFF (character_core)"
call :show 4 "gl33effect.enable"     "Core effect path ON (effects/particles)"
call :show 5 "gl33terrain.disable"   "Core terrain path OFF"
call :show 6 "renderprofiler.enable" "Render profiler writes Client\RenderProfiler.log"
echo   ---------------------------------------------------------------
echo.
echo    [1-6] toggle      [F] fast preset      [R] legacy reference
echo    [P] profiler run  [S] show log tail    [0] exit
echo.
set "key="
set /p key=^>
if /i "%key%"=="0" goto :eof
if /i "%key%"=="1" call :toggle "vbo.disable"           & goto menu
if /i "%key%"=="2" call :toggle "vbotranslate.on"       & goto menu
if /i "%key%"=="3" call :toggle "gl33char.disable"      & goto menu
if /i "%key%"=="4" call :toggle "gl33effect.enable"     & goto menu
if /i "%key%"=="5" call :toggle "gl33terrain.disable"   & goto menu
if /i "%key%"=="6" call :toggle "renderprofiler.enable" & goto menu
if /i "%key%"=="F" call :preset_fast      & goto menu
if /i "%key%"=="R" call :preset_reference & goto menu
if /i "%key%"=="P" call :preset_profile   & goto menu
if /i "%key%"=="S" call :tail             & goto menu
goto menu

:show
if exist %~2 (set "st=[ON ]") else (set "st=[off]")
echo    %~1  !st!  %~2
echo           %~3
goto :eof

:toggle
if exist %~1 (
  del %~1
) else (
  type nul > %~1
)
goto :eof

:on
if not exist %~1 type nul > %~1
goto :eof

:off
if exist %~1 del %~1
goto :eof

:preset_fast
REM Fastest verified config: legacy character arrays + translated VBO meshes.
call :off "vbo.disable"
call :on  "vbotranslate.on"
call :on  "gl33char.disable"
call :off "gl33effect.enable"
call :off "gl33terrain.disable"
call :off "renderprofiler.enable"
echo.
echo    Fast preset set. Launch the game normally.
pause >nul
goto :eof

:preset_reference
REM Pure legacy CPU path - the image to compare the shader output against.
call :on  "vbo.disable"
call :off "vbotranslate.on"
call :on  "gl33char.disable"
call :off "gl33effect.enable"
call :off "renderprofiler.enable"
echo.
echo    Legacy reference set (slow on purpose). Launch the game normally.
pause >nul
goto :eof

:preset_profile
REM Fast preset plus the profiler, for before/after measurements.
call :preset_fast >nul
call :on "renderprofiler.enable"
echo.
echo    Profiling run armed. Play ~30 s in the SAME crowded spot, then exit.
echo    Report is written to Client\RenderProfiler.log (readable while running).
pause >nul
goto :eof

:tail
echo.
if not exist "RenderProfiler.log" (
  echo    No RenderProfiler.log yet - enable the profiler and play for ~5 s.
) else (
  powershell -NoProfile -Command "Get-Content 'RenderProfiler.log' | Select-String -Pattern '^\[RenderProfiler\]' | Select-Object -Last 8"
)
echo.
pause >nul
goto :eof
