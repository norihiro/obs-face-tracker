#! /bin/bash

set -ex

# These archives are extracted from Homebrew bottle.
ff=(
	http://www.nagater.net/obs-studio/2a2cbc788eb7db6dcc07a52d01d4ed353ed6928f42306b4637f2eadb8c59b79c--gcc--12.1.0.arm64_big_sur.bottle.tar.gz
	http://www.nagater.net/obs-studio/db57c4a5cdfbad728748c63dcae651c5f727c93d40e1d6d65eb42af8d7f5865d--openblas--0.3.21.arm64_big_sur.bottle.tar.gz
)

dest='/usr/local/opt/arm64/'
sudo mkdir -p $dest
sudo chown $USER:staff $dest
cd $dest
for f in ${ff[@]}; do
	curl -O $f
	b=$(basename "$f")
	tar --strip-components=2 -xzf $b
done

sha256sum -c <<-EOF
34e57867496112f8a0748db2d06243f7d197a171667326194cca86f7b6fb8fb4  db57c4a5cdfbad728748c63dcae651c5f727c93d40e1d6d65eb42af8d7f5865d--openblas--0.3.21.arm64_big_sur.bottle.tar.gz
eb9373608f61146a507198f50cdb77b340c2229db9c9ec995538fb96e672e840  2a2cbc788eb7db6dcc07a52d01d4ed353ed6928f42306b4637f2eadb8c59b79c--gcc--12.1.0.arm64_big_sur.bottle.tar.gz
EOF

install_name_tool \
	-id $PWD/lib/libopenblas.dylib \
	-change @@HOMEBREW_PREFIX@@/opt/gcc/lib/gcc/current/libgfortran.5.dylib $PWD/lib/gcc/current/libgfortran.5.dylib \
	-change @@HOMEBREW_PREFIX@@/opt/gcc/lib/gcc/current/libgomp.1.dylib $PWD/lib/gcc/current/libgomp.1.dylib \
	-change @@HOMEBREW_PREFIX@@/opt/gcc/lib/gcc/current/libquadmath.0.dylib $PWD/lib/gcc/current/libquadmath.0.dylib \
	lib/libopenblas.dylib

install_name_tool \
	-id $PWD/lib/gcc/current/libgfortran.5.dylib \
	-change @rpath/libquadmath.0.dylib $PWD/lib/gcc/current/libquadmath.0.dylib \
	-change @rpath/libgcc_s.1.1.dylib $PWD/lib/gcc/current/libgcc_s.1.1.dylib \
	lib/gcc/current/libgfortran.5.dylib

install_name_tool \
	-id $PWD/lib/gcc/current/libgomp.1.dylib \
	lib/gcc/current/libgomp.1.dylib

install_name_tool \
	-id $PWD/lib/gcc/current/libquadmath.0.dylib \
	lib/gcc/current/libquadmath.0.dylib

install_name_tool \
	-id $PWD/lib/gcc/current/libgcc_s.1.1.dylib \
	lib/gcc/current/libgcc_s.1.1.dylib

echo "OPENBLAS_HOME=$PWD/" >> $GITHUB_ENV

cd -
cp $dest/LICENSE data/LICENSE-openblas
