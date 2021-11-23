#!/bin/sh
set -ex

OSTYPE=$(uname)

if [ "${OSTYPE}" != "Darwin" ]; then
    echo "[Error] macOS build script can be run on Darwin-type OS only."
    exit 1
fi

HAS_CMAKE=$(type cmake 2>/dev/null)

if [ "${HAS_CMAKE}" = "" ]; then
    echo "[Error] CMake not installed - please run 'install-dependencies-macos.sh' first."
    exit 1
fi

echo "=> Cloning dlib..."
git clone https://github.com/davisking/dlib.git
cd dlib
git checkout $(git describe --tags --abbrev=0 --exclude="*-rc*")
patch -p1 < ../ci/common/dlib-slim.patch
patch -p1 < ../ci/common/dlib-cmake-no-openblasp.patch
cd ..

#export QT_PREFIX="$(find /usr/local/Cellar/qt5 -d 1 | tail -n 1)"

export OPENBLAS_HOME=/usr/local/opt/openblas/
echo "=> Building plugin for macOS."
mkdir -p build && cd build
cmake .. \
	-DQTDIR="/tmp/obsdeps" \
	-DLIBOBS_INCLUDE_DIR=../../obs-studio/libobs \
	-DLIBOBS_LIB=../../obs-studio/libobs \
	-DOBS_FRONTEND_LIB="$(pwd)/../../obs-studio/build/UI/obs-frontend-api/libobs-frontend-api.dylib" \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_INSTALL_PREFIX=/usr \
&& make -j4
