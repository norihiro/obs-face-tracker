#! /bin/bash
set -ex
.github/containers/fedora-common/build.sh obs-plugin-build/fedora%releasever% fedora%releasever%-rpmbuild
echo 'FILE_NAME=fedora%releasever%-rpmbuild/*RPMS/**/*.rpm' >> $GITHUB_ENV
