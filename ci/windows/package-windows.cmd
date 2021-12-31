call "%~dp0..\ci_includes.generated.cmd"

mkdir package
cd package

git describe --tags --long --always > package-version.txt
set /p PackageVersion=<package-version.txt
del package-version.txt

copy ..\LICENSE          ..\release\data\obs-plugins\%PluginName%\LICENCE-%PluginName%.txt
copy ..\dlib\LICENSE.txt ..\release\data\obs-plugins\%PluginName%\LICENSE-dlib.txt
copy ..\dlib-models\LICENSE ..\release\data\obs-plugins\%PluginName%\LICENSE-dlib-models.txt

REM Package ZIP archive
7z a "%PluginName%-%PackageVersion%-Windows.zip" "..\release\*"

REM Build installer
iscc ..\installer\installer-Windows.generated.iss /O. /F"%PluginName%-%PackageVersion%-Windows-Installer"
