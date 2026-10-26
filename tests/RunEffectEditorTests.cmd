@echo off
setlocal
cd /d "%~dp0.."
if not exist generated\obj\CG2_00\Development\TemporalRiftEffect.obj (
 echo Build Development x64 before running this test.
 exit /b 1
)
if not exist generated\tests\effect-editor-build mkdir generated\tests\effect-editor-build
cl /nologo /std:c++20 /EHsc /utf-8 /MT /DUSE_IMGUI ^
 /Iproject /Iproject\engine\base /Iproject\engine\3d /Iproject\engine\2d ^
 /Iproject\engine\utility /Iproject\externals\assimp\include ^
 /Fogenerated\tests\effect-editor-build\ /Fegenerated\tests\EffectEditorTests.exe ^
 tests\EffectEditorTests.cpp generated\obj\CG2_00\Development\*.obj ^
 generated\outputs\Development\imgui.lib generated\outputs\Development\DirectXTex.lib ^
 project\externals\assimp\lib\Release\assimp-vc143-mt.lib ^
 /link /SUBSYSTEM:CONSOLE /LTCG /INCREMENTAL:NO /ignore:4049 /ignore:4098 /ignore:4099
if errorlevel 1 exit /b 1
set "PATH=%CD%\generated\outputs\Development;%PATH%"
generated\tests\EffectEditorTests.exe
exit /b %errorlevel%
