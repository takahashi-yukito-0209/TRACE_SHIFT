@echo off
setlocal
cd /d "%~dp0.."
if not exist generated\obj\CG2_00\Development\StageGimmicks.obj (
 echo Build Development x64 before running this test.
 exit /b 1
)
if not exist generated\tests\stage-switch-build mkdir generated\tests\stage-switch-build
cl /nologo /std:c++20 /EHsc /utf-8 /MT /Iproject ^
 /Fogenerated\tests\stage-switch-build\ /Fegenerated\tests\PastSelfTutorialStateTests.exe ^
 tests\PastSelfTutorialStateTests.cpp
if errorlevel 1 exit /b 1
generated\tests\PastSelfTutorialStateTests.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /utf-8 /MT /DUSE_IMGUI ^
 /Iproject /Iproject\engine\base /Iproject\engine\3d /Iproject\engine\2d ^
 /Iproject\engine\utility /Iproject\externals\assimp\include ^
 /Fogenerated\tests\stage-switch-build\ /Fegenerated\tests\StageGimmickSwitchTests.exe ^
 tests\StageGimmickSwitchTests.cpp generated\obj\CG2_00\Development\*.obj ^
 generated\outputs\Development\imgui.lib generated\outputs\Development\DirectXTex.lib ^
 project\externals\assimp\lib\Release\assimp-vc143-mt.lib ^
 /link /SUBSYSTEM:CONSOLE /LTCG /INCREMENTAL:NO /ignore:4049 /ignore:4098 /ignore:4099
if errorlevel 1 exit /b 1
set "PATH=%CD%\generated\outputs\Development;%PATH%"
generated\tests\StageGimmickSwitchTests.exe
exit /b %errorlevel%
