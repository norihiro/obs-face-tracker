call "build\ci\ci_includes.generated.cmd"

mkdir package
cd package

git describe --tags --always > package-version.txt
set /p PackageVersion=<package-version.txt
del package-version.txt

REM Package ZIP archive
7z a "%PluginName%-%PackageVersion%-obs%1-Windows-%2.zip" "..\release\*"

REM Build installer
iscc ..\build\installer-Windows.generated.iss /O. /F"%PluginName%-%PackageVersion%-obs%1-Windows-%2-Installer"
