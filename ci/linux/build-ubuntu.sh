#!/bin/sh
set -ex

git clone https://github.com/davisking/dlib.git
cd dlib
git checkout $(git describe --tags --abbrev=0 --exclude="*-rc*")
patch -p1 < ../ci/common/dlib-slim.patch
patch -p1 < ../ci/common/dlib-cmake-no-openblasp.patch
cd ..

git clone --depth 1 https://github.com/davisking/dlib-models
bunzip2 < dlib-models/shape_predictor_5_face_landmarks.dat.bz2 > data/shape_predictor_5_face_landmarks.dat
cp dlib-models/LICENSE data/LICENSE-dlib-models

cp LICENSE data/LICENSE-$PLUGIN_NAME
cp dlib/LICENSE.txt data/LICENSE-dlib

sed -i 's;${CMAKE_INSTALL_FULL_LIBDIR};/usr/lib;' CMakeLists.txt

mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
