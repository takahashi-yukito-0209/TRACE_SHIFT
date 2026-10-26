@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."
if not exist generated\obj\CG2_00\Development\TitleScene.obj (
 echo Build Development x64 before running this test.
 exit /b 1
)
if not exist generated\tests\title-demo-build mkdir generated\tests\title-demo-build
rem TitleScene.cpp is included in the test translation unit to test its private helpers.
set "gameObjects="
for %%F in (generated\obj\CG2_00\Development\*.obj) do (
 if /I not "%%~nxF"=="TitleScene.obj" set "gameObjects=!gameObjects! %%F"
)
cl /nologo /std:c++20 /EHsc /utf-8 /MT /DUSE_IMGUI ^
 /Iproject /Iproject\engine\base /Iproject\engine\3d /Iproject\engine\2d ^
 /Iproject\engine\utility /Iproject\externals\assimp\include ^
 /Fogenerated\tests\title-demo-build\ /Fegenerated\tests\TitleDemoTimelineTests.exe ^
 tests\TitleDemoTimelineTests.cpp !gameObjects! ^
 generated\outputs\Development\imgui.lib generated\outputs\Development\DirectXTex.lib ^
 project\externals\assimp\lib\Release\assimp-vc143-mt.lib ^
 /link /SUBSYSTEM:CONSOLE /LTCG /INCREMENTAL:NO /ignore:4049 /ignore:4098 /ignore:4099
if errorlevel 1 exit /b 1
set "PATH=%CD%\generated\outputs\Development;%PATH%"
generated\tests\TitleDemoTimelineTests.exe
exit /b %errorlevel%
