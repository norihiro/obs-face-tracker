
if not exist dlib-models (
	echo checkout dlib-models...
	git clone --depth 1 https://github.com/davisking/dlib-models
)
7z x dlib-models/shape_predictor_5_face_landmarks.dat.bz2 -so > data/shape_predictor_5_face_landmarks.dat

if "%buildWin32%" == "false" goto skippedWin32
mkdir build32
cd build32
cmake -G "Visual Studio 16 2019" ^
-A Win32 -DCMAKE_SYSTEM_VERSION=10.0 -DQTDIR="%QTDIR32%" ^
-DLibObs_DIR="%OBSPath%\build32\libobs" ^
-DLIBOBS_INCLUDE_DIR="%OBSPath%\libobs" ^
-DLIBOBS_LIB="%OBSPath%\build32\libobs\%build_config%\obs.lib" ^
-DPTHREAD_LIBS="%OBSPath%\build32\deps\w32-pthreads\%build_config%\w32-pthreads.lib" ^
-DOBS_FRONTEND_LIB="%OBSPath%\build32\UI\obs-frontend-api\%build_config%\obs-frontend-api.lib" ^
..
cd ..
:skippedWin32

mkdir build64
cd build64
cmake -G "Visual Studio 16 2019" ^
-A x64 -DCMAKE_SYSTEM_VERSION=10.0 ^
-DQTDIR="%QTDIR64%" -DLibObs_DIR="%OBSPath%\build64\libobs" ^
-DLIBOBS_INCLUDE_DIR="%OBSPath%\libobs" ^
-DLIBOBS_LIB="%OBSPath%\build64\libobs\%build_config%\obs.lib" ^
-DPTHREAD_LIBS="%OBSPath%\build64\deps\w32-pthreads\%build_config%\w32-pthreads.lib" ^
-DOBS_FRONTEND_LIB="%OBSPath%\build64\UI\obs-frontend-api\%build_config%\obs-frontend-api.lib" ^
..

REM Import the generated includes to get the plugin's name
call "%~dp0..\ci_includes.generated.cmd"

REM Rename the solution files to something CI can pick up 
cd ..
ren "build32\%PluginName%.sln" "main.sln"
ren "build64\%PluginName%.sln" "main.sln"
