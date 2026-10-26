@echo off
setlocal
cd /d "%~dp0.."
if not exist generated\tests\scene-availability-build mkdir generated\tests\scene-availability-build
cl /nologo /std:c++20 /EHsc /utf-8 /MT /Iproject /DNDEBUG ^
 /Fogenerated\tests\scene-availability-build\Release.obj /Fegenerated\tests\SceneAvailabilityReleaseTests.exe ^
 tests\SceneAvailabilityTests.cpp
if errorlevel 1 exit /b 1
generated\tests\SceneAvailabilityReleaseTests.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /utf-8 /MT /Iproject /DNDEBUG /DUSE_IMGUI ^
 /Fogenerated\tests\scene-availability-build\Development.obj /Fegenerated\tests\SceneAvailabilityDevelopmentTests.exe ^
 tests\SceneAvailabilityTests.cpp
if errorlevel 1 exit /b 1
generated\tests\SceneAvailabilityDevelopmentTests.exe
exit /b %errorlevel%
