#!/bin/sh
set -ex

git clone https://github.com/davisking/dlib.git
cd dlib
git checkout `git describe --tags --abbrev=0 --exclude="*-rc*"`
patch -p1 < ../ci/common/dlib-slim.patch
cd ..

mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
