@echo off
setlocal
cd /d "%~dp0.."
if not exist generated\tests\core-build mkdir generated\tests\core-build
call :BuildAndRun JsonUtilityTests project\engine\utility\JsonUtility.cpp
if errorlevel 1 exit /b 1
call :BuildAndRun FileUtilityWriteTests project\engine\utility\FileUtility.cpp
if errorlevel 1 exit /b 1
call :BuildAndRun PastSelfTutorialStateTests
if errorlevel 1 exit /b 1
call :BuildAndRun PastSelfTutorialLayoutTests
if errorlevel 1 exit /b 1
call :BuildAndRun TraceShiftStageJsonTests project\application\scenes\TraceShiftStageJson.cpp project\engine\utility\JsonUtility.cpp
exit /b %errorlevel%

rem 指定した検証をビルドし、成功した場合だけ実行する。
:BuildAndRun
set "testName=%~1"
cl /nologo /std:c++20 /EHsc /utf-8 /MT /Iproject /Iproject\engine\utility ^
 /Fogenerated\tests\core-build\ /Fegenerated\tests\%testName%.exe ^
 tests\%testName%.cpp %2 %3 /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
generated\tests\%testName%.exe
exit /b %errorlevel%
