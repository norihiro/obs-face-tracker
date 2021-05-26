#!/bin/sh

OSTYPE=$(uname)

if [ "${OSTYPE}" != "Darwin" ]; then
    echo "[Error] macOS obs-studio build script can be run on Darwin-type OS only."
    exit 1
fi

HAS_CMAKE=$(type cmake 2>/dev/null)
HAS_GIT=$(type git 2>/dev/null)

if [ "${HAS_CMAKE}" = "" ]; then
    echo "[Error] CMake not installed - please run 'install-dependencies-macos.sh' first."
    exit 1
fi

if [ "${HAS_GIT}" = "" ]; then
    echo "[Error] Git not installed - please install Xcode developer tools or via Homebrew."
    exit 1
fi

# Build obs-studio
plugindir=$(pwd)
cd ..
echo "=> Cloning obs-studio from GitHub.."
git clone https://github.com/obsproject/obs-studio
cd obs-studio
if [ -z "$OBSLatestTag" ]; then
	OBSLatestTag=$(git describe --tags --abbrev=0)
fi
git checkout $OBSLatestTag
patch -p1 < $plugindir/ci/macos/obs-studio-build.patch
echo 'add_subdirectory(obs-frontend-api)' > UI/CMakeLists.txt

export TERM=
./CI/full-build-macos.sh
