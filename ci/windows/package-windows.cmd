call "build\ci\ci_includes.generated.cmd"

REM x64 keeps the names it has always had, others say their architecture
set ArchSuffix=
if not "%2"=="" if not "%2"=="x64" set ArchSuffix=-%2

mkdir package
cd package

git describe --tags --always > package-version.txt
set /p PackageVersion=<package-version.txt
del package-version.txt

copy ..\LICENSE          ..\release\data\obs-plugins\%PluginName%\LICENCE-%PluginName%.txt

REM Package ZIP archive
7z a "%PluginName%-%PackageVersion%-obs%1-Windows%ArchSuffix%.zip" "..\release\*"

REM Build installer
iscc ..\build\installer-Windows.generated.iss /O. /F"%PluginName%-%PackageVersion%-obs%1-Windows%ArchSuffix%-Installer"

certutil.exe -hashfile "%PluginName%-%PackageVersion%-obs%1-Windows%ArchSuffix%.zip" SHA1
certutil.exe -hashfile "%PluginName%-%PackageVersion%-obs%1-Windows%ArchSuffix%-Installer.exe" SHA1
