call "build\ci\ci_includes.generated.cmd"

mkdir package
cd package

git describe --tags --always > package-version.txt
set /p PackageVersion=<package-version.txt
del package-version.txt

copy ..\LICENSE          ..\release\data\obs-plugins\%PluginName%\LICENCE-%PluginName%.txt

REM Package ZIP archive
7z a "%PluginName%-%PackageVersion%-obs%1-Windows-%2.zip" "..\release\*"

REM Build installer
iscc ..\build\installer-Windows.generated.iss /O. /F"%PluginName%-%PackageVersion%-obs%1-Windows-%2-Installer"

certutil.exe -hashfile "%PluginName%-%PackageVersion%-obs%1-Windows-%2.zip" SHA1
certutil.exe -hashfile "%PluginName%-%PackageVersion%-obs%1-Windows-%2-Installer.exe" SHA1
