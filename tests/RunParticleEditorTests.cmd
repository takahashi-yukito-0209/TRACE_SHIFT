@echo off
setlocal
cd /d "%~dp0.."
if not exist generated\tests\particle-editor-build mkdir generated\tests\particle-editor-build
cl /nologo /std:c++20 /EHsc /utf-8 /Gy /DUSE_IMGUI /DIMGUI_ENABLE_TEST_ENGINE ^
 /Iproject /Iproject\engine\base /Iproject\engine\3d /Iproject\engine\2d ^
 /Iproject\engine\utility /Iproject\engine\particle /Iproject\externals\assimp\include ^
 /Fogenerated\tests\particle-editor-build\ /Fegenerated\tests\ParticleEditorTests.exe ^
 tests\ParticleEditorTests.cpp ^
 project\engine\particle\ParticleManagerImGui.cpp ^
 project\engine\particle\ParticleEmitter.cpp ^
 project\engine\particle\ParticleManagerGpuEmitterSettings.cpp ^
 project\engine\particle\GpuEmitterSettingsUtility.cpp ^
 project\engine\utility\FileUtility.cpp project\engine\utility\Logger.cpp ^
 project\engine\utility\JsonUtility.cpp project\engine\utility\JsonFileLoader.cpp ^
 project\engine\utility\ResourceResolver.cpp project\engine\utility\StringUtility.cpp ^
 project\externals\imgui\imgui.cpp project\externals\imgui\imgui_draw.cpp ^
 project\externals\imgui\imgui_tables.cpp project\externals\imgui\imgui_widgets.cpp ^
 /link /OPT:REF /INCREMENTAL:NO
if errorlevel 1 exit /b 1
generated\tests\ParticleEditorTests.exe
exit /b %errorlevel%
