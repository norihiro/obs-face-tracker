#!/bin/bash

set -e

script_dir=$(dirname "$0")
source "$script_dir/../ci_includes.generated.sh"

export PKG_VERSION="1-$(git describe --tags --long --always)"

cd ./build

OBS_VER="$(dpkg -s obs-studio | awk '$1=="Version:"{print gensub(/^([0-9]*\.[0-9]*)\..*/, "\\1.0", "g", $2)}')"

PAGER="cat" sudo checkinstall -y --type=debian --fstrans=no --nodoc \
	--backup=no --deldoc=yes --install=no \
	--pkgname="$PLUGIN_NAME" --pkgversion="$PKG_VERSION" \
	--pkglicense="GPLv2.0" --maintainer="$LINUX_MAINTAINER_EMAIL" \
	--pkggroup="video" \
	--requires="obs-studio \(\>= ${OBS_VER}\), libqt5core5a, libqt5widgets5, qt5-image-formats-plugins" \
	--pakdir="../package"

sudo chmod ao+r ../package/*
