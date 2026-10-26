@echo off
setlocal enabledelayedexpansion

rem Builds the plugin with -DENABLE_TESTS=ON for the given architecture and
rem runs tests\integration against it, in an OBS kept for the tests. See the
rem Windows section of tests\integration\README.md.
rem
rem Usage: scripts\windows-build-and-test.bat <arm64|x64> [pytest arguments]
rem
rem OBS's development files are those the CI workflow builds against, which
rem are fetched into .deps\windows-<arch> the first time. Needs Visual Studio
rem 2022 with the C++ tools for the architecture, CMake, Git and Python.
rem
rem Env overrides:
rem   FACE_TRACKER_OBS_BINARY  the obs64.exe of the OBS for the tests (default:
rem                            C:\FT-Test\<arch>\bin\64bit\obs64.exe)
rem   FACE_TRACKER_DATA        the detector's model (default: fetched into a
rem                            cache by tests\integration\fetch_model.py)
rem   VENV                     the Python environment to make and use

set ARCH=%1
if not "%ARCH%"=="arm64" if not "%ARCH%"=="x64" (
	echo Usage: %~n0 ^<arm64^|x64^> [pytest arguments]
	exit /b 1
)

set ROOT=%~dp0..
for %%i in ("%ROOT%") do set ROOT=%%~fi
if not exist "%ROOT%\dlib\CMakeLists.txt" (
	echo error: the submodules are not checked out: git submodule update --init --recursive
	exit /b 1
)

if not defined FACE_TRACKER_OBS_BINARY set FACE_TRACKER_OBS_BINARY=C:\FT-Test\%ARCH%\bin\64bit\obs64.exe
if not exist "%FACE_TRACKER_OBS_BINARY%" (
	echo error: no OBS for the tests at %FACE_TRACKER_OBS_BINARY%: unpack an OBS release's zip for
	echo %ARCH% in a folder of its own, kept for the tests, or set FACE_TRACKER_OBS_BINARY
	exit /b 1
)
tasklist /FI "IMAGENAME eq obs64.exe" | findstr /i obs64.exe > nul
if not errorlevel 1 (
	echo error: OBS is already running -- quit it first
	exit /b 1
)

rem As the CI workflow's obs-studio-devel-action fetches them for OBS 32
if "%ARCH%"=="x64" (
	set KIT_URL=https://github.com/norihiro/obs-plugintemplate/releases/download/32.0.1-dev1/obs-plugintemplate-5b9629713-windows-x64.tar.gz
	set KIT_SHA256=65c7769ff5c466cf0d5af4e134669489f21d882c1880ed7b1df6e828e2dbb679
	set PLATFORM=x64
) else (
	set KIT_URL=https://github.com/norihiro/obs-plugintemplate/releases/download/32.0.1-dev2/obs-plugintemplate-1116ffc69-windows-arm64.tar.gz
	set KIT_SHA256=86c2cd2a07542a8e830f2a20154375658a56b68f67326d3f2bc916131b24e7a0
	set PLATFORM=ARM64
)
set KIT=%ROOT%\.deps\windows-%ARCH%
set DEPS=%KIT%\obs-build-dependencies\plugin-deps-%ARCH%
if not exist "%DEPS%" (
	echo === fetching OBS's development files for %ARCH% ===
	if not exist "%KIT%" mkdir "%KIT%"
	curl -fsSL -o "%KIT%\kit.tar.gz" "!KIT_URL!"
	if errorlevel 1 exit /b 1
	for /f %%h in ('powershell -NoProfile -Command "(Get-FileHash -Algorithm SHA256 '%KIT%\kit.tar.gz').Hash"') do set GOT=%%h
	if /i not "!GOT!"=="!KIT_SHA256!" (
		echo error: !KIT_URL! is not the file it was
		exit /b 1
	)
	tar xzf "%KIT%\kit.tar.gz" -C "%KIT%"
	if errorlevel 1 exit /b 1
	del "%KIT%\kit.tar.gz"
)

set BUILD=%ROOT%\build_%ARCH%
echo === building for %ARCH% ===
cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A %PLATFORM% -DCMAKE_SYSTEM_VERSION=10.0.18363.657 ^
	-DQT_VERSION=6 -DENABLE_TESTS=ON "-DCMAKE_PREFIX_PATH=%DEPS%" "-DCMAKE_INSTALL_PREFIX=%DEPS%"
if errorlevel 1 exit /b 1
cmake --build "%BUILD%" --config RelWithDebInfo -- /m /v:m
if errorlevel 1 exit /b 1

if not defined VENV set VENV=%LOCALAPPDATA%\obs-face-tracker\integration-venv
if not exist "%VENV%\Scripts\python.exe" (
	echo === making the Python environment in %VENV% ===
	python -m venv "%VENV%"
	if errorlevel 1 exit /b 1
)
"%VENV%\Scripts\python.exe" -c "import pytest, websockets, PIL" 2> nul
if errorlevel 1 (
	"%VENV%\Scripts\python.exe" -m pip install -q -r "%ROOT%\tests\integration\requirements.txt"
	if errorlevel 1 exit /b 1
)

if not defined FACE_TRACKER_DATA (
	for /f "delims=" %%d in ('call "%VENV%\Scripts\python.exe" "%ROOT%\tests\integration\fetch_model.py"') do set FACE_TRACKER_DATA=%%d
	if not defined FACE_TRACKER_DATA exit /b 1
)

set FACE_TRACKER_PLUGIN=%BUILD%\RelWithDebInfo\obs-face-tracker.dll
set FACE_TRACKER_TEST_SOURCES=%BUILD%\tests\test-sources\RelWithDebInfo\face-tracker-test-sources.dll
rem What OBS and the harness write is UTF-8, which Python reads as the code
rem page of the system unless it is told
set PYTHONUTF8=1

rem The arguments after the architecture. No labels to loop with: cmd can
rem lose them in a file of LF line endings
set PYTEST_ARGS=
for /f "tokens=1,*" %%a in ("%*") do set PYTEST_ARGS=%%b

echo === running tests\integration against %ARCH% ===
cd /d "%ROOT%\tests\integration"
"%VENV%\Scripts\python.exe" -m pytest -p no:cacheprovider !PYTEST_ARGS!
exit /b %ERRORLEVEL%
